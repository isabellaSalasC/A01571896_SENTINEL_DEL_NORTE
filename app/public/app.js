/* SENTINEL Digital Twin - front-end.
 *
 * Architecture (see README):  UI  --JSON-->  server.js  --stdin-->  build/sentinel (C++)
 * This file never implements an algorithm. It builds requests from the active
 * scenario, sends them to /api/run and draws whatever the C++ core returns.
 */
'use strict';
(() => {
  // ------------------------------------------------------------------ utils
  const W = 860, H = 680;                    // SVG viewBox of the map
  const $ = (sel, root = document) => root.querySelector(sel);
  const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  const num = (x, d = 1) => Number(x).toLocaleString('es-MX', { maximumFractionDigits: d });
  const clamp = (x, lo, hi) => Math.min(hi, Math.max(lo, x));
  const fmtTime = (us) => (us < 1000 ? `${us} µs` : us < 1e6 ? `${num(us / 1000, 2)} ms` : `${num(us / 1e6, 2)} s`);
  const badge = (t, kind = '') => `<span class="badge ${kind}">${esc(t)}</span>`;
  const kicker = (t) => `<div class="kicker">${esc(t)}</div>`;

  const TABS = {
    mst: { mode: 'mst', metric: 'COSTO TOTAL DEL ÁRBOL', methods: [['kruskal', 'Kruskal'], ['prim', 'Prim']], compare: ['kruskal', 'prim'] },
    tsp: { mode: 'tsp', metric: 'DISTANCIA TOTAL DE LA RUTA', methods: [['auto', 'Automático'], ['heldkarp', 'Held-Karp (exacto)'], ['heuristic', 'NN + 2-opt (aprox.)']], compare: ['heldkarp', 'heuristic'] },
    flow: { mode: 'maxflow', metric: 'FLUJO MÁXIMO', methods: [['dinic', 'Dinic'], ['edmonds_karp', 'Edmonds-Karp']], compare: ['dinic', 'edmonds_karp'] },
    voronoi: { mode: 'voronoi', metric: 'COBERTURA DEL DOMINIO', methods: [], compare: null },
  };

  const state = {
    scenarios: [], scen: null, proj: null,
    down: new Set(),                          // indices of failed links ("blackout")
    tab: 'mst',
    res: {}, cmp: {}, pending: {}, tokens: {},
    opts: { mst: { method: 'kruskal' }, tsp: { method: 'auto', start: null }, flow: { method: 'dinic', source: null, sink: null, demand: '' }, voronoi: {} },
    queries: [],                              // Voronoi query points in plane coordinates
    view: { x: 0, y: 0, w: W, h: H },
    cyber: null, cyberCmp: null, seedCounter: 2038,
  };

  // ------------------------------------------------------------- core access
  async function core(mode, input) {
    try {
      const r = await fetch('/api/run', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ mode, input }) });
      return await r.json();
    } catch (e) {
      return { ok: false, error: `No se pudo contactar al servidor: ${e.message}` };
    }
  }
  const metaLine = (resp) => {
    if (!resp || !resp.meta) return '';
    const m = resp.meta;
    return `C++ · algoritmo ${fmtTime(m.elapsed_us)}${m.peak_kb ? ` · pico ${num(m.peak_kb / 1024, 1)} MB` : ''} · ida y vuelta ${num(resp.wall_ms, 1)} ms`;
  };

  // ---------------------------------------------------------------- scenarios
  const haversine = (a, b) => {
    const r = Math.PI / 180, dLat = (b.lat - a.lat) * r, dLon = (b.lon - a.lon) * r;
    const h = Math.sin(dLat / 2) ** 2 + Math.cos(a.lat * r) * Math.cos(b.lat * r) * Math.sin(dLon / 2) ** 2;
    return 2 * 6371.0088 * Math.asin(Math.sqrt(h));
  };

  /** Validates a scenario and fills in sensible defaults (also used for uploaded files). */
  function normalizeScenario(raw) {
    const sc = JSON.parse(JSON.stringify(raw));
    if (!Array.isArray(sc.nodes) || !sc.nodes.length) throw new Error('El escenario necesita una lista "nodes".');
    sc.id = sc.id || 'custom';
    sc.name = sc.name || 'Escenario personalizado';
    sc.units = Object.assign({ cost: 'km', cap: 'Gbps' }, sc.units || {});
    sc.nodes.forEach((n) => {
      if (n.id == null) throw new Error('Todos los nodos necesitan "id".');
      n.id = String(n.id);
      n.name = n.name || n.id;
      if (typeof n.lon !== 'number' || typeof n.lat !== 'number') throw new Error(`El nodo ${n.id} necesita "lon" y "lat" numéricos.`);
    });
    const by = Object.fromEntries(sc.nodes.map((n) => [n.id, n]));
    sc.edges = (sc.edges || []).map((e) => {
      const u = String(e.u), v = String(e.v);
      if (!by[u] || !by[v]) throw new Error(`Enlace ${u}–${v}: nodo desconocido.`);
      return { u, v, cost: typeof e.cost === 'number' ? e.cost : Math.round(haversine(by[u], by[v]) * 10) / 10, capacity: typeof e.capacity === 'number' ? e.capacity : 10 };
    });
    const lons = sc.nodes.map((n) => n.lon), lats = sc.nodes.map((n) => n.lat);
    const [a, b, c, d] = [Math.min(...lons), Math.min(...lats), Math.max(...lons), Math.max(...lats)];
    const padX = Math.max((c - a) * 0.12, 0.5), padY = Math.max((d - b) * 0.12, 0.5);
    sc.voronoi = sc.voronoi || {};
    sc.voronoi.domain = sc.voronoi.domain || [a - padX, b - padY, c + padX, d + padY];
    sc.voronoi.k = sc.voronoi.k || Math.cos((((sc.voronoi.domain[1] + sc.voronoi.domain[3]) / 2) * Math.PI) / 180);
    sc.tsp = sc.tsp || { start: sc.nodes[0].id };
    sc.flow = sc.flow || { source: sc.nodes[0].id, sink: sc.nodes[sc.nodes.length - 1].id };
    sc.transmissions = sc.transmissions || { t1: 'AABCDCBA12FFABBA', t2: '00BCDCBAFFABBA77', signatures: ['ABBA', 'FF', 'DEAD'] };
    return sc;
  }

  /** Equirectangular projection on the isotropic plane x = lon * k, y = lat. */
  function buildProjection(sc) {
    const k = sc.voronoi.k, [x0, y0, x1, y1] = sc.voronoi.domain;
    const dx = (x1 - x0) * k, dy = y1 - y0, pad = 26;
    const s = Math.min((W - 2 * pad) / dx, (H - 2 * pad) / dy);
    const ox = (W - dx * s) / 2, oy = (H - dy * s) / 2;
    const toSvg = (px, py) => [ox + (px - x0 * k) * s, H - (oy + (py - y0) * s)];
    const fromSvg = (sx, sy) => [(sx - ox) / s + x0 * k, (H - sy - oy) / s + y0];
    return { k, s, toSvg, fromSvg, geo: (lon, lat) => toSvg(lon * k, lat), domain: [x0, y0, x1, y1] };
  }

  function loadScenario(raw) {
    const sc = normalizeScenario(raw);
    state.scen = sc;
    state.proj = buildProjection(sc);
    state.down.clear();
    state.res = {}; state.cmp = {}; state.queries = [];
    state.view = { x: 0, y: 0, w: W, h: H };
    state.opts.tsp.start = sc.tsp.start;
    state.opts.flow.source = sc.flow.source;
    state.opts.flow.sink = sc.flow.sink;
    state.opts.flow.demand = sc.flow.demand ?? '';
    $('#mapTag').textContent = `${sc.name.toUpperCase()}`;
    $('#scenarioDesc').textContent = sc.description || '';
    // transmissions for the cybersecurity console
    $('#t1').value = sc.transmissions.t1; $('#t2').value = sc.transmissions.t2; $('#sigs').value = sc.transmissions.signatures.join(', ');
    state.cyber = null; state.cyberCmp = null; $('#cyberOut').innerHTML = '';
    applyView(); renderMeta(); renderControls(); renderMap(); renderLegend(); ensureResult();
  }

  function renderMeta() {
    const sc = state.scen;
    const chips = [`${sc.nodes.length} nodos`, `${sc.edges.length} enlaces`];
    $('#scenarioMeta').innerHTML = chips.map((c) => `<span class="chip">${esc(c)}</span>`).join('') +
      (state.down.size ? `<span class="chip warn">${state.down.size} enlace(s) caído(s)</span>` : '');
    $('#btnRestore').disabled = state.down.size === 0;
  }

  // ------------------------------------------------------------ run algorithms
  function graphInput() {
    const sc = state.scen;
    return {
      nodes: sc.nodes.map((n) => ({ id: n.id })),
      edges: sc.edges.map((e, i) => ({ id: i, u: e.u, v: e.v, cost: e.cost, capacity: e.capacity })).filter((e) => !state.down.has(e.id)),
    };
  }

  function requestFor(tab, over = {}) {
    const sc = state.scen, o = { ...state.opts[tab], ...over }, g = graphInput();
    switch (tab) {
      case 'mst': return ['mst', { ...g, method: o.method }];
      case 'tsp': return ['tsp', { ...g, start: o.start, method: o.method }];
      case 'flow': {
        const input = { ...g, source: o.source, sink: o.sink, method: o.method };
        if (o.demand !== '' && o.demand != null && !Number.isNaN(Number(o.demand))) input.demand = Number(o.demand);
        return ['maxflow', input];
      }
      default: {
        const k = sc.voronoi.k, [a, b, c, d] = sc.voronoi.domain;
        return ['voronoi', { sites: sc.nodes.map((n) => ({ id: n.id, x: n.lon * k, y: n.lat })), domain: [a * k, b, c * k, d], queries: state.queries.map((q) => ({ x: q[0], y: q[1] })) }];
      }
    }
  }

  async function runTab(tab = state.tab) {
    const token = (state.tokens[tab] = (state.tokens[tab] || 0) + 1);
    state.pending[tab] = true;
    renderResult();
    const [mode, input] = requestFor(tab);
    const resp = await core(mode, input);
    if (token !== state.tokens[tab]) return;           // a newer request superseded this one
    state.pending[tab] = false;
    state.res[tab] = resp;
    if (tab === state.tab) { renderResult(); renderMap(); }
  }
  const ensureResult = () => { if (!state.res[state.tab] && !state.pending[state.tab]) runTab(state.tab); else { renderResult(); renderMap(); } };
  const invalidate = () => { state.res = {}; state.cmp = {}; };

  async function compare(tab) {
    const methods = TABS[tab].compare;
    state.cmp[tab] = 'loading';
    renderResult();
    const rows = [];
    for (const m of methods) {
      const [mode, input] = requestFor(tab, { method: m });
      rows.push({ method: m, resp: await core(mode, input) });
    }
    state.cmp[tab] = rows;
    if (tab === state.tab) renderResult();
  }

  const summary = (tab, resp) => {
    if (!resp || !resp.ok) return null;
    const r = resp.result;
    return tab === 'mst' ? r.total_cost : tab === 'tsp' ? r.total_distance : r.max_flow;
  };

  // ------------------------------------------------------------------ controls
  const nodeLabel = (n) => `${n.id} · ${n.name}`;
  const nodeOptions = (sel) => state.scen.nodes.map((n) => `<option value="${esc(n.id)}"${n.id === sel ? ' selected' : ''}>${esc(nodeLabel(n))}</option>`).join('');
  const methodOptions = (tab, sel) => TABS[tab].methods.map(([v, l]) => `<option value="${v}"${v === sel ? ' selected' : ''}>${esc(l)}</option>`).join('');

  function renderControls() {
    const tab = state.tab, o = state.opts[tab], box = $('#controls');
    const field = (label, inner) => `<label class="field"><span class="lbl">${label}</span>${inner}</label>`;
    let html = '';
    if (tab === 'mst') html = `<p class="hint">Árbol de expansión mínima sobre los enlaces activos. Haz clic en un enlace del mapa para simular que se cae.</p>`;
    if (tab === 'tsp') html = field('Origen', `<select id="cStart">${nodeOptions(o.start)}</select>`);
    if (tab === 'flow') {
      html = field('Origen', `<select id="cSource">${nodeOptions(o.source)}</select>`) + field('Destino', `<select id="cSink">${nodeOptions(o.sink)}</select>`) +
        field(`Demanda (${esc(state.scen.units.cap)})`, `<input id="cDemand" type="number" min="0" step="any" value="${esc(o.demand)}" placeholder="opcional">`);
    }
    if (tab === 'voronoi') html = `<p class="hint">Haz clic en el mapa para consultar a qué central pertenece una ubicación nueva.</p><button id="btnClearQ" class="ghost" type="button"${state.queries.length ? '' : ' disabled'}>Borrar consultas</button>`;
    html += tab === 'voronoi' ? '' : `<button id="btnRun" class="primary" type="button">Calcular</button>`;
    box.innerHTML = html;

    const on = (id, ev, fn) => { const el = $(id, box); if (el) el.addEventListener(ev, fn); };
    const change = (key) => (e) => { state.opts[tab][key] = e.target.value; delete state.res[tab]; delete state.cmp[tab]; runTab(tab); };
    on('#cStart', 'change', change('start'));
    on('#cSource', 'change', change('source')); on('#cSink', 'change', change('sink'));
    on('#cDemand', 'change', change('demand'));
    on('#btnRun', 'click', () => { delete state.res[tab]; delete state.cmp[tab]; runTab(tab); });
    on('#btnClearQ', 'click', () => { state.queries = []; delete state.res.voronoi; runTab('voronoi'); renderControls(); });
  }

  // ------------------------------------------------------------------- results
  const nodeName = (id) => (state.scen.nodes.find((n) => n.id === id) || { name: id }).name;

  function renderResult() {
    const tab = state.tab, box = $('#result'), resp = state.res[tab];
    if (state.pending[tab] && !resp) { box.innerHTML = '<div class="busy">Ejecutando el núcleo C++…</div>'; return; }
    if (!resp) { box.innerHTML = ''; return; }
    if (!resp.ok) { box.innerHTML = `<div class="err"><b>El núcleo C++ respondió con un error.</b><br>${esc(typeof resp.error === 'string' ? resp.error : JSON.stringify(resp.error ?? resp))}${resp.request_preview ? `<details><summary>Ver petición enviada</summary><pre style="white-space:pre-wrap;word-break:break-all;font-size:11px">${esc(resp.request_preview)}</pre></details>` : ''}</div>`; return; }
    const r = resp.result, u = state.scen.units;
    let html = '';
    const downBadge = state.down.size ? badge(`${state.down.size} enlace(s) caído(s)`, 'amber') : '';
    if (tab === 'mst') {
      html = `${kicker(TABS.mst.metric)}<div class="big">${num(r.total_cost)}<small>${esc(u.cost)}</small></div>
        <div class="desc">Conecta todas las centrales al menor costo.</div>
        <div class="badges">${r.connected ? badge('Red conectada', 'ok') : badge(`Red desconectada · ${r.components} componentes`, 'bad')}${badge(`${r.edges.length} enlaces seleccionados`)}${downBadge}</div>
        <div class="scroll"><table class="t"><thead><tr><th>Enlace</th><th class="r">Costo (${esc(u.cost)})</th></tr></thead><tbody>
        ${r.edges.map((e) => `<tr><td title="${esc(nodeName(e.u))} — ${esc(nodeName(e.v))}">${esc(e.u)} — ${esc(e.v)}</td><td class="r">${num(e.cost)}</td></tr>`).join('')}</tbody></table></div>`;
    } else if (tab === 'tsp') {
      html = `${kicker(TABS.tsp.metric)}<div class="big">${num(r.total_distance)}<small>${esc(u.cost)}</small></div>
        <div class="desc">${esc(r.order.concat([r.order[0]]).join(' → '))}</div>
        <div class="badges">${r.exact ? badge('Óptimo · Held-Karp', 'ok') : badge('Aproximado · NN + 2-opt', 'amber')}${badge(`${r.order.length} centrales`)}${downBadge}</div>
        <div class="scroll"><table class="t"><thead><tr><th>Tramo</th><th class="r">Dist.</th><th>Pasa por</th></tr></thead><tbody>
        ${r.legs.map((l) => `<tr><td>${esc(l.from)} → ${esc(l.to)}</td><td class="r">${num(l.distance)}</td><td>${l.via.length ? esc(l.via.join(', ')) : '—'}</td></tr>`).join('')}</tbody></table></div>`;
    } else if (tab === 'flow') {
      const used = r.edges.filter((e) => Math.abs(e.flow) > 1e-9).sort((a, b) => Math.abs(b.flow) - Math.abs(a.flow));
      const maxCap = Math.max(...r.edges.map((e) => e.capacity), 1);
      html = `${kicker(TABS.flow.metric)}<div class="big">${num(r.max_flow)}<small>${esc(u.cap)}</small></div>
        <div class="desc">${esc(r.source)} (${esc(nodeName(r.source))}) → ${esc(r.sink)} (${esc(nodeName(r.sink))})</div>
        <div class="badges">${r.demand_satisfied === undefined ? '' : r.demand_satisfied ? badge(`Demanda de ${num(r.demand)} ${u.cap}: satisfecha`, 'ok') : badge(`Demanda de ${num(r.demand)} ${u.cap}: insuficiente`, 'bad')}
        ${badge(`Corte mínimo ${num(r.min_cut.capacity)} ${u.cap} · ${r.min_cut.edges.length} enlaces`)}${downBadge}</div>
        <div class="scroll"><table class="t"><thead><tr><th>Enlace</th><th class="r">Flujo / capacidad</th></tr></thead><tbody>
        ${used.map((e) => { const f = Math.abs(e.flow); const [a, b] = e.flow >= 0 ? [e.u, e.v] : [e.v, e.u];
          return `<tr><td>${esc(a)} → ${esc(b)}</td><td class="r">${num(f)} / ${num(e.capacity)}<span class="bar-in" style="width:${Math.round(60 * f / maxCap)}px"></span></td></tr>`; }).join('') || '<tr><td colspan="2">Sin flujo posible entre esos nodos.</td></tr>'}</tbody></table></div>`;
    } else {
      const k = state.scen.voronoi.k;
      const cells = r.cells.slice().sort((a, b) => b.area - a.area);
      html = `${kicker(TABS.voronoi.metric)}<div class="big">${num(100 * r.total_area / r.domain_area, 2)}<small>%</small></div>
        <div class="desc">Cada nueva contratación se asigna a la central más cercana (celdas de Voronoi).</div>
        ${r.queries.length ? `<div class="sep"></div>${kicker('Consultas')}<table class="t"><thead><tr><th>Ubicación</th><th>Central</th><th class="r">Distancia*</th></tr></thead><tbody>
        ${r.queries.map((q) => `<tr><td>${num(q.y, 2)}°N, ${num(q.x / k, 2)}°E</td><td>${esc(q.site)} · ${esc(nodeName(q.site))}${q.inside_domain ? '' : ' (fuera del dominio)'}</td><td class="r">≈ ${num(q.distance * 111.19, 0)} km</td></tr>`).join('')}</tbody></table>
        <p class="meta">* aproximación sobre el plano (1° ≈ 111 km)</p>` : ''}
        <div class="sep"></div>${kicker('Área de cada región')}<div class="scroll"><table class="t"><thead><tr><th>Central</th><th class="r">% del dominio</th></tr></thead><tbody>
        ${cells.map((c) => `<tr><td>${esc(c.id)} · ${esc(nodeName(c.id))}</td><td class="r">${num(100 * c.area / r.domain_area, 2)}</td></tr>`).join('')}</tbody></table></div>`;
    }
    html += `<p class="meta">${metaLine(resp)}</p>`;
    box.innerHTML = html;
  }

  function compareHtml(tab) {
    const c = state.cmp[tab];
    const button = `<button id="btnCompare" class="ghost cmpbtn" type="button">${c ? 'Volver a comparar' : 'Comparar métodos'}</button>`;
    if (c === 'loading') return '<div class="busy">Comparando métodos en el núcleo C++…</div>';
    if (!c) return button;
    const ref = summary(tab, c[0].resp);
    const best = Math.min(...c.map((x) => summary(tab, x.resp)).filter((v) => v != null));
    const label = (m) => (TABS[tab].methods.find(([v]) => v === m) || [m, m])[1];
    const rows = c.map(({ method, resp }) => {
      if (!resp.ok) return `<tr><td>${esc(label(method))}</td><td colspan="3">${esc(resp.error)}</td></tr>`;
      const v = summary(tab, resp);
      let note = '';
      if (tab === 'flow') note = Math.abs(v - ref) < 1e-6 ? '= mismo valor' : 'distinto';
      else if (Math.abs(v - best) < 1e-6) note = tab === 'tsp' ? 'óptimo' : '= mismo costo';
      else note = `+${num(100 * (v - best) / best, 2)} % vs mejor`;
      return `<tr><td>${esc(label(method))}</td><td class="r">${num(v)}</td><td class="r">${fmtTime(resp.meta.elapsed_us)}</td><td>${note}</td></tr>`;
    }).join('');
    return `<div class="sep"></div>${kicker('Comparación de métodos')}<table class="t"><thead><tr><th>Método</th><th class="r">Resultado</th><th class="r">C++</th><th>Nota</th></tr></thead><tbody>${rows}</tbody></table>${button}`;
  }

  // ----------------------------------------------------------------------- map
  function renderLegend() {
    const dot = (c) => `<span><span class="dot" style="background:${c}"></span>`;
    const line = (c, dashed) => `<span><i style="border-color:${c}${dashed ? ';border-top-style:dashed' : ''}"></i>`;
    let h = `${dot('#4fd1b6')}Central</span>${dot('#fdf0cf')}Nodo núcleo</span>${line('#4d6b68')}Enlace</span>${line('#ff7a85', true)}Enlace caído (clic para alternar)</span>`;
    if (state.tab === 'mst') h += `${line('#f5c04a')}Árbol de expansión mínima</span>`;
    if (state.tab === 'tsp') h += `${line('#ff8f6b')}Ruta de mantenimiento</span>`;
    if (state.tab === 'flow') h += `${line('#3ccfb4')}Flujo</span>${line('#ff7a85', true)}Corte mínimo</span>`;
    if (state.tab === 'voronoi') h += `<span>Regiones de servicio · clic para consultar</span>`;
    $('#legend').innerHTML = h;
  }

  function renderMap() {
    const sc = state.scen, P = state.proj, tab = state.tab;
    if (!sc) return;
    const resp = state.res[tab], r = resp && resp.ok ? resp.result : null;
    const pos = Object.fromEntries(sc.nodes.map((n) => [n.id, P.geo(n.lon, n.lat)]));
    const f = (v) => v.toFixed(1);
    let s = '';

    // graticule every 10 degrees
    const [x0, y0, x1, y1] = P.domain;
    for (let lon = Math.ceil(x0 / 10) * 10; lon <= x1; lon += 10) {
      const [sx, sa] = P.geo(lon, y0), [, sb] = P.geo(lon, y1);
      s += `<line class="grat" x1="${f(sx)}" y1="${f(sa)}" x2="${f(sx)}" y2="${f(sb)}"/><text class="gratlbl" x="${f(sx + 3)}" y="${f(sa - 5)}">${lon}°</text>`;
    }
    for (let lat = Math.ceil(y0 / 10) * 10; lat <= y1; lat += 10) {
      const [sa, sy] = P.geo(x0, lat), [sb] = P.geo(x1, lat);
      s += `<line class="grat" x1="${f(sa)}" y1="${f(sy)}" x2="${f(sb)}" y2="${f(sy)}"/><text class="gratlbl" x="${f(sa + 4)}" y="${f(sy - 4)}">${lat}°</text>`;
    }
    const [ra, rb] = P.geo(x0, y1), [rc, rd] = P.geo(x1, y0);
    s += `<rect class="dom" x="${f(ra)}" y="${f(rb)}" width="${f(rc - ra)}" height="${f(rd - rb)}"/>`;

    // Voronoi regions
    if (tab === 'voronoi' && r) {
      r.cells.forEach((c, i) => {
        if (!c.polygon.length) return;
        const pts = c.polygon.map((p) => P.toSvg(p[0], p[1]).map(f).join(',')).join(' ');
        s += `<polygon class="vcell" points="${pts}" style="fill:hsl(${(i * 47) % 360} 75% 58% / .18)"><title>${esc(c.id)} · ${esc(nodeName(c.id))}</title></polygon>`;
      });
    }

    // links
    const mst = tab === 'mst' && r ? new Set(r.edges.map((e) => Number(e.id))) : null;
    const cut = tab === 'flow' && r ? new Set(r.min_cut.edges.map(Number)) : null;
    sc.edges.forEach((e, i) => {
      const [ax, ay] = pos[e.u], [bx, by] = pos[e.v];
      let cls = 'edge';
      if (state.down.has(i)) cls += ' down'; else { if (mst && mst.has(i)) cls += ' mst'; if (cut && cut.has(i)) cls += ' cut'; }
      s += `<line class="${cls}" x1="${f(ax)}" y1="${f(ay)}" x2="${f(bx)}" y2="${f(by)}"/>`;
    });

    // max flow: thickness proportional to the flow, dashes animated along its direction
    if (tab === 'flow' && r) {
      const maxCap = Math.max(...r.edges.map((e) => e.capacity), 1);
      r.edges.forEach((e) => {
        if (Math.abs(e.flow) < 1e-9) return;
        const [a, b] = e.flow >= 0 ? [e.u, e.v] : [e.v, e.u];
        const [ax, ay] = pos[a], [bx, by] = pos[b];
        s += `<line class="flowband" style="stroke-width:${f(3 + 9 * Math.abs(e.flow) / maxCap)}px" x1="${f(ax)}" y1="${f(ay)}" x2="${f(bx)}" y2="${f(by)}"/><line class="flowdash" x1="${f(ax)}" y1="${f(ay)}" x2="${f(bx)}" y2="${f(by)}"/>`;
      });
    }

    // TSP tour (expanded through intermediate nodes)
    if (tab === 'tsp' && r) {
      const pts = r.path.map((id) => pos[id].map(f).join(',')).join(' ');
      s += `<polyline class="tour" points="${pts}"/><polyline class="tourdash" points="${pts}"/>`;
    }

    // click targets for every link (simulate a blackout)
    sc.edges.forEach((e, i) => {
      const [ax, ay] = pos[e.u], [bx, by] = pos[e.v];
      s += `<line class="hit" data-edge="${i}" x1="${f(ax)}" y1="${f(ay)}" x2="${f(bx)}" y2="${f(by)}"><title>${esc(nodeName(e.u))} — ${esc(nodeName(e.v))} · ${num(e.cost)} ${esc(sc.units.cost)} · ${num(e.capacity)} ${esc(sc.units.cap)}${state.down.has(i) ? ' · CAÍDO' : ''} (clic: ${state.down.has(i) ? 'restaurar' : 'simular caída'})</title></line>`;
    });

    // nodes
    const showLabels = sc.nodes.length <= 30;
    const orderOf = tab === 'tsp' && r ? new Map(r.order.map((id, i) => [id, i + 1])) : null;
    sc.nodes.forEach((n) => {
      const [x, y] = pos[n.id];
      let cls = 'node' + (n.core ? ' core' : '');
      if (tab === 'flow') { if (n.id === state.opts.flow.source) cls += ' src'; if (n.id === state.opts.flow.sink) cls += ' sink'; }
      if (tab === 'tsp' && n.id === state.opts.tsp.start) cls += ' start';
      s += `<circle class="${cls}" cx="${f(x)}" cy="${f(y)}" r="${n.core ? 8.5 : sc.nodes.length > 30 ? 4 : 6.5}"><title>${esc(nodeLabel(n))} (${n.lat}°N, ${n.lon}°E)</title></circle>`;
      if (showLabels) s += `<text class="nlabel" x="${f(x + 10)}" y="${f(y - 9)}">${esc(n.id)}</text>`;
      if (orderOf && showLabels) s += `<circle class="orderbg" cx="${f(x - 11)}" cy="${f(y + 12)}" r="7.5"/><text class="order" x="${f(x - 11)}" y="${f(y + 15.4)}">${orderOf.get(n.id)}</text>`;
    });

    // Voronoi queries
    if (tab === 'voronoi') {
      state.queries.forEach((q, i) => {
        const [qx, qy] = P.toSvg(q[0], q[1]);
        const hit = r && r.queries[i];
        if (hit && pos[hit.site]) s += `<line class="qline" x1="${f(qx)}" y1="${f(qy)}" x2="${f(pos[hit.site][0])}" y2="${f(pos[hit.site][1])}"/>`;
        s += `<circle class="qmark" cx="${f(qx)}" cy="${f(qy)}" r="6"/><circle cx="${f(qx)}" cy="${f(qy)}" r="2" fill="#fff"/>`;
      });
    }
    $('#map').innerHTML = s;
  }

  // ---- pan / zoom / click handling on the SVG
  function applyView() { const v = state.view; $('#map').setAttribute('viewBox', `${v.x} ${v.y} ${v.w} ${v.h}`); }
  function svgPoint(e) {
    const svg = $('#map'), pt = svg.createSVGPoint();
    pt.x = e.clientX; pt.y = e.clientY;
    const p = pt.matrixTransform(svg.getScreenCTM().inverse());
    return [p.x, p.y];
  }
  function zoomAt(factor, cx = state.view.x + state.view.w / 2, cy = state.view.y + state.view.h / 2) {
    const v = state.view, nw = clamp(v.w * factor, W / 12, W), nh = nw * H / W;
    state.view = { x: clamp(cx - (cx - v.x) * nw / v.w, -W * 0.1, W * 1.1 - nw), y: clamp(cy - (cy - v.y) * nh / v.h, -H * 0.1, H * 1.1 - nh), w: nw, h: nh };
    applyView();
  }
  function toggleEdge(i) {
    if (state.down.has(i)) state.down.delete(i); else state.down.add(i);
    invalidate(); renderMeta(); renderMap(); runTab(state.tab);
  }
  function bindMap() {
    const svg = $('#map');
    let drag = null;
    svg.addEventListener('wheel', (e) => { e.preventDefault(); const [cx, cy] = svgPoint(e); zoomAt(e.deltaY > 0 ? 1.15 : 1 / 1.15, cx, cy); }, { passive: false });
    svg.addEventListener('pointerdown', (e) => {
      drag = { x: e.clientX, y: e.clientY, view: { ...state.view }, moved: false, target: e.target };
    });
    window.addEventListener('pointermove', (e) => {
      if (!drag) return;
      const dx = e.clientX - drag.x, dy = e.clientY - drag.y;
      if (!drag.moved && Math.hypot(dx, dy) < 4) return;
      drag.moved = true; svg.classList.add('dragging');
      const k = drag.view.w / svg.getBoundingClientRect().width;
      state.view.x = drag.view.x - dx * k; state.view.y = drag.view.y - dy * k; applyView();
    });
    window.addEventListener('pointerup', (e) => {
      if (!drag) return;
      const d = drag; drag = null; svg.classList.remove('dragging');
      if (d.moved) return;
      const edge = d.target.closest && d.target.closest('[data-edge]');
      if (edge) { toggleEdge(Number(edge.dataset.edge)); return; }
      if (state.tab === 'voronoi' && d.target.closest && d.target.closest('#map')) {
        const [sx, sy] = svgPoint(e), q = state.proj.fromSvg(sx, sy);
        state.queries = state.queries.concat([q]).slice(-8);
        delete state.res.voronoi; renderControls(); runTab('voronoi');
      }
    });
    $('#zoomIn').addEventListener('click', () => zoomAt(1 / 1.4));
    $('#zoomOut').addEventListener('click', () => zoomAt(1.4));
    $('#zoomReset').addEventListener('click', () => { state.view = { x: 0, y: 0, w: W, h: H }; applyView(); });
  }

  // ------------------------------------------------------- cybersecurity console
  const SIG_COLORS = ['#ffe2a8', '#d9c8ff', '#bdeedd', '#ffd0d6', '#c9e9f6'];
  const norm = (s) => s.replace(/\s+/g, '').toUpperCase();
  const STRAND_LIMIT = 4000;

  function mulberry32(seed) {
    let a = seed >>> 0;
    return () => { a = (a + 0x6d2b79f5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
  }

  function generateRandom() {
    const n = clamp(Number($('#randLen').value) || 2000, 10, 300000), seed = state.seedCounter++, R = mulberry32(seed);
    const hex = (len) => { let s = ''; for (let i = 0; i < len; i++) s += '0123456789ABCDEF'[(R() * 16) | 0]; return s; };
    const t1 = hex(n), block = Math.max(6, Math.floor(n / 10)), from = Math.floor(R() * (n - block)), at = Math.floor(R() * (n - block));
    const t2 = hex(n).slice(0, at) + t1.slice(from, from + block) + hex(n).slice(at + block);
    $('#t1').value = t1; $('#t2').value = t2.slice(0, n);
    $('#sigs').value = `${t1.slice(Math.floor(n / 2), Math.floor(n / 2) + 6)}, FF00FF, DEADBEEF`;
    $('#btnRandom').textContent = `Generar aleatorias (semilla ${seed})`;
  }

  async function analyze() {
    const t1 = $('#t1').value, t2 = $('#t2').value, out = $('#cyberOut');
    const sigs = $('#sigs').value.split(',').map((s) => s.trim()).filter(Boolean);
    if (!norm(t1) || !norm(t2)) { out.innerHTML = '<div class="err">Escribe las dos transmisiones.</div>'; return; }
    out.innerHTML = '<div class="busy">Ejecutando el núcleo C++…</div>';
    state.cyberCmp = null;
    const base = { transmissions: [t1, t2], signatures: sigs };
    const [m, p, l] = await Promise.all([
      core('matching', { ...base, method: 'kmp' }),
      core('palindrome', { ...base, method: 'manacher' }),
      core('lcs', { ...base, method: 'auto' }),
    ]);
    state.cyber = { m, p, l, t: [norm(t1), norm(t2)], sigs: sigs.map(norm) };
    renderCyber();
  }

  function strand(label, text, marks) {
    const n = Math.min(text.length, STRAND_LIMIT), cls = Array.from({ length: n }, () => []);
    marks.forEach((mk) => { for (let i = mk.from; i < mk.from + mk.len && i < n; i++) if (i >= 0) cls[i].push(mk.cls); });
    let rows = '';
    for (let line = 0; line < n; line += 64) {
      let html = '', i = line;
      while (i < Math.min(line + 64, n)) {
        const key = cls[i].join(' ');
        let j = i;
        while (j < Math.min(line + 64, n) && cls[j].join(' ') === key) j++;
        const chunk = esc(text.slice(i, j));
        html += key ? `<span class="${key}">${chunk}</span>` : chunk;
        i = j;
      }
      rows += `<div class="srow"><span class="sidx">${line + 1}</span><span class="schars">${html}</span></div>`;
    }
    const more = text.length > n ? `<div class="note">… ${num(text.length - n, 0)} caracteres más (la vista muestra los primeros ${num(n, 0)}; las posiciones reportadas siguen siendo completas).</div>` : '';
    return `<div class="strand"><span class="lbl">${esc(label)} · ${num(text.length, 0)} caracteres</span>${rows}${more}</div>`;
  }

  function renderCyber() {
    const c = state.cyber, out = $('#cyberOut');
    if (!c) { out.innerHTML = ''; return; }
    const errs = [c.m, c.p, c.l].filter((x) => !x.ok);
    if (errs.length) { out.innerHTML = errs.map((x) => `<div class="err"><b>Error del núcleo C++:</b> ${esc(x.error)}</div>`).join(''); return; }
    const nS = c.sigs.length, M = c.m.result.results, P = c.p.result.results, L = c.l.result;
    const cell = (x) => (x.found ? `<span class="hit-true">true ${x.position}</span>` : '<span class="hit-false">false</span>');
    const kmp = `<table class="t mono"><thead><tr><th>Firma</th><th>T1</th><th>T2</th></tr></thead><tbody>${c.sigs.map((sig, i) =>
      `<tr><td><span class="sw" style="background:${SIG_COLORS[i % 5]}"></span>${esc(sig)}</td><td>${cell(M[i])}</td><td>${cell(M[nS + i])}</td></tr>`).join('')}</tbody></table>`;
    const marks = [[], []];
    M.forEach((x, k) => { if (x.found) marks[Math.floor(k / nS)].push({ from: x.position - 1, len: x.signature.length, cls: `m${(k % nS) % 5}` }); });
    P.forEach((x, k) => marks[k].push({ from: x.start - 1, len: x.length, cls: 'pal' }));
    if (L.found) { marks[0].push({ from: L.start - 1, len: L.length, cls: 'lcs' }); marks[1].push({ from: L.start_second - 1, len: L.length, cls: 'lcs' }); }
    const official = `<span class="c">Parte 1 · Patrones</span>\n${c.m.result.lines.join('\n')}\n<span class="c">Parte 2 · Palíndromos</span>\n${c.p.result.lines.join('\n')}\n<span class="c">Parte 3 · Subcadena común</span>\n${L.lines.join('\n')}`;
    out.innerHTML = `
      <div class="cards">
        <div class="card"><h3>Detección de firmas</h3>${kmp}<p class="meta">${metaLine(c.m)}</p></div>
        <div class="card"><h3>Palíndromo más largo</h3><table class="t mono"><tbody>${P.map((x, k) =>
          `<tr><td>T${k + 1}</td><td>${esc(x.preview)}</td><td class="r">${x.start}–${x.end}</td></tr>`).join('')}</tbody></table><p class="meta">${metaLine(c.p)}</p></div>
        <div class="card"><h3>Subcadena común más larga</h3>${L.found ?
          `<table class="t mono"><tbody><tr><td>${esc(L.preview)}</td><td class="r">T1 ${L.start}–${L.end}</td></tr><tr><td>longitud ${L.length}</td><td class="r">T2 ${L.start_second}–${L.end_second}</td></tr></tbody></table>` : '<p class="hit-false mono">Sin subcadena común</p>'}
          <p class="meta">${metaLine(c.l)}</p></div>
      </div>
      <div class="strands">
        ${strand('TRANSMISSION 1', c.t[0], marks[0])}${strand('TRANSMISSION 2', c.t[1], marks[1])}
        <div class="legendrow">${c.sigs.map((s, i) => `<span><span class="sw" style="background:${SIG_COLORS[i % 5]}"></span>firma ${esc(s)}</span>`).join('')}
          <span><span class="sw" style="background:#2fb67c"></span>palíndromo</span><span><span class="sw" style="background:#cdeae2"></span>subcadena común</span></div>
      </div>
      <div class="cmp"><span class="lbl">SALIDA EN EL FORMATO DEL RETO</span><pre class="official">${official}</pre></div>
      `;
  }

  async function compareCyber() {
    const c = state.cyber;
    if (!c) return;
    $('#cmpCyBody').innerHTML = '<div class="busy">Comparando métodos en el núcleo C++…</div>';
    const base = { transmissions: [$('#t1').value, $('#t2').value], signatures: c.sigs };
    const jobs = [['Firmas', 'matching', ['kmp', 'z', 'naive']], ['Palíndromo', 'palindrome', ['manacher', 'expand']], ['Subcadena común', 'lcs', ['dp', 'hash']]];
    const rows = [];
    for (const [name, mode, methods] of jobs)
      for (const method of methods) {
        const r = await core(mode, { ...base, method });
        rows.push({ name, method, ok: r.ok, lines: r.ok ? (r.result.lines || []).join(' | ') : r.error, us: r.ok ? r.meta.elapsed_us : null });
      }
    state.cyberCmp = rows;
    $('#cmpCyBody').innerHTML = cmpCyHtml();
  }
  const cmpCyHtml = () => !state.cyberCmp ? '' : `<table class="t mono"><thead><tr><th>Problema</th><th>Método</th><th>Resultado</th><th class="r">Tiempo C++</th></tr></thead><tbody>${state.cyberCmp.map((r) =>
    `<tr><td>${esc(r.name)}</td><td>${esc(r.method)}</td><td>${esc(r.lines)}</td><td class="r">${r.us == null ? '—' : fmtTime(r.us)}</td></tr>`).join('')}</tbody></table>`;

  // ------------------------------------------------------------------ benchmark
  async function loadBench() {
    const body = $('#benchBody');
    try {
      const r = await fetch('/api/benchmark');
      if (!r.ok) { body.innerHTML = '<div class="benchmeta">Aún no hay resultados. Ejecuta <code>make bench</code> y recarga la página.</div>'; return; }
      const b = await r.json(), m = b.meta;
      let html = `<div class="benchmeta">${esc(m.date)} · ${esc(m.cpu)} (${m.cores} hilos) · ${esc(m.os)}<br>Compilador: ${esc(m.compiler)} · banderas: <code>${esc(m.flags)}</code><br>Semilla ${m.seed} · ${m.repetitions} repeticiones (mediana) · tiempo = sólo el algoritmo, pico de memoria = VmHWM del proceso</div>`;
      b.groups.forEach((g) => {
        const maxMs = Math.max(...g.rows.map((x) => x.ms), 1e-6);
        html += `<div class="grp"><h3>${esc(g.title)}</h3><div class="cx">Complejidad teórica: ${esc(g.complexity)}</div><table class="t"><thead><tr><th>Instancia</th><th>Tamaño</th><th class="r">Tiempo</th><th></th><th class="r">Pico</th><th class="r">× tiempo</th><th class="r">× tamaño</th></tr></thead><tbody>
        ${g.rows.map((x, i) => {
          const prev = g.rows[i - 1];
          const bar = Math.max(2, Math.round(160 * Math.log10(1 + 1000 * x.ms) / Math.log10(1 + 1000 * maxMs)));
          return `<tr><td>${esc(x.label)}</td><td>${esc(x.size)}</td><td class="r">${num(x.ms, 3)} ms</td><td><span class="bar-in" style="width:${bar}px;background:var(--blue)"></span></td><td class="r">${x.peak_mb == null ? '—' : `${num(x.peak_mb, 1)} MB`}</td>
            <td class="r">${prev && prev.ms > 0 ? `×${num(x.ms / prev.ms, 1)}` : '—'}</td><td class="r">${prev ? `×${num(x.n / prev.n, 1)}` : '—'}</td></tr>`; }).join('')}</tbody></table></div>`;
      });
      body.innerHTML = html;
    } catch (e) {
      body.innerHTML = `<div class="err">No se pudo cargar el benchmark: ${esc(e.message)}</div>`;
    }
  }

  // ----------------------------------------------------------------------- init
  async function init() {
    bindMap();
    $('#tabs').addEventListener('click', (e) => {
      const b = e.target.closest('button[data-tab]');
      if (!b || b.dataset.tab === state.tab) return;
      state.tab = b.dataset.tab;
      document.querySelectorAll('#tabs button').forEach((x) => x.classList.toggle('on', x === b));
      renderControls(); renderLegend(); renderMap(); ensureResult();
    });
    $('#btnRestore').addEventListener('click', () => { state.down.clear(); invalidate(); renderMeta(); renderMap(); runTab(state.tab); });
    $('#btnUpload').addEventListener('click', () => $('#fileUpload').click());
    $('#fileUpload').addEventListener('change', async (e) => {
      const file = e.target.files[0];
      if (!file) return;
      try {
        const sc = normalizeScenario(JSON.parse(await file.text()));
        sc.id = `custom-${Date.now()}`;
        state.scenarios.push({ id: sc.id, name: `Personalizado · ${sc.name}`, raw: sc });
        const opt = new Option(`Personalizado · ${sc.name}`, sc.id);
        $('#scenarioSelect').add(opt); $('#scenarioSelect').value = sc.id;
        loadScenario(sc);
      } catch (err) { alert(`No se pudo cargar el escenario: ${err.message}`); }
      e.target.value = '';
    });
    $('#scenarioSelect').addEventListener('change', async (e) => {
      const entry = state.scenarios.find((s) => s.id === e.target.value);
      loadScenario(entry.raw || await (await fetch(`/api/scenarios/${entry.id}`)).json());
    });
    $('#btnAnalyze').addEventListener('click', analyze);
    $('#btnRandom').addEventListener('click', () => { generateRandom(); analyze(); });
    $('#btnExample').addEventListener('click', () => { $('#t1').value = 'AABCDCBA12FFABBA'; $('#t2').value = '00BCDCBAFFABBA77'; $('#sigs').value = 'ABBA, FF, DEAD'; analyze(); });

    try {
      const h = await (await fetch('/api/health')).json();
      const st = $('#coreStatus');
      st.textContent = h.binary ? 'núcleo C++ listo' : 'falta compilar: make';
      st.className = `status ${h.binary ? 'ok' : 'bad'}`;
    } catch { $('#coreStatus').textContent = 'sin servidor'; $('#coreStatus').className = 'status bad'; }

    try {
      state.scenarios = await (await fetch('/api/scenarios')).json();
      $('#scenarioSelect').innerHTML = state.scenarios.map((s) => `<option value="${esc(s.id)}">${esc(s.name)}</option>`).join('');
      if (state.scenarios.length) loadScenario(await (await fetch(`/api/scenarios/${state.scenarios[0].id}`)).json());
    } catch (e) { $('#scenarioDesc').textContent = `No se pudieron cargar los escenarios: ${e.message}`; }
    loadBench();
  }
  init();
})();