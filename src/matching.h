#ifndef MATCHING_H
#define MATCHING_H

#include <string>
#include <vector>
#include <utility>

using namespace std;

// Si se encontro la firma y en que posicion
struct ResultadoMatching {
    bool encontrado;
    int posicionInicial;
};

// Tabla de fallo para el algoritmo de KMP, ayuda a evitar comparar caracteres
// que ya sabemos que coinciden. 
inline vector<int> computeFailure(const string& P) {
    int m = P.size();
    vector<int> f(m, 0);
    int j = 0;

    for (int i = 1; i < m; ++i) {
        while (j > 0 && P[i] != P[j]) {
            j = f[j - 1];
        }
        if (P[i] == P[j]) {
            ++j;
        }
        f[i] = j;
    }

    return f;
}

// Algoritmo KMP para encontrar la primera coincidencia de una firma
inline ResultadoMatching KMP(const string& T, const string& P) {
    int n = T.size();
    int m = P.size();

    auto f = computeFailure(P);
    int j = 0;

    for (int i = 0; i < n; ++i) {
        while (j > 0 && T[i] != P[j]) {
            j = f[j - 1];
        }
        if (T[i] == P[j]) {
            ++j;
        }
        if (j == m) {
            return {true, i - m + 1};
        } // Aqui es en donde se obtiene la primera coincidencia
    }

    return {false, -1};
}

// Buscar las firmas en las dos transmisiones y devolver los resultados
inline vector<ResultadoMatching> todasFirmas(const string& T1, const string& T2, const vector<string>& firmas) {
    vector<ResultadoMatching> resultados;

    for (int i = 0; i < (int)firmas.size(); ++i) {
        resultados.push_back(KMP(T1, firmas[i]));
    }

    for (int i = 0; i < (int)firmas.size(); ++i) {
        resultados.push_back(KMP(T2, firmas[i]));
    }
    return resultados;
}


#endif 