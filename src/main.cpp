#include <iostream>
#include <vector>
#include "arbolExpanMin.h"
#include "viajante.h"

using namespace std;

int main() {
    int n;
    cin >> n;

    vector<vector<int>> distancias(n, vector<int>(n));
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            cin >> distancias[i][j];

    ResultadoMST mst = arbolExpansionMinima(distancias);
    cout << "1. Cableado de fibra optica:" << endl;
    for (auto& c : mst.conexiones) {
        cout << "(" << char('A' + c.first) << ", " << char('A' + c.second) << ")" << endl;
    }

    ResultadoTSP tsp = viajante(distancias);
    cout << "2. Ruta de reparto:" << endl;
    for (int colonia : tsp.ruta) cout << char('A' + colonia) << " ";
    cout << endl;

    return 0;
}