/*
 * Modulo I: Network Intelligence - Flujo Maximo
 * Descripcion: Algoritmo de Edmonds-Karp (BFS) para calcular el flujo maximo
 *              de informacion entre dos centrales en la red troncal.
 * Responsable: Sophia Alexandra Ramos García
 */

#ifndef FLUJO_MAXIMO_H
#define FLUJO_MAXIMO_H

#include <vector>
#include <algorithm>
#include <queue>
#include <limits>
#include <iostream>

using namespace std;

// Estructura que representa una arista en la red residual
struct Arista {
	int destino = -1;
	long long capacidad = 0;
	long long flujo = 0;
	int aristaAnterior = -1;

	/*
	 * Calcula la capacidad residual disponible en la arista.
	 * Parametros: Ninguno.
	 * Regresa:
	 *   - Valor entero de 64 bits con la capacidad residual.
	 */
	long long capacidadResidual() const {
		return capacidad - flujo;
	}
};

// Clase que representa un grafo de flujo para calculo de capacidad maxima
class GrafoFlujo {
private:
	vector<vector<Arista>> adyacencia;
	int numTotalNodos = 0;

public:
	/*
	 * Constructor por defecto de GrafoFlujo.
	 * Parametros: Ninguno.
	 */
	GrafoFlujo() {
		numTotalNodos = 0;
	}

	/*
	 * Constructor parametrizado que asigna la cantidad total de nodos.
	 * Parametros:
	 *   - nodos: Cantidad total de nodos en la red.
	 */
	GrafoFlujo(int nodos) {
		numTotalNodos = nodos;
		adyacencia.resize(numTotalNodos);
	}

	/*
	 * Agrega una arista dirigida con su correspondiente arista residual inversa.
	 * Parametros:
	 *   - origen: Identificador del nodo origen.
	 *   - destino: Identificador del nodo destino.
	 *   - capacidad: Capacidad maxima de transmision de la arista.
	 * Regresa: Sin valor de retorno.
	 */
	void agregarArista(int origen, int destino, long long capacidad) {
		Arista aristaDirecta = { destino, capacidad, 0, static_cast<int>(adyacencia[destino].size()) };
		Arista aristaInversa = { origen, 0, 0, static_cast<int>(adyacencia[origen].size()) };

		adyacencia[origen].push_back(aristaDirecta);
		adyacencia[destino].push_back(aristaInversa);
	}

	/*
	 * Calcula el flujo maximo de informacion entre un nodo origen y un nodo destino (Edmonds-Karp).
	 * Parametros:
	 *   - origen: Nodo fuente de la red.
	 *   - destino: Nodo sumidero de la red.
	 * Regresa:
	 *   - Valor entero de 64 bits con el flujo maximo alcanzable.
	 */
	const vector<vector<Arista>>& obtenerAdyacencia() const {
		return adyacencia;
	}

	long long obtenerFlujoMaximo(int origen, int destino) {
		long long flujoMaximo = 0;
		bool existeCaminoAumento = true;

		while (existeCaminoAumento) {
			vector<pair<int, int>> padre(numTotalNodos, { -1, -1 });
			queue<int> colaNodos;
			bool encontradoDestino = false;

			colaNodos.push(origen);

			while (!colaNodos.empty() && !encontradoDestino) {
				int nodoActual = colaNodos.front();
				colaNodos.pop();

				if (nodoActual == destino) {
					encontradoDestino = true;
				} else {
					size_t totalAdyacentes = adyacencia[nodoActual].size();
					for (size_t i = 0; i < totalAdyacentes; ++i) {
						const Arista& aristaActual = adyacencia[nodoActual][i];

						if (aristaActual.capacidadResidual() > 0 && padre[aristaActual.destino].first == -1 && aristaActual.destino != origen) {
							padre[aristaActual.destino] = { nodoActual, static_cast<int>(i) };
							colaNodos.push(aristaActual.destino);
						}
					}
				}
			}

			if (padre[destino].first == -1) {
				existeCaminoAumento = false;
			} else {
				long long cuelloBotella = numeric_limits<long long>::max();
				int nodoCamino = destino;

				while (nodoCamino != origen) {
					int nodoPadre = padre[nodoCamino].first;
					int indiceArista = padre[nodoCamino].second;
					cuelloBotella = min(cuelloBotella, adyacencia[nodoPadre][indiceArista].capacidadResidual());
					nodoCamino = nodoPadre;
				}

				nodoCamino = destino;
				while (nodoCamino != origen) {
					int nodoPadre = padre[nodoCamino].first;
					int indiceArista = padre[nodoCamino].second;
					int indiceInversa = adyacencia[nodoPadre][indiceArista].aristaAnterior;

					adyacencia[nodoPadre][indiceArista].flujo += cuelloBotella;
					adyacencia[nodoCamino][indiceInversa].flujo -= cuelloBotella;
					nodoCamino = nodoPadre;
				}

				flujoMaximo += cuelloBotella;
			}
		}

		return flujoMaximo;
	}
};

#endif // FLUJO_MAXIMO_H
