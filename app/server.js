#!/usr/bin/env node
/**
 * SENTINEL Digital Twin - web server.
 *
 * Zero npm dependencies. It does three things:
 *   1. serves the static UI in app/public
 *   2. exposes the scenarios in app/scenarios
 *   3. runs the C++ core (build/sentinel) for every algorithm request:
 *        POST /api/run   {"mode":"mst","input":{...}}  ->  core's JSON answer
 *
 * The UI never computes results itself: every number shown on screen comes
 * from the C++ executable, as the challenge requires.
 *
 *   node app/server.js            (PORT=3000 HOST=127.0.0.1 by default)
 */
'use strict';
const http = require('http');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');

const ROOT = path.resolve(__dirname, '..');
const PUBLIC_DIR = path.join(__dirname, 'public');
const SCENARIO_DIR = path.join(__dirname, 'scenarios');
const RESULTS_DIR = path.join(ROOT, 'results');
const BIN = process.env.SENTINEL_BIN || path.join(ROOT, 'build', process.platform === 'win32' ? 'sentinel.exe' : 'sentinel');
const PORT = Number(process.env.PORT || 3000);
const HOST = process.env.HOST || '127.0.0.1';
const MAX_BODY = 64 * 1024 * 1024;
const RUN_TIMEOUT_MS = 60_000;

const MIME = {
  '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8', '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8', '.svg': 'image/svg+xml', '.png': 'image/png', '.ico': 'image/x-icon',
};

function send(res, status, body, type = 'application/json; charset=utf-8') {
  const data = typeof body === 'string' || Buffer.isBuffer(body) ? body : JSON.stringify(body);
  res.writeHead(status, { 'Content-Type': type, 'Cache-Control': 'no-store' });
  res.end(data);
}

function readBody(req) {
  return new Promise((resolve, reject) => {
    const chunks = [];
    let size = 0;
    req.on('data', (c) => {
      size += c.length;
      if (size > MAX_BODY) { reject(Object.assign(new Error('request too large'), { status: 413 })); req.destroy(); return; }
      chunks.push(c);
    });
    req.on('end', () => resolve(Buffer.concat(chunks).toString('utf8')));
    req.on('error', reject);
  });
}

/** Spawns the C++ core, pipes the request in, parses the single JSON object out. */
function runCore(requestJson, quiet = false) {
  return new Promise((resolve, reject) => {
    const started = process.hrtime.bigint();
    const child = spawn(BIN, [], { stdio: ['pipe', 'pipe', 'pipe'] });
    const out = [];
    let errText = '';
    let outSize = 0;
    const timer = setTimeout(() => { child.kill('SIGKILL'); reject(Object.assign(new Error('el cálculo excedió el tiempo límite'), { status: 504 })); }, RUN_TIMEOUT_MS);
    child.stdout.on('data', (c) => { outSize += c.length; if (outSize <= MAX_BODY) out.push(c); });
    child.stderr.on('data', (c) => { errText += c; });
    child.on('error', (e) => { clearTimeout(timer); reject(Object.assign(new Error(`no se pudo ejecutar el núcleo C++: ${e.message}`), { status: 503 })); });
    child.on('close', (code) => {
      clearTimeout(timer);
      const wallMs = Number(process.hrtime.bigint() - started) / 1e6;
      try {
        const parsed = JSON.parse(Buffer.concat(out).toString('utf8'));
        parsed.wall_ms = Math.round(wallMs * 100) / 100;
        let reqMode = '?'; try { reqMode = JSON.parse(requestJson).mode; } catch {}
        if (parsed.ok === false || (parsed.ok === undefined && parsed.error)) {
          if (!quiet) console.error(`[núcleo] modo="${reqMode}" código=${code} ERROR`);
          if (!quiet) {
            console.error('  petición :', requestJson.slice(0, 400) + (requestJson.length > 400 ? '…' : ''));
            console.error('  respuesta:', JSON.stringify(parsed).slice(0, 600));
            if (errText) console.error('  stderr   :', errText.slice(0, 400));
          }
          parsed.ok = false;
          parsed.request_preview = requestJson.slice(0, 600);
        } else if (!quiet) console.log(`[núcleo] modo="${reqMode}" ok ${parsed.wall_ms} ms`);
        resolve(parsed);
      } catch {
        console.error(`[núcleo] código=${code}, salida no es JSON: ${Buffer.concat(out).toString('utf8').slice(0, 300)} | stderr: ${errText.slice(0, 300)}`);
        reject(Object.assign(new Error(`el núcleo terminó con código ${code} sin devolver JSON válido. ${errText.slice(0, 300)}`), { status: 500 }));
      }
    });
    child.stdin.on('error', () => { /* child exited early; handled on close */ });
    child.stdin.end(requestJson);
  });
}


