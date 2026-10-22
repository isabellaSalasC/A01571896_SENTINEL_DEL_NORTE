# Sentinela del Norte
En este README se encontrarán todas las instrucciones e información sobre la compilación, instalación, ejecución, arquitectura y dependencias para el buen funcionamiento del programa. 

## Arquitectura
El proyecto está diseñado bajo el patrón Header-Only Library, donde toda la lógica algorítmica y las estructuras de datos residen en archivos de cabecera (.h). El archivo main.cpp actúa exclusivamente como el despachador que lee la entrada JSON, selecciona el módulo activo y escribe el resultado estructurado.

## Módulos principales
1. Módulo I: Análisis Topológico y Geometría de Red
   * mst: Cálculo del Árbol de Expansión Mínima (Kruskal / Prim).
   * tsp: Ruta óptima de inspección física pasando por todos los nodos (TSP).
   * max_flow: Análisis de capacidad de transmisión evaluando Edmonds-Karp y Dinic simultáneamente.
   * voronoi: Cobertura de regiones espaciales y asignación de contrataciones a la central más cercana mediante el algoritmo de Bowyer-Watson.
2. Módulo II: Ciberseguridad y Procesamiento de Transmisiones
   * matching: Localización de firmas de malware en transmisiones de red mediante Knuth-Morris-Pratt (KMP).
   * palindrome: Detección de secuencias palindrómicas espejo en código malicioso con Manacher.
   * common_substring: Identificación de la subcadena común más larga (LCS) compartida entre transmisiones.
  
## Depedencias
* Solo requiere un compilador para archivos de C++
* No ocupa ninguna librería adicional

## Compilación

## Instalalción

## Ejecución
El programa funciona leyendo un objeto JSON desde la entrada estándar (stdin) y enviando el resultado en JSON a la salida estándar (stdout).
