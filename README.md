# Sentinela del Norte
En este README se encontrarán todas las instrucciones e información sobre la compilación, instalación, ejecución, arquitectura y dependencias para el buen funcionamiento del programa. 

## Arquitectura
El proyecto está diseñado bajo el patrón Header-Only Library, donde toda la lógica algorítmica y las estructuras de datos residen en archivos de cabecera (.h). El archivo main.cpp actúa exclusivamente como el despachador que lee la entrada JSON, selecciona el módulo activo y escribe el resultado estructurado.

## Módulos principales
1. Módulo I: Análisis Topológico y Geometría de Red
   * mst: Cálculo del Árbol de Expansión Mínima (Kruskal / Prim).
   * tsp: Ruta óptima de inspección física pasando por todos los nodos (TSP).
   * max_flow: Análisis de capacidad de transmisión evaluando Edmonds-Karp y Dinic simultáneamente.
   * voronoi: Cobertura de regiones espaciales y asignación de contrataciones a la central más cercana recortando el dominio con el semiplano de cada central (O(n²)); también se incluye la triangulación de Delaunay (Bowyer-Watson) en `diagramaVoronoi.h`.
2. Módulo II: Ciberseguridad y Procesamiento de Transmisiones
   * matching: Localización de firmas de malware en transmisiones de red mediante Knuth-Morris-Pratt (KMP).
   * palindrome: Detección de secuencias palindrómicas espejo en código malicioso con Manacher.
   * common_substring: Identificación de la subcadena común más larga (LCS) compartida entre transmisiones.
  
## Depedencias
* Solo requiere un compilador para archivos de C++
* No ocupa ninguna librería adicional

## Compilación
Desde la raíz del repositorio:

```
mkdir -p build
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -o build/sentinel src/main.cpp
```

* Estándar: C++17. Compila sin advertencias con g++ y con clang++.
* El ejecutable resultante es `build/sentinel`. No se incluye en el repositorio (`build/` está en `.gitignore`), así que hay que compilarlo.
* En Windows (MinGW) el mismo comando genera `build/sentinel.exe`.

## Instalación
1. Instalar un compilador de C++ con soporte para C++17:
   * macOS: `xcode-select --install` (incluye clang++).
   * Linux (Debian/Ubuntu): `sudo apt install g++`.
   * Windows: MinGW-w64 o MSYS2.
2. Clonar o descomprimir el proyecto y compilar con el comando de la sección anterior.
3. Opcional, solo para la interfaz web y las pruebas:
   * Node.js 18 o superior para `app/server.js` (no requiere `npm install`, no usa paquetes externos).
   * Python 3 para `tests/run_tests.py` y `tests/benchmark.py`.

El núcleo en C++ funciona solo, sin la interfaz, ni Node ni Python.

## Ejecución
El programa funciona leyendo un objeto JSON desde la entrada estándar (stdin) y enviando el resultado en JSON a la salida estándar (stdout).