// ---------------------------------------------------------------------------
// Adaptador "dialecto del equipo": traduce entre lo que habla la interfaz y el
// contrato del main.cpp del equipo (modos mst, tsp, matching, palindrome,
// common_substring; salidas {"edges","cost"}, {"route","cost","exact"}, ...).
// Se activa solo si el núcleo no entiende el modo "info" de la implementación
// de referencia, o con SENTINEL_DIALECT=team|ref.
// ---------------------------------------------------------------------------
let DIALECT = process.env.SENTINEL_DIALECT || null;
async function dialect() {
  if (DIALECT) return DIALECT;
  if (!fs.existsSync(BIN)) return 'ref';
  try {
    const r = await runCore(JSON.stringify({ mode: 'info', input: {} }), true);
    DIALECT = r && r.ok === true ? 'ref' : 'team';
  } catch { DIALECT = 'ref'; }
  console.log(`[núcleo] dialecto detectado: ${DIALECT === 'team' ? 'del equipo (adaptador activo)' : 'referencia'}`);
  return DIALECT;
}

const norm = (t) => String(t).replace(/\s+/g, '');
const fail = (error, extra = {}) => ({ ok: false, error, ...extra });

function dijkstra(edges, src) {
  const adj = new Map();
  const add = (a, b, w) => { if (!adj.has(a)) adj.set(a, []); adj.get(a).push([b, w]); };
  for (const e of edges) { add(String(e.u), String(e.v), e.cost); add(String(e.v), String(e.u), e.cost); }
  const dist = new Map([[String(src), 0]]), prev = new Map(), done = new Set();
  for (;;) {
    let best = null;
    for (const [k, d] of dist) if (!done.has(k) && (best === null || d < dist.get(best))) best = k;
    if (best === null) break;
    done.add(best);
    for (const [nb, w] of adj.get(best) || []) {
      const nd = dist.get(best) + w;
      if (!dist.has(nb) || nd < dist.get(nb)) { dist.set(nb, nd); prev.set(nb, best); }
    }
  }
  return { dist, path: (t) => { const out = [String(t)]; while (prev.has(out[0])) out.unshift(prev.get(out[0])); return out; } };
}

async function callTeam(mode, input) {
  const raw = await runCore(JSON.stringify({ mode, input }), true);
  return raw;
}
const teamFailed = (r) => r.ok === false || (r.error !== undefined && r.ok === undefined);

