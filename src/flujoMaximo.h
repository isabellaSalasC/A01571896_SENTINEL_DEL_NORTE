#include <vector>
#include <algorithm>
#include <queue>
#include <climits>
#include <iostream>

using namespace std;

struct Arista {
    int destino;
    int capacidad;
    int flujo;
    int aristaAnterior;

    int capacidadResidual() const {
        return capacidad - flujo;
    }
};

class grafoFlujo {
private:
    vector<vector<Arista>> adyacencia;
    int numTotalNodos;

public:
    grafoFlujo(int nodos) {
        numTotalNodos = nodos;
        adyacencia.resize(numTotalNodos);
    }

    void agregarArista(int origen, int destino, int capacidad) {
        // Arista original con la capacidad dada
        Arista dir = {destino, capacidad, 0, static_cast<int>(adyacencia[destino].size())};

        // Arista residual inversa con capacidad 0
        Arista inv = {origen, 0, 0, static_cast<int>(adyacencia[origen].size())};

        adyacencia[origen].push_back(dir);
        adyacencia[destino].push_back(inv);
    }

    int obtenerFlujoMaximo(int origen, int destino) {
        int flujoMaximo = 0;

        //Edmonds-Karp (BFS)
        while (true) {
            // Guarda para cada nodo: {nodo_padre, indice_de_arista_usada}
            vector<pair<int, int>> padre(numTotalNodos, {-1, -1});
            queue<int> q;

            q.push(origen);

            // BFS para hallar el camino más corto al destino
            while (!q.empty()) {
                int u = q.front();
                q.pop();

                if (u == destino) break;

                for (size_t i = 0; i < adyacencia[u].size(); ++i) {
                    const Arista& e = adyacencia[u][i];
                    
                    if (e.capacidadResidual() > 0 && padre[e.destino].first == -1 && e.destino != origen) {
                        padre[e.destino] = {u, static_cast<int>(i)};
                        q.push(e.destino);
                    }
                }
            }

            // Si el destino no es alcanzable, finaliza la búsqueda de flujo
            if (padre[destino].first == -1) break;

            //Encuentra la capacidad residual mínima (cuello de botella) del camino
            int cuelloBotella = INT_MAX;
            for (int v = destino; v != origen; v = padre[v].first) {
                int u = padre[v].first;
                int idx = padre[v].second;
                cuelloBotella = min(cuelloBotella, adyacencia[u][idx].capacidadResidual());
            }

            //Actualiza flujos de arista directa e inversa
            for (int v = destino; v != origen; v = padre[v].first) {
                int u = padre[v].first;
                int idx = padre[v].second;
                int revIdx = adyacencia[u][idx].aristaAnterior;

                adyacencia[u][idx].flujo += cuelloBotella;
                adyacencia[v][revIdx].flujo -= cuelloBotella;
            }

            flujoMaximo += cuelloBotella;
        }

        return flujoMaximo;
    }
};