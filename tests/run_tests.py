#!/usr/bin/env python3
"""Ejecuta cada caso de tests/cases contra el binario C++ y compara la salida.

Uso: python3 tests/run_tests.py [ruta/al/binario]   (por defecto build/sentinel)
Un caso pasa si cada campo de "expect" aparece en la salida (comparacion
parcial; las listas se comparan elemento por elemento).
"""
import glob, json, os, subprocess, sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN = sys.argv[1] if len(sys.argv) > 1 else os.path.join(RAIZ, "build", "sentinel")


def contiene(real, esperado):
    if isinstance(esperado, dict):
        return isinstance(real, dict) and all(k in real and contiene(real[k], v) for k, v in esperado.items())
    if isinstance(esperado, list):
        return isinstance(real, list) and len(real) == len(esperado) and all(contiene(r, e) for r, e in zip(real, esperado))
    if isinstance(esperado, (int, float)) and not isinstance(esperado, bool):
        return isinstance(real, (int, float)) and abs(real - esperado) < 1e-6
    return real == esperado


fallos = 0
casos = sorted(glob.glob(os.path.join(RAIZ, "tests", "cases", "*.json")))
for ruta in casos:
    caso = json.load(open(ruta))
    p = subprocess.run([BIN], input=json.dumps(caso["request"]), capture_output=True, text=True, timeout=30)
    try:
        salida = json.loads(p.stdout)
    except ValueError:
        salida = None
    ok = salida is not None and contiene(salida, caso["expect"])
    fallos += not ok
    print(("PASS " if ok else "FAIL ") + caso["name"])
    if not ok:
        print("   esperado:", json.dumps(caso["expect"]))
        print("   obtenido:", p.stdout.strip()[:300])
print(f"\n{len(casos) - fallos}/{len(casos)} casos correctos")
sys.exit(1 if fallos else 0)
