#ifndef ARBOL_EXPAN_MIN_H
#define ARBOL_EXPAN_MIN_H

#include <vector>
#include <climits>
#include <utility>

using namespace std;

// Resultado del cableado: qué colonias se conectan y cuántos km de fibra se usan
struct ResultadoMST {
    vector<pair<int, int>> conexiones;
    int costoTotal;
};

// Algoritmo de Prim sobre la matriz de distancias.
// Complejidad: O(N^2) tiempo, O(N) memoria extra.
// Se usa la versión con arreglo (sin heap) porque la matriz es un grafo denso:
// todas las colonias tienen distancia a todas, así que E = N^2.
inline ResultadoMST arbolExpansionMinima(const vector<vector<int>>& distancias) {
    int n = distancias.size();
    ResultadoMST resultado = {{}, 0};
    if (n <= 1) return resultado;

    vector<bool> enArbol(n, false);      // ¿la colonia ya está cableada?
    vector<int> costoMinimo(n, INT_MAX); // cable más barato para conectarla al árbol
    vector<int> conectadoDesde(n, -1);   // desde qué colonia sale ese cable

    costoMinimo[0] = 0; // Empezamos en la colonia A

    for (int paso = 0; paso < n; ++paso) {
        // 1. Elegir la colonia fuera del árbol con el cable más barato
        int u = -1;
        for (int v = 0; v < n; ++v) {
            if (!enArbol[v] && (u == -1 || costoMinimo[v] < costoMinimo[u])) {
                u = v;
            }
        }

        // 2. Meterla al árbol y guardar la conexión que usamos
        enArbol[u] = true;
        if (conectadoDesde[u] != -1) {
            resultado.conexiones.push_back({conectadoDesde[u], u});
            resultado.costoTotal += costoMinimo[u];
        }

        // 3. Ver si desde u ahora es más barato llegar a las demás colonias
        for (int v = 0; v < n; ++v) {
            if (!enArbol[v] && distancias[u][v] < costoMinimo[v]) {
                costoMinimo[v] = distancias[u][v];
                conectadoDesde[v] = u;
            }
        }
    }

    return resultado;
}

#endif