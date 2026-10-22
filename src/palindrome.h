#ifndef PALINDROME_H
#define PALINDROME_H

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;

struct ResultadoPalindromo {
    int posicionInicial;
    int posicionFinal;
};

// Se agrega # entre los caracteres para identificar si la longitud de los palindromos es par o impar. 
inline string preprocess(const string& s) {
    string t = "#";
    
    for (char c : s) {
        t += c;
        t += "#";
    }
    return t;
}

// Algoritmo de Manacher para encontrar el palíndromo más largo
inline ResultadoPalindromo manacher(const string& s) {
    if (s.empty()) {
        return {0, 0};
    }

    string T = preprocess(s);
    int n = T.size();

    vector<int> P(n, 0);
    int C = 0, R = 0, maxLen = 0, centerIdx = 0;
    for (int i = 0; i < n; ++i) {
        int mir = 2 * C - i;

        // Si i está dentro del palíndromo actual, se puede usar la información de P[mir] para inicializar P[i]
        // Se utiliza el radio del palíndromo espejo para evitar comparaciones innecesarias.
        if (i < R) {
            P[i] = min(R - i, P[mir]);
        }

        // Intentar expandir el palindromo mientras los caracteres sean iguales
        while (i + P[i] + 1 < n && i - P[i] - 1 >= 0 && T[i + P[i] + 1] == T[i - P[i] - 1]) {
            ++P[i];
        }

        // Se actualiza el centro y el limite derecho
        if (i + P[i] > R) {
            C = i;
            R = i + P[i];
        }

        // Guardar el palindromo mas largo
        if (P[i] > maxLen) {
            maxLen = P[i];
            centerIdx = i;
        }
    }
    // Hacer las posiciones del texto transformado a las posiciones del texto original
    int start = (centerIdx - maxLen) / 2;

    return {start + 1, start + maxLen};
}

// Manacher para las dos transmisiones
inline vector<ResultadoPalindromo> analizarPalindromos(
    const string& T1, const string& T2) {

    vector<ResultadoPalindromo> resultados;

    resultados.push_back(manacher(T1));
    resultados.push_back(manacher(T2));

    return resultados;
}

#endif 