async function runTeam(mode, input) {
  const t0 = process.hrtime.bigint();
  const wall = () => Math.round(Number(process.hrtime.bigint() - t0) / 1e4) / 100;
  const meta = (method) => ({ method, elapsed_us: Math.round(Number(process.hrtime.bigint() - t0) / 1e3) });
  const out = (result, method) => ({ ok: true, mode, result, meta: meta(method), wall_ms: wall() });
  const logFail = (label, req, resp) => {
    console.error(`[núcleo·equipo] modo="${label}" ERROR`);
    console.error('  petición :', JSON.stringify(req).slice(0, 400));
    console.error('  respuesta:', JSON.stringify(resp).slice(0, 600));
  };

  if (mode === 'mst' || mode === 'tsp') {
    const edges = input.edges;
    const base = mode === 'mst' ? { algorithm: input.method || 'kruskal' } : { start: input.start };
    const variants = [
      { nodes: input.nodes.map((n) => n.id), edges: edges.map((e) => ({ u: e.u, v: e.v, cost: e.cost, capacity: e.capacity })), ...base },
      { nodes: input.nodes, edges: edges.map((e) => ({ u: e.u, v: e.v, cost: e.cost, capacity: e.capacity })), ...base },
    ];
    let r, used;
    for (const v of variants) { used = v; r = await callTeam(mode, v); if (!teamFailed(r)) break; }
    if (teamFailed(r)) { logFail(mode, { mode, input: used }, r); return fail(`Tu núcleo respondió: ${typeof r.error === 'string' ? r.error : JSON.stringify(r)}`, { request_preview: JSON.stringify({ mode, input: used }).slice(0, 600), wall_ms: wall() }); }

    if (mode === 'mst') {
      const free = edges.slice(), picked = [];
      for (const raw of r.edges || []) {
        const e = Array.isArray(raw) ? { u: raw[0], v: raw[1], cost: raw[2] } : raw;
        let k = free.findIndex((f) => ((String(f.u) === String(e.u) && String(f.v) === String(e.v)) || (String(f.u) === String(e.v) && String(f.v) === String(e.u))) && Math.abs(f.cost - e.cost) < 1e-6);
        if (k < 0) k = free.findIndex((f) => (String(f.u) === String(e.u) && String(f.v) === String(e.v)) || (String(f.u) === String(e.v) && String(f.v) === String(e.u)));
        picked.push({ id: k >= 0 ? free[k].id : -1, u: e.u, v: e.v, cost: e.cost });
        if (k >= 0) free.splice(k, 1);
      }
      return out({ edges: picked, total_cost: r.cost, connected: !!r.connected, components: r.connected ? 1 : undefined, node_count: input.nodes.length, edge_count: edges.length }, r.algorithm || base.algorithm);
    }
    // tsp
    let order = (r.route || []).map(String);
    if (order.length > 1 && order[0] === order[order.length - 1]) order.pop();
    const legs = [], full = [order[0]];
    const sp = new Map();
    for (let i = 0; i < order.length; i++) {
      const a = order[i], b = order[(i + 1) % order.length];
      if (!sp.has(a)) sp.set(a, dijkstra(edges, a));
      const d = sp.get(a), p = d.path(b);
      legs.push({ from: a, to: b, distance: d.dist.get(b), via: p.slice(1, -1) });
      full.push(...p.slice(1));
    }
    return out({ start: order[0], order, total_distance: r.cost, exact: !!r.exact, legs, path: full }, r.exact ? 'heldkarp' : 'heuristic');
  }

  if (mode === 'matching' || mode === 'palindrome' || mode === 'lcs') {
    const t = (input.transmissions || []).map(norm);
    const req = { transmission1: t[0], transmission2: t[1], signatures: (input.signatures || []).map(norm) };
    const teamMode = mode === 'lcs' ? 'common_substring' : mode;
    const r = await callTeam(teamMode, req);
    if (teamFailed(r)) { logFail(teamMode, { mode: teamMode, input: req }, r); return fail(`Tu núcleo respondió: ${typeof r.error === 'string' ? r.error : JSON.stringify(r)}`, { wall_ms: wall() }); }
    const prev = (tx, start, end) => tx.slice(start - 1, Math.min(end, start + 59));
    if (mode === 'matching') {
      const sigs = req.signatures, results = [], lines = [];
      (r.results || []).forEach((x, i) => {
        const ti = Math.floor(i / Math.max(sigs.length, 1)), si = i % Math.max(sigs.length, 1);
        results.push({ transmission: ti + 1, signature: sigs[si], found: !!x.found, ...(x.found ? { position: x.position } : {}) });
        lines.push(x.found ? `true ${x.position}` : 'false');
      });
      return out({ results, lines }, 'kmp');
    }
    if (mode === 'palindrome') {
      const results = [], lines = [];
      (r.results || []).forEach((x, i) => {
        results.push({ transmission: i + 1, start: x.start, end: x.end, length: x.end - x.start + 1, preview: prev(t[i] || '', x.start, x.end) });
        lines.push(`${x.start} ${x.end}`);
      });
      return out({ results, lines }, 'manacher');
    }
    const found = r.end >= r.start && r.end > 0;
    const res = { found, length: found ? r.end - r.start + 1 : 0, lines: [`${r.start} ${r.end}`] };
    if (found) { res.start = r.start; res.end = r.end; res.preview = prev(t[0], r.start, r.end); }
    return out(res, 'dp');
  }

  if (mode === 'maxflow' || mode === 'voronoi') {
    const req = mode === 'maxflow'
      ? { nodes: input.nodes.map((n) => n.id), edges: input.edges, source: input.source, sink: input.sink, directed: false, ...(input.demand !== undefined ? { demand: input.demand } : {}) }
      : input;
    const r = await callTeam(mode, req);
    if (teamFailed(r)) { logFail(mode, { mode, input: req }, r); return fail(`Tu núcleo respondió: ${typeof r.error === 'string' ? r.error : JSON.stringify(r)}`, { wall_ms: wall() }); }
    // La interfaz espera las celdas como objetos ({id, area, polygon}); el nucleo las entrega tambien en 'regions'
    return out(mode === 'voronoi' ? { ...r, cells: r.regions } : r, mode === 'maxflow' ? 'edmonds_karp' : 'voronoi');
  }

  const lista = 'mst, tsp, matching, palindrome, lcs, maxflow, voronoi';
  return fail(`El modo "${mode}" todavía no está implementado en tu src/main.cpp (solo hay: ${lista}). Falta enlazar ese algoritmo en main.cpp.`, { wall_ms: wall() });
}

