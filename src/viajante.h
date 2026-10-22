#ifndef VIAJANTE_H
#define VIAJANTE_H

#include <algorithm>
#include <limits>
#include <queue>
#include <vector>

#include "grafo.h"

using namespace std;

const double INF = numeric_limits<double>::infinity();

// Hasta este número de nodos se usa el algoritmo exacto.
// Memoria de Held-Karp: 2^16 * 16 * (8 + 4) bytes ≈ 12 MB.
const int LIMITE_EXACTO = 16;

struct ResultadoTSP {
    vector<int> ruta;   // Empieza y termina en el nodo inicial
    double costoTotal = 0;
    bool esOptimo = false;
    bool factible = true;
};

// Matriz de distancias entre TODOS los pares de nodos.
// Si dos centrales no tienen enlace directo, se usa el camino más corto (Dijkstra desde cada nodo).
// Complejidad: O(N * E log N).
inline vector<vector<double>> distanciasEntreTodos(const Red& red) {
    int n = red.numNodos();
    vector<vector<pair<int, double>>> vecinos(n);
    for (const Enlace& e : red.enlaces) {
        vecinos[e.origen].push_back({e.destino, e.costo});
        vecinos[e.destino].push_back({e.origen, e.costo});
    }

    vector<vector<double>> dist(n, vector<double>(n, INF));
    for (int s = 0; s < n; ++s) {
        priority_queue<pair<double, int>, vector<pair<double, int>>, greater<>> cola;
        dist[s][s] = 0;
        cola.push({0, s});
        while (!cola.empty()) {
            auto [d, u] = cola.top();
            cola.pop();
            if (d > dist[s][u]) continue;
            for (auto [v, w] : vecinos[u]) {
                if (d + w < dist[s][v]) {
                    dist[s][v] = d + w;
                    cola.push({dist[s][v], v});
                }
            }
        }
    }
    return dist;
}

inline double costoRuta(const vector<vector<double>>& dist, const vector<int>& ruta) {
    double total = 0;
    for (size_t i = 0; i + 1 < ruta.size(); ++i) total += dist[ruta[i]][ruta[i + 1]];
    return total;
}

// Held-Karp (programación dinámica con máscaras de bits). Da la ruta ÓPTIMA.
// dp[mascara][u] = costo mínimo de salir de "inicio", visitar las colonias de "mascara"
// y estar parado en u. Complejidad: O(2^N * N^2) tiempo, O(2^N * N) memoria.
inline ResultadoTSP tspExacto(const vector<vector<double>>& dist, int inicio) {
    int n = dist.size();
    int totalMascaras = 1 << n;
    vector<vector<double>> dp(totalMascaras, vector<double>(n, INF));
    vector<vector<int>> anterior(totalMascaras, vector<int>(n, -1));

    dp[1 << inicio][inicio] = 0;

    for (int mascara = 0; mascara < totalMascaras; ++mascara) {
        if (!(mascara & (1 << inicio))) continue;
        for (int u = 0; u < n; ++u) {
            if (!(mascara & (1 << u)) || dp[mascara][u] == INF) continue;
            for (int v = 0; v < n; ++v) {
                if (mascara & (1 << v)) continue;
                int nueva = mascara | (1 << v);
                double costo = dp[mascara][u] + dist[u][v];
                if (costo < dp[nueva][v]) {
                    dp[nueva][v] = costo;
                    anterior[nueva][v] = u;
                }
            }
        }
    }

    ResultadoTSP resultado;
    resultado.esOptimo = true;
    int todas = totalMascaras - 1;
    int ultima = -1;
    double mejor = INF;
    for (int u = 0; u < n; ++u) {
        if (u == inicio || dp[todas][u] == INF) continue;
        double costo = dp[todas][u] + dist[u][inicio];
        if (costo < mejor) {
            mejor = costo;
            ultima = u;
        }
    }
    if (ultima == -1) {
        resultado.factible = false;
        return resultado;
    }

    int mascara = todas;
    for (int actual = ultima; actual != -1;) {
        resultado.ruta.push_back(actual);
        int previo = anterior[mascara][actual];
        mascara ^= (1 << actual);
        actual = previo;
    }
    reverse(resultado.ruta.begin(), resultado.ruta.end());
    resultado.ruta.push_back(inicio);
    resultado.costoTotal = mejor;
    return resultado;
}

// Vecino más cercano + 2-opt. Rápido, NO garantiza el óptimo.
// O(N^2) la construcción, O(N^2) cada pasada de 2-opt.
inline ResultadoTSP tspAproximado(const vector<vector<double>>& dist, int inicio) {
    int n = dist.size();
    ResultadoTSP resultado;
    vector<bool> visitado(n, false);
    vector<int>& ruta = resultado.ruta;
    ruta.push_back(inicio);
    visitado[inicio] = true;

    int actual = inicio;
    for (int paso = 1; paso < n; ++paso) {
        int siguiente = -1;
        for (int v = 0; v < n; ++v) {
            if (!visitado[v] && dist[actual][v] < INF &&
                (siguiente == -1 || dist[actual][v] < dist[actual][siguiente])) {
                siguiente = v;
            }
        }
        if (siguiente == -1) {
            resultado.factible = false;
            return resultado;
        }
        visitado[siguiente] = true;
        ruta.push_back(siguiente);
        actual = siguiente;
    }
    ruta.push_back(inicio);

    // 2-opt: si cambiar a-b y c-d por a-c y b-d acorta la ruta, invertir el tramo
    bool mejoro = true;
    while (mejoro) {
        mejoro = false;
        for (int i = 1; i < n - 1; ++i) {
            for (int j = i + 1; j < n; ++j) {
                int a = ruta[i - 1], b = ruta[i], c = ruta[j], d = ruta[j + 1];
                if (dist[a][c] + dist[b][d] < dist[a][b] + dist[c][d] - 1e-9) {
                    reverse(ruta.begin() + i, ruta.begin() + j + 1);
                    mejoro = true;
                }
            }
        }
    }
    resultado.costoTotal = costoRuta(dist, ruta);
    resultado.factible = resultado.costoTotal < INF;
    return resultado;
}

inline ResultadoTSP resolverTSP(const Red& red, int inicio) {
    int n = red.numNodos();
    ResultadoTSP resultado;
    if (n == 0) {
        resultado.factible = false;
        return resultado;
    }
    if (n == 1) {
        resultado.ruta = {inicio, inicio};
        resultado.esOptimo = true;
        return resultado;
    }
    vector<vector<double>> dist = distanciasEntreTodos(red);
    return n <= LIMITE_EXACTO ? tspExacto(dist, inicio) : tspAproximado(dist, inicio);
}

#endif