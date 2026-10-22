/*
 * Modulo I: Network Intelligence - Flujo Maximo
 * Descripcion: Implementacion del algoritmo de Dinic (BFS + DFS por niveles)
 *              para el calculo eficiente del flujo maximo en redes troncales.
 * Responsable: Sophia Alexandra Ramos García
 */

#ifndef ALTERNATIVA_FLUJO_MAXIMO
#define ALTERNATIVA_FLUJO_MAXIMO

#include <vector>
#include <queue>
#include <algorithm>
#include <limits>
#include <iostream>

using namespace std;

namespace Sentinel {

// Constante para infinito en capacidad/flujo
const long long INFINITO_FLUJO = numeric_limits<long long>::max();

// Estructura que representa una arista en la red residual para el algoritmo de Dinic
struct AristaDinic {
	int destino = -1;
	long long capacidad = 0;
	long long flujo = 0;
	int aristaInversa = -1;

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

// Clase que representa un grafo de flujo optimizado con el Algoritmo de Dinic
class GrafoFlujoDinic {
private:
	vector<vector<AristaDinic>> adyacencia;
	vector<int> nivelNodo;
	vector<size_t> indicePunteroAdyacente;
	int numTotalNodos = 0;

	/*
	 * Construye el grafo de niveles mediante una busqueda en anchura (BFS).
	 * Parametros:
	 *   - origen: Identificador del nodo fuente.
	 *   - destino: Identificador del nodo sumidero.
	 * Regresa:
	 *   - Valor booleano verdadero si existe un camino residual hacia el nodo destino.
	 */
	bool construirNivelesBFS(int origen, int destino) {
		fill(nivelNodo.begin(), nivelNodo.end(), -1);
		nivelNodo[origen] = 0;

		queue<int> colaNodos;
		colaNodos.push(origen);

		bool procesarCola = !colaNodos.empty();

		while (procesarCola) {
			int nodoActual = colaNodos.front();
			colaNodos.pop();

			size_t totalAdyacentes = adyacencia[nodoActual].size();
			for (size_t i = 0; i < totalAdyacentes; ++i) {
				const AristaDinic& aristaActual = adyacencia[nodoActual][i];

				if (aristaActual.capacidadResidual() > 0 && nivelNodo[aristaActual.destino] == -1) {
					nivelNodo[aristaActual.destino] = nivelNodo[nodoActual] + 1;
					colaNodos.push(aristaActual.destino);
				}
			}

			procesarCola = !colaNodos.empty();
		}

		return nivelNodo[destino] != -1;
	}

	/*
	 * Busca y satura un camino de aumento mediante busqueda en profundidad (DFS).
	 * Parametros:
	 *   - nodoActual: Nodo fuente de la iteracion actual.
	 *   - destino: Nodo sumidero objetivo.
	 *   - flujoPulsado: Flujo acumulado que puede enviarse a traves del camino.
	 * Regresa:
	 *   - Cantidad de flujo efectivamente enviado a traves del camino.
	 */
	long long enviarFlujoDFS(int nodoActual, int destino, long long flujoPulsado) {
		long long flujoRetorno = 0;

		if (nodoActual == destino) {
			flujoRetorno = flujoPulsado;
		} else if (flujoPulsado > 0) {
			size_t totalAdyacentes = adyacencia[nodoActual].size();
			bool buscarSiguienteArista = true;

			while (indicePunteroAdyacente[nodoActual] < totalAdyacentes && buscarSiguienteArista) {
				size_t i = indicePunteroAdyacente[nodoActual];
				AristaDinic& aristaActual = adyacencia[nodoActual][i];

				if (nivelNodo[aristaActual.destino] == nivelNodo[nodoActual] + 1 && aristaActual.capacidadResidual() > 0) {
					long long capacidadPermitida = min(flujoPulsado, aristaActual.capacidadResidual());
					long long flujoProcesado = enviarFlujoDFS(aristaActual.destino, destino, capacidadPermitida);

					if (flujoProcesado > 0) {
						aristaActual.flujo += flujoProcesado;
						int idxInversa = aristaActual.aristaInversa;
						adyacencia[aristaActual.destino][idxInversa].flujo -= flujoProcesado;

						flujoRetorno = flujoProcesado;
						buscarSiguienteArista = false;
					}
				}

				if (buscarSiguienteArista) {
					indicePunteroAdyacente[nodoActual]++;
				}
			}
		}

		return flujoRetorno;
	}

public:
	/*
	 * Constructor por defecto de GrafoFlujoDinic.
	 * Parametros: Ninguno.
	 */
	GrafoFlujoDinic() {
		numTotalNodos = 0;
	}

	/*
	 * Constructor parametrizado que asigna la cantidad total de nodos.
	 * Parametros:
	 *   - nodos: Cantidad total de nodos en la red.
	 */
	GrafoFlujoDinic(int nodos) {
		numTotalNodos = nodos;
		adyacencia.resize(numTotalNodos);
		nivelNodo.resize(numTotalNodos, -1);
		indicePunteroAdyacente.resize(numTotalNodos, 0);
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
		AristaDinic aristaDirecta = { destino, capacidad, 0, static_cast<int>(adyacencia[destino].size()) };
		AristaDinic aristaInversa = { origen, 0, 0, static_cast<int>(adyacencia[origen].size()) };

		adyacencia[origen].push_back(aristaDirecta);
		adyacencia[destino].push_back(aristaInversa);
	}

	/*
	 * Calcula el flujo maximo de informacion entre un nodo origen y un nodo destino mediante Dinic.
	 * Parametros:
	 *   - origen: Nodo fuente de la red.
	 *   - destino: Nodo sumidero de la red.
	 * Regresa:
	 *   - Valor entero de 64 bits con el flujo maximo alcanzable.
	 */
	long long obtenerFlujoMaximo(int origen, int destino) {
		long long flujoMaximoTotal = 0;

		if (origen != destino && numTotalNodos > 0) {
			bool continuarFase = construirNivelesBFS(origen, destino);

			while (continuarFase) {
				fill(indicePunteroAdyacente.begin(), indicePunteroAdyacente.end(), 0);

				long long flujoAdicional = enviarFlujoDFS(origen, destino, INFINITO_FLUJO);
				while (flujoAdicional > 0) {
					flujoMaximoTotal += flujoAdicional;
					flujoAdicional = enviarFlujoDFS(origen, destino, INFINITO_FLUJO);
				}

				continuarFase = construirNivelesBFS(origen, destino);
			}
		}

		return flujoMaximoTotal;
	}
};

} // namespace Sentinel

#endif // ALTERNATIVA_FLUJO_MAXIMO