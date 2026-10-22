#ifndef COMMON_SUBSTR_H
#define COMMON_SUBSTR_H

#include <string>
#include <vector>

using namespace std;

struct ResultadoSubcadena {
    int posicionInicial;
    int posicionFinal;
};

// Usar programacion dinamica para encontrar la subcadena mas larga que es comun entre dos cadenas.
inline ResultadoSubcadena longestCommonSubstring(const string& S, const string& T) {
    int n = S.size();
    int m = T.size();

    vector<vector<int>> dp(n + 1, vector<int>(m + 1, 0));

    int best = 0, end = 0;
    int bestStartT = 0;

    for (int i = 1; i <= n; ++i) {
        for (int j = 1; j <= m; ++j) {

            if (S[i - 1] == T[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;

                int startS = i - dp[i][j];
                int startT = j - dp[i][j];

                // Guardar la subcadena mas larga y si hay empate se elige la primera en S y luego en T
                if (dp[i][j] > best ||
                    (dp[i][j] == best && best > 0 &&
                    (startS < end - best ||
                    (startS == end - best && startT < bestStartT)))) {

                    best = dp[i][j];
                    end = i;
                    bestStartT = startT;
                }
            }
        }
    }

    // Si no hay en comun
    if (best == 0) {
        return {0, 0};
    }

    return {end - best + 1, end};
}

#endif 