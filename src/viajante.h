#ifndef VIAJANTE_H
#define VIAJANTE_H

#include <vector>
#include <climits>
#include <algorithm>

using namespace std;

// Hasta este número de colonias se usa el algoritmo exacto.
// Memoria de Held-Karp: 2^18 * 18 * 2 tablas * 4 bytes ≈ 38 MB.
const int LIMITE_EXACTO = 18;

// Resultado de la ruta de reparto: empieza y termina en la colonia A (0)
struct ResultadoTSP {
    vector<int> ruta;
    int costoTotal;
    bool esOptimo;
};

// Suma el costo de recorrer la ruta completa
inline int costoRuta(const vector<vector<int>>& distancias, const vector<int>& ruta) {
    int total = 0;
    for (size_t i = 0; i + 1 < ruta.size(); ++i) {
        total += distancias[ruta[i]][ruta[i + 1]];
    }
    return total;
}

// Held-Karp (programación dinámica con máscaras de bits). Da la ruta ÓPTIMA.
// dp[mascara][u] = costo mínimo de salir de A, visitar las colonias de "mascara"
// y estar parados en u. Cada bit de la máscara es una colonia (1 = ya visitada).
// Complejidad: O(2^N * N^2) tiempo, O(2^N * N) memoria.
inline ResultadoTSP viajanteExacto(const vector<vector<int>>& distancias) {
    int n = distancias.size();
    int totalMascaras = 1 << n;
    vector<vector<int>> dp(totalMascaras, vector<int>(n, INT_MAX));
    vector<vector<int>> anterior(totalMascaras, vector<int>(n, -1));

    dp[1][0] = 0; // Solo hemos visitado A (bit 0) y estamos en A

    for (int mascara = 1; mascara < totalMascaras; ++mascara) {
        if (!(mascara & 1)) continue; // Toda ruta válida incluye a A

        for (int u = 0; u < n; ++u) {
            if (!(mascara & (1 << u)) || dp[mascara][u] == INT_MAX) continue;

            // Intentar ir de u a una colonia v que aún no visitamos
            for (int v = 0; v < n; ++v) {
                if (mascara & (1 << v)) continue;

                int nuevaMascara = mascara | (1 << v);
                int nuevoCosto = dp[mascara][u] + distancias[u][v];
                if (nuevoCosto < dp[nuevaMascara][v]) {
                    dp[nuevaMascara][v] = nuevoCosto;
                    anterior[nuevaMascara][v] = u;
                }
            }
        }
    }

    // Con todas visitadas, elegir la última colonia que da el regreso más barato a A
    int todas = totalMascaras - 1;
    int mejorCosto = INT_MAX;
    int ultima = -1;
    for (int u = 1; u < n; ++u) {
        if (dp[todas][u] == INT_MAX) continue;
        int costo = dp[todas][u] + distancias[u][0];
        if (costo < mejorCosto) {
            mejorCosto = costo;
            ultima = u;
        }
    }

    // Reconstruir la ruta caminando hacia atrás con "anterior"
    vector<int> ruta;
    int mascara = todas;
    int actual = ultima;
    while (actual != -1) {
        ruta.push_back(actual);
        int previo = anterior[mascara][actual];
        mascara ^= (1 << actual); // Quitar la colonia de la máscara
        actual = previo;
    }
    reverse(ruta.begin(), ruta.end()); // Queda A, ..., ultima
    ruta.push_back(0);                 // Regresar a A

    return {ruta, mejorCosto, true};
}

// Vecino más cercano + 2-opt. Rápido pero NO garantiza el óptimo.
// Se usa cuando N es muy grande para Held-Karp.
// Complejidad: O(N^2) la construcción, O(N^2) por cada pasada de 2-opt.
// Supone matriz simétrica (ir de u a v cuesta lo mismo que de v a u).
inline ResultadoTSP viajanteAproximado(const vector<vector<int>>& distancias) {
    int n = distancias.size();
    vector<bool> visitado(n, false);
    vector<int> ruta = {0};
    visitado[0] = true;

    // 1. Vecino más cercano: siempre ir a la colonia no visitada más cercana
    int actual = 0;
    for (int paso = 1; paso < n; ++paso) {
        int siguiente = -1;
        for (int v = 0; v < n; ++v) {
            if (!visitado[v] && (siguiente == -1 || distancias[actual][v] < distancias[actual][siguiente])) {
                siguiente = v;
            }
        }
        visitado[siguiente] = true;
        ruta.push_back(siguiente);
        actual = siguiente;
    }
    ruta.push_back(0);

    // 2. 2-opt: si dos tramos de la ruta se "cruzan", invertir el pedazo de en medio
    bool mejoro = true;
    while (mejoro) {
        mejoro = false;
        for (int i = 1; i < n - 1; ++i) {
            for (int j = i + 1; j < n; ++j) {
                int a = ruta[i - 1], b = ruta[i];
                int c = ruta[j],     d = ruta[j + 1];
                // ¿Cambiar a-b y c-d por a-c y b-d acorta la ruta?
                if (distancias[a][c] + distancias[b][d] < distancias[a][b] + distancias[c][d]) {
                    reverse(ruta.begin() + i, ruta.begin() + j + 1);
                    mejoro = true;
                }
            }
        }
    }

    return {ruta, costoRuta(distancias, ruta), false};
}

// Decide qué algoritmo usar según el tamaño
inline ResultadoTSP viajante(const vector<vector<int>>& distancias) {
    int n = distancias.size();
    if (n == 1) return {{0, 0}, 0, true};
    if (n <= LIMITE_EXACTO) return viajanteExacto(distancias);
    return viajanteAproximado(distancias);
}

#endif