function listScenarios() {
  if (!fs.existsSync(SCENARIO_DIR)) return [];
  return fs.readdirSync(SCENARIO_DIR).filter((f) => f.endsWith('.json')).sort().map((f) => {
    const sc = JSON.parse(fs.readFileSync(path.join(SCENARIO_DIR, f), 'utf8'));
    return { id: sc.id, name: sc.name, nodes: sc.nodes.length, edges: sc.edges.length };
  });
}

async function handleApi(req, res, url) {
  if (req.method === 'GET' && url.pathname === '/api/health') {
    return send(res, 200, { ok: true, binary: fs.existsSync(BIN), bin: path.relative(ROOT, BIN), node: process.version, dialect: await dialect() });
  }
  if (req.method === 'GET' && url.pathname === '/api/scenarios') return send(res, 200, listScenarios());
  const m = url.pathname.match(/^\/api\/scenarios\/([\w.-]+)$/);
  if (req.method === 'GET' && m) {
    const file = path.join(SCENARIO_DIR, `${m[1]}.json`);
    if (!file.startsWith(SCENARIO_DIR + path.sep) || !fs.existsSync(file)) return send(res, 404, { ok: false, error: 'escenario no encontrado' });
    return send(res, 200, fs.readFileSync(file, 'utf8'));
  }
  if (req.method === 'GET' && url.pathname === '/api/benchmark') {
    const file = path.join(RESULTS_DIR, 'benchmark.json');
    if (!fs.existsSync(file)) return send(res, 404, { ok: false, error: 'aún no hay benchmark: ejecuta `make bench`' });
    return send(res, 200, fs.readFileSync(file, 'utf8'));
  }
  if (req.method === 'POST' && url.pathname === '/api/run') {
    if (!fs.existsSync(BIN)) return send(res, 503, { ok: false, error: 'el núcleo C++ no está compilado: ejecuta `make`' });
    const body = await readBody(req);
    try { JSON.parse(body); } catch { return send(res, 400, { ok: false, error: 'el cuerpo no es JSON válido' }); }
    if ((await dialect()) === 'team') {
      const { mode, input } = JSON.parse(body);
      return send(res, 200, await runTeam(mode, input || {}));
    }
    return send(res, 200, await runCore(body));
  }
  return send(res, 404, { ok: false, error: 'ruta de API desconocida' });
}

function serveStatic(req, res, url) {
  let rel = decodeURIComponent(url.pathname);
  if (rel.endsWith('/')) rel += 'index.html';
  const file = path.normalize(path.join(PUBLIC_DIR, rel));
  if (!file.startsWith(PUBLIC_DIR + path.sep) || !fs.existsSync(file) || !fs.statSync(file).isFile()) return send(res, 404, 'No encontrado', 'text/plain; charset=utf-8');
  send(res, 200, fs.readFileSync(file), MIME[path.extname(file)] || 'application/octet-stream');
}

const server = http.createServer(async (req, res) => {
  try {
    const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
    if (url.pathname.startsWith('/api/')) await handleApi(req, res, url);
    else serveStatic(req, res, url);
  } catch (e) {
    send(res, e.status || 500, { ok: false, error: e.message });
  }
});

if (require.main === module) {
  server.listen(PORT, HOST, () => {
    console.log(`SENTINEL Digital Twin → http://${HOST === '0.0.0.0' ? 'localhost' : HOST}:${PORT}`);
    if (!fs.existsSync(BIN)) console.warn(`⚠ Falta el núcleo C++ (${path.relative(ROOT, BIN)}). Ejecuta: make`);
  });
}
module.exports = { server, runCore };