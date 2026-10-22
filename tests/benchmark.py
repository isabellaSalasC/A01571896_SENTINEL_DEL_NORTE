#!/usr/bin/env python3
"""Benchmarks reproducibles: instancias pequenas, medianas y grandes por modo.

Uso: python3 tests/benchmark.py [ruta/al/binario]
Genera results/benchmark.csv (semilla fija) y results/entorno.txt.
Mide tiempo de pared del proceso completo y pico de memoria (maxrss).
"""
import csv, json, os, platform, random, resource, subprocess, sys, time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = sys.argv[1] if len(sys.argv) > 1 else os.path.join(RAIZ, "build", "sentinel")
SEMILLA = 2038
HEX = "0123456789ABCDEF"


def hexa(rng, n):
    return "".join(rng.choice(HEX) for _ in range(n))


def red(rng, n, extra):
    aristas = [[i, rng.randrange(i), rng.randint(1, 100)] for i in range(1, n)]
    aristas += [[rng.randrange(n), rng.randrange(n), rng.randint(1, 100)] for _ in range(extra)]
    return [i for i in range(n)], aristas


def generar(modo, n, rng):
    if modo == "mst":
        nodos, ar = red(rng, n, 4 * n)
        return {"nodes": nodos, "edges": ar}
    if modo == "tsp":
        nodos, ar = red(rng, n, n * n // 2)
        return {"nodes": nodos, "edges": ar, "start": 0}
    if modo == "maxflow":
        nodos, ar = red(rng, n, 4 * n)
        return {"nodes": nodos, "edges": [{"id": i, "u": u, "v": v, "cost": 1, "capacity": c} for i, (u, v, c) in enumerate(ar)], "source": 0, "sink": n - 1}
    if modo == "voronoi":
        sitios = [{"id": i, "x": rng.uniform(0, 1000), "y": rng.uniform(0, 1000)} for i in range(n)]
        return {"sites": sitios, "domain": [0, 0, 1000, 1000], "queries": [{"x": 500, "y": 500}]}
    t1, t2 = hexa(rng, n), hexa(rng, n)
    if modo == "matching":
        return {"transmission1": t1, "transmission2": t2, "signatures": ["ABBA", "FF00", "DEADBEEF"]}
    return {"transmission1": t1, "transmission2": t2}


TAMANOS = {
    "mst": [10, 1000, 100000],
    "tsp": [8, 12, 15],
    "maxflow": [10, 1000, 20000],
    "voronoi": [10, 200, 1000],
    "matching": [100, 10000, 1000000],
    "palindrome": [100, 10000, 1000000],
    "common_substring": [100, 2000, 10000],
}

rng = random.Random(SEMILLA)
filas = []
for modo, tams in TAMANOS.items():
    for etiqueta, n in zip(["pequena", "mediana", "grande"], tams):
        peticion = json.dumps({"mode": modo, "input": generar(modo, n, rng)})
        t0 = time.perf_counter()
        proc = subprocess.Popen([BIN], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
        try:
            # wait4 entrega el uso de recursos de ESTE proceso (pico de memoria propio)
            proc.stdin.write(peticion)
            proc.stdin.close()
            salida = proc.stdout.read()
            _, status, uso = os.wait4(proc.pid, 0)
            proc.returncode = 0
            estado = "ok" if os.WEXITSTATUS(status) == 0 and not salida.startswith('{"error"') else "error"
            rss_mb = uso.ru_maxrss / (1024 * 1024 if sys.platform == "darwin" else 1024)
        except Exception:
            proc.kill()
            estado, rss_mb = "error", 0
        ms = (time.perf_counter() - t0) * 1000
        filas.append([modo, etiqueta, n, estado, round(ms, 2), round(rss_mb, 1)])
        print(f"{modo:17s} {etiqueta:8s} n={n:<8d} {estado:7s} {ms:9.2f} ms  {rss_mb:6.1f} MB")

os.makedirs(os.path.join(RAIZ, "results"), exist_ok=True)
with open(os.path.join(RAIZ, "results", "benchmark.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["modo", "tamano", "n", "estado", "tiempo_ms", "pico_memoria_mb"])
    w.writerows(filas)
with open(os.path.join(RAIZ, "results", "entorno.txt"), "w") as f:
    f.write(f"Sistema: {platform.platform()}\nProcesador: {platform.processor() or platform.machine()}\n")
    f.write(f"Python: {platform.python_version()}\nSemilla: {SEMILLA}\n")
    f.write("Compilacion: g++ -std=c++17 -O2 -o build/sentinel src/main.cpp\n")
    f.write("Procedimiento: python3 tests/benchmark.py\n")
