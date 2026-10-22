#ifndef ARBOL_EXPAN_MIN_H
#define ARBOL_EXPAN_MIN_H

#include <algorithm>
#include <numeric>
#include <queue>
#include <vector>

#include "grafo.h"

using namespace std;

// Resultado del despliegue: enlaces elegidos y costo total
struct ResultadoMST {
    vector<Enlace> enlaces;
    double costoTotal = 0;
    bool conexo = true;
};

// Union-Find: dice en qué "componente" está cada nodo.
// Con compresión de caminos y unión por rango cada operación es casi O(1).
class UnionFind {
private:
    vector<int> padre;
    vector<int> rango;

public:
    UnionFind(int n) : padre(n), rango(n, 0) {
        iota(padre.begin(), padre.end(), 0);
    }

    int encontrar(int x) {
        while (padre[x] != x) {
            padre[x] = padre[padre[x]];
            x = padre[x];
        }
        return x;
    }

    // Une dos componentes. Regresa false si ya estaban unidos (formaría ciclo).
    bool unir(int a, int b) {
        a = encontrar(a);
        b = encontrar(b);
        if (a == b) return false;
        if (rango[a] < rango[b]) swap(a, b);
        padre[b] = a;
        if (rango[a] == rango[b]) ++rango[a];
        return true;
    }
};

// Kruskal: ordenar enlaces de más barato a más caro y tomar cada uno
// si no forma ciclo. O(E log E). Es el que usamos porque la red es dispersa (E ~ N).
inline ResultadoMST mstKruskal(const Red& red) {
    int n = red.numNodos();
    ResultadoMST resultado;

    vector<Enlace> ordenados = red.enlaces;
    // stable_sort: si hay empate en costo, se respeta el orden de entrada (resultado reproducible)
    stable_sort(ordenados.begin(), ordenados.end(),
                [](const Enlace& a, const Enlace& b) { return a.costo < b.costo; });

    UnionFind componentes(n);
    for (const Enlace& e : ordenados) {
        if (e.origen != e.destino && componentes.unir(e.origen, e.destino)) {
            resultado.enlaces.push_back(e);
            resultado.costoTotal += e.costo;
        }
    }
    resultado.conexo = n <= 1 || (int)resultado.enlaces.size() == n - 1;
    return resultado;
}

// Prim con cola de prioridad: crecer el árbol desde un nodo tomando siempre
// el enlace más barato que sale de él. O(E log V). Segunda técnica para comparar.
inline ResultadoMST mstPrim(const Red& red) {
    int n = red.numNodos();
    ResultadoMST resultado;
    if (n == 0) return resultado;

    // Lista de adyacencia: para cada nodo, {vecino, índice del enlace}
    vector<vector<pair<int, int>>> vecinos(n);
    for (int i = 0; i < (int)red.enlaces.size(); ++i) {
        vecinos[red.enlaces[i].origen].push_back({red.enlaces[i].destino, i});
        vecinos[red.enlaces[i].destino].push_back({red.enlaces[i].origen, i});
    }

    vector<bool> enArbol(n, false);
    // Cola: {costo, índice del enlace, nodo al que llega}. greater = sale el más barato.
    priority_queue<tuple<double, int, int>, vector<tuple<double, int, int>>, greater<>> cola;

    for (int inicio = 0; inicio < n; ++inicio) { // Por si la red no es conexa
        if (enArbol[inicio]) continue;
        if (inicio > 0) resultado.conexo = false;
        enArbol[inicio] = true;
        for (auto [v, idx] : vecinos[inicio]) cola.push({red.enlaces[idx].costo, idx, v});

        while (!cola.empty()) {
            auto [costo, idx, u] = cola.top();
            cola.pop();
            if (enArbol[u]) continue; // Ya estaba conectado: este enlace formaría ciclo
            enArbol[u] = true;
            resultado.enlaces.push_back(red.enlaces[idx]);
            resultado.costoTotal += costo;
            for (auto [v, siguiente] : vecinos[u]) {
                if (!enArbol[v]) cola.push({red.enlaces[siguiente].costo, siguiente, v});
            }
        }
    }
    return resultado;
}

#endif