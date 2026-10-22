/*
 * Modulo I: Network Intelligence - Diagrama de Voronoi
 * Descripcion: Implementacion del Diagrama de Voronoi mediante el dual de Delaunay
 *              para la asignacion eficiente de contrataciones a centrales.
 * Responsable: Sophia Alexandra Ramos Garcia
 */

#ifndef DIAGRAMA_VORONOI_H
#define DIAGRAMA_VORONOI_H

#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <string>

using namespace std;

namespace Sentinel {

// Constantes globales de precision y dominio
const double EPSILON = 0.0000001;
const double EPSILON_GEOMETRICO = 0.000000001;
const double MARGEN_SUPER_TRIANGULO = 10.0;
const double DOMINIO_DEFECTO_MIN = 0.0;
const double DOMINIO_DEFECTO_MAX = 100.0;

// Estructura para coordenadas en un plano 2D (Centrales, Nodos, Contrataciones)
struct Punto2D {
	double x = 0.0;
	double y = 0.0;
	int id = -1;
	string nombre = "";

	/*
	 * Compara la igualdad de dos puntos dentro de un margen de tolerancia.
	 * Parametros:
	 *   - puntoComparar: Punto contra el que se realiza la comparacion.
	 * Regresa:
	 *   - Valor booleano verdadero si las coordenadas coinciden dentro de la tolerancia.
	 */
	bool operator==(const Punto2D& puntoComparar) const {
		return fabs(x - puntoComparar.x) < EPSILON && fabs(y - puntoComparar.y) < EPSILON;
	}

	/*
	 * Define el orden estricto de puntos para permitir ordenamiento en contenedores.
	 * Parametros:
	 *   - puntoComparar: Punto contra el que se compara la posicion.
	 * Regresa:
	 *   - Valor booleano verdadero si el punto actual es menor espacialmente.
	 */
	bool operator<(const Punto2D& puntoComparar) const {
		bool esMenor = false;
		if (fabs(x - puntoComparar.x) > EPSILON) {
			esMenor = x < puntoComparar.x;
		} else {
			esMenor = y < puntoComparar.y - EPSILON;
		}
		return esMenor;
	}

	/*
	 * Calcula la distancia euclidiana al cuadrado entre dos puntos.
	 * Parametros:
	 *   - puntoDestino: Punto con el cual se calcula la distancia.
	 * Regresa:
	 *   - Valor real con la distancia al cuadrado.
	 */
	double distanciaCuadrada(const Punto2D& puntoDestino) const {
		double diferenciaX = x - puntoDestino.x;
		double diferenciaY = y - puntoDestino.y;
		return diferenciaX * diferenciaX + diferenciaY * diferenciaY;
	}

	/*
	 * Calcula la distancia euclidiana real entre dos puntos.
	 * Parametros:
	 *   - puntoDestino: Punto con el cual se calcula la distancia.
	 * Regresa:
	 *   - Valor real con la distancia euclidiana.
	 */
	double distancia(const Punto2D& puntoDestino) const {
		return sqrt(distanciaCuadrada(puntoDestino));
	}
};

// Dominio o Cuadro Delimitador del plano de trabajo
struct BoundingBox {
	double xMin = DOMINIO_DEFECTO_MIN;
	double xMax = DOMINIO_DEFECTO_MAX;
	double yMin = DOMINIO_DEFECTO_MIN;
	double yMax = DOMINIO_DEFECTO_MAX;

	/*
	 * Evalua si un punto se encuentra dentro de los limites del cuadro delimitador.
	 * Parametros:
	 *   - puntoEvaluar: Punto que se desea verificar.
	 * Regresa:
	 *   - Valor booleano verdadero si el punto esta dentro del dominio.
	 */
	bool contiene(const Punto2D& puntoEvaluar) const {
		return puntoEvaluar.x >= xMin - EPSILON && puntoEvaluar.x <= xMax + EPSILON &&
		       puntoEvaluar.y >= yMin - EPSILON && puntoEvaluar.y <= yMax + EPSILON;
	}
};

// Circulo circunscrito auxiliar para la triangulacion
struct CirculoCircunscrito {
	Punto2D centro = { 0.0, 0.0, -1, "" };
	double radio = 0.0;
};

// Triangulo de la Triangulacion de Delaunay
struct TrianguloDelaunay {
	Punto2D a = { 0.0, 0.0, -1, "" };
	Punto2D b = { 0.0, 0.0, -1, "" };
	Punto2D c = { 0.0, 0.0, -1, "" };

	/*
	 * Verifica si un punto dado es uno de los tres vertices del triangulo.
	 * Parametros:
	 *   - puntoVerificar: Punto a consultar.
	 * Regresa:
	 *   - Valor booleano verdadero si el punto coincide con alguno de los vertices.
	 */
	bool tieneVertice(const Punto2D& puntoVerificar) const {
		return (a == puntoVerificar || b == puntoVerificar || c == puntoVerificar);
	}

	/*
	 * Obtiene el centro y radio del circulo circunscrito del triangulo.
	 * Parametros: Ninguno.
	 * Regresa:
	 *   - Estructura CirculoCircunscrito con el centro y radio calculados.
	 */
	CirculoCircunscrito obtenerCircuncirculo() const {
		CirculoCircunscrito resultado;
		double denominador = 2.0 * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));

		if (fabs(denominador) < EPSILON_GEOMETRICO) {
			resultado.centro = { 0.0, 0.0, -1, "Invalido" };
			resultado.radio = numeric_limits<double>::infinity();
		} else {
			double ux = ((a.x * a.x + a.y * a.y) * (b.y - c.y) +
			             (b.x * b.x + b.y * b.y) * (c.y - a.y) +
			             (c.x * c.x + c.y * c.y) * (a.y - b.y)) / denominador;
			double uy = ((a.x * a.x + a.y * a.y) * (c.x - b.x) +
			             (b.x * b.x + b.y * b.y) * (a.x - c.x) +
			             (c.x * c.x + c.y * c.y) * (b.x - a.x)) / denominador;

			Punto2D puntoCentro = { ux, uy, -1, "Circuncentro" };
			resultado.centro = puntoCentro;
			resultado.radio = puntoCentro.distancia(a);
		}

		return resultado;
	}

	/*
	 * Determina si un punto se encuentra dentro del circulo circunscrito del triangulo.
	 * Parametros:
	 *   - puntoConsulta: Punto a evaluar.
	 * Regresa:
	 *   - Valor booleano verdadero si el punto esta dentro del circuncirculo.
	 */
	bool enCircuncirculo(const Punto2D& puntoConsulta) const {
		CirculoCircunscrito circulo = obtenerCircuncirculo();
		return puntoConsulta.distancia(circulo.centro) <= circulo.radio + EPSILON;
	}
};

// Arista para conectar la cavidad poligonal de Delaunay
struct AristaDelaunay {
	Punto2D p1 = { 0.0, 0.0, -1, "" };
	Punto2D p2 = { 0.0, 0.0, -1, "" };

	/*
	 * Evalua la igualdad entre dos aristas sin importar el orden de sus puntos.
	 * Parametros:
	 *   - aristaComparar: Arista con la que se compara.
	 * Regresa:
	 *   - Valor booleano verdadero si representan el mismo segmento de recta.
	 */
	bool operator==(const AristaDelaunay& aristaComparar) const {
		return (p1 == aristaComparar.p1 && p2 == aristaComparar.p2) ||
		       (p1 == aristaComparar.p2 && p2 == aristaComparar.p1);
	}
};

// Celda o Region de Voronoi asociada a una central
struct CeldaVoronoi {
	int idCentral = -1;
	string nombreCentral = "";
	Punto2D sitioCentral = { 0.0, 0.0, -1, "" };
	vector<Punto2D> verticesPoligono;
};

// Clase Principal: Diagrama de Voronoi
class DiagramaVoronoi {
private:
	vector<Punto2D> centrales;
	BoundingBox dominio;
	vector<TrianguloDelaunay> triangulacionDelaunay;
	vector<CeldaVoronoi> regiones;

	/*
	 * Construye la triangulacion de Delaunay utilizando el algoritmo de Bowyer-Watson.
	 * Parametros: Ninguno.
	 * Regresa: Sin valor de retorno.
	 */
	void construirDelaunay() {
		triangulacionDelaunay.clear();

		if (!centrales.empty()) {
			double margenAncho = max(dominio.xMax - dominio.xMin, dominio.yMax - dominio.yMin) * MARGEN_SUPER_TRIANGULO;
			Punto2D puntoSuperUno = { (dominio.xMin + dominio.xMax) / 2.0, dominio.yMax + margenAncho, -999, "SUPER_1" };
			Punto2D puntoSuperDos = { dominio.xMin - margenAncho, dominio.yMin - margenAncho, -998, "SUPER_2" };
			Punto2D puntoSuperTres = { dominio.xMax + margenAncho, dominio.yMin - margenAncho, -997, "SUPER_3" };

			triangulacionDelaunay.push_back({ puntoSuperUno, puntoSuperDos, puntoSuperTres });

			size_t totalCentrales = centrales.size();
			for (size_t i = 0; i < totalCentrales; ++i) {
				Punto2D puntoActual = centrales[i];
				vector<TrianguloDelaunay> triangulosMalos;

				size_t totalTriangulos = triangulacionDelaunay.size();
				for (size_t j = 0; j < totalTriangulos; ++j) {
					if (triangulacionDelaunay[j].enCircuncirculo(puntoActual)) {
						triangulosMalos.push_back(triangulacionDelaunay[j]);
					}
				}

				vector<AristaDelaunay> poligono;
				size_t totalMalos = triangulosMalos.size();
				for (size_t j = 0; j < totalMalos; ++j) {
					TrianguloDelaunay trianguloUno = triangulosMalos[j];
					vector<AristaDelaunay> aristasTrianguloUno = {
						{ trianguloUno.a, trianguloUno.b },
						{ trianguloUno.b, trianguloUno.c },
						{ trianguloUno.c, trianguloUno.a }
					};

					for (size_t k = 0; k < 3; ++k) {
						AristaDelaunay aristaEvaluar = aristasTrianguloUno[k];
						bool compartida = false;

						size_t indiceMalo = 0;
						while (indiceMalo < totalMalos && !compartida) {
							if (indiceMalo != j) {
								TrianguloDelaunay trianguloDos = triangulosMalos[indiceMalo];
								vector<AristaDelaunay> aristasTrianguloDos = {
									{ trianguloDos.a, trianguloDos.b },
									{ trianguloDos.b, trianguloDos.c },
									{ trianguloDos.c, trianguloDos.a }
								};

								size_t indiceArista = 0;
								while (indiceArista < 3 && !compartida) {
									if (aristaEvaluar == aristasTrianguloDos[indiceArista]) {
										compartida = true;
									}
									++indiceArista;
								}
							}
							++indiceMalo;
						}

						if (!compartida) {
							poligono.push_back(aristaEvaluar);
						}
					}
				}

				vector<TrianguloDelaunay> nuevaTriangulacion;
				for (size_t j = 0; j < totalTriangulos; ++j) {
					TrianguloDelaunay trianguloEvaluar = triangulacionDelaunay[j];
					bool esMalo = false;

					size_t indiceMalo = 0;
					while (indiceMalo < totalMalos && !esMalo) {
						if (trianguloEvaluar.a == triangulosMalos[indiceMalo].a &&
						    trianguloEvaluar.b == triangulosMalos[indiceMalo].b &&
						    trianguloEvaluar.c == triangulosMalos[indiceMalo].c) {
							esMalo = true;
						}
						++indiceMalo;
					}

					if (!esMalo) {
						nuevaTriangulacion.push_back(trianguloEvaluar);
					}
				}
				triangulacionDelaunay = nuevaTriangulacion;

				size_t totalAristasPoligono = poligono.size();
				for (size_t j = 0; j < totalAristasPoligono; ++j) {
					triangulacionDelaunay.push_back({ poligono[j].p1, poligono[j].p2, puntoActual });
				}
			}

			vector<TrianguloDelaunay> resultadoFinal;
			size_t totalResultantes = triangulacionDelaunay.size();
			for (size_t i = 0; i < totalResultantes; ++i) {
				TrianguloDelaunay trianguloRevisar = triangulacionDelaunay[i];
				if (!trianguloRevisar.tieneVertice(puntoSuperUno) &&
				    !trianguloRevisar.tieneVertice(puntoSuperDos) &&
				    !trianguloRevisar.tieneVertice(puntoSuperTres)) {
					resultadoFinal.push_back(trianguloRevisar);
				}
			}
			triangulacionDelaunay = resultadoFinal;
		}
	}

	/*
	 * Genera las celdas poligonales de Voronoi calculando los circuncentros de los triangulos de Delaunay.
	 * Parametros: Ninguno.
	 * Regresa: Sin valor de retorno.
	 */
	void construirVoronoi() {
		regiones.clear();

		size_t totalCentrales = centrales.size();
		for (size_t i = 0; i < totalCentrales; ++i) {
			Punto2D centralActual = centrales[i];
			CeldaVoronoi celda;
			celda.idCentral = centralActual.id;
			celda.nombreCentral = centralActual.nombre;
			celda.sitioCentral = centralActual;

			vector<Punto2D> circuncentros;
			size_t totalTriangulos = triangulacionDelaunay.size();
			for (size_t j = 0; j < totalTriangulos; ++j) {
				if (triangulacionDelaunay[j].tieneVertice(centralActual)) {
					Punto2D centroCirculo = triangulacionDelaunay[j].obtenerCircuncirculo().centro;
					circuncentros.push_back(centroCirculo);
				}
			}

			sort(circuncentros.begin(), circuncentros.end(), [&centralActual](const Punto2D& puntoUno, const Punto2D& puntoDos) {
				double anguloUno = atan2(puntoUno.y - centralActual.y, puntoUno.x - centralActual.x);
				double anguloDos = atan2(puntoDos.y - centralActual.y, puntoDos.x - centralActual.x);
				return anguloUno < anguloDos;
			});

			celda.verticesPoligono = circuncentros;
			regiones.push_back(celda);
		}
	}

public:
	/*
	 * Constructor por defecto. Inicializa el dominio con valores predeterminados.
	 * Parametros: Ninguno.
	 */
	DiagramaVoronoi() {
		dominio = { DOMINIO_DEFECTO_MIN, DOMINIO_DEFECTO_MAX, DOMINIO_DEFECTO_MIN, DOMINIO_DEFECTO_MAX };
	}

	/*
	 * Constructor parametrizado. Construye el diagrama a partir de las centrales y un dominio dado.
	 * Parametros:
	 *   - sitios: Vector con los puntos de servicio/centrales.
	 *   - cuadroDelimitador: Dominio espacial de operacion.
	 */
	DiagramaVoronoi(const vector<Punto2D>& sitios, BoundingBox cuadroDelimitador) {
		centrales = sitios;
		dominio = cuadroDelimitador;
		construirDelaunay();
		construirVoronoi();
	}

	/*
	 * Inicializa o reconfigura el diagrama de Voronoi con nuevas centrales y dominio.
	 * Parametros:
	 *   - sitios: Vector con las nuevas centrales.
	 *   - cuadroDelimitador: Nuevo dominio espacial de operacion.
	 * Regresa: Sin valor de retorno.
	 */
	void inicializar(const vector<Punto2D>& sitios, BoundingBox cuadroDelimitador) {
		centrales = sitios;
		dominio = cuadroDelimitador;
		construirDelaunay();
		construirVoronoi();
	}

	/*
	 * Consulta la central de servicio mas cercana para una ubicacion dada (Consulta de proximidad).
	 * Parametros:
	 *   - ubicacion: Coordenada de la nueva contratacion o punto de consulta.
	 * Regresa:
	 *   - Objeto Punto2D correspondiente a la central mas cercana.
	 */
	Punto2D consultarCentralMasCercana(const Punto2D& ubicacion) const {
		Punto2D centralResultado = { 0.0, 0.0, -1, "NULA" };

		if (!centrales.empty()) {
			size_t mejorIndice = 0;
			double distanciaMinimaCuadrada = numeric_limits<double>::max();

			size_t totalCentrales = centrales.size();
			for (size_t i = 0; i < totalCentrales; ++i) {
				double distanciaActualCuadrada = ubicacion.distanciaCuadrada(centrales[i]);
				if (distanciaActualCuadrada < distanciaMinimaCuadrada) {
					distanciaMinimaCuadrada = distanciaActualCuadrada;
					mejorIndice = i;
				}
			}
			centralResultado = centrales[mejorIndice];
		}

		return centralResultado;
	}

	/*
	 * Devuelve las regiones poligonales de Voronoi generadas.
	 * Parametros: Ninguno.
	 * Regresa:
	 *   - Referencia constante al vector de celdas de Voronoi.
	 */
	const vector<CeldaVoronoi>& obtenerRegiones() const {
		return regiones;
	}

	/*
	 * Devuelve el dominio o BoundingBox de operacion.
	 * Parametros: Ninguno.
	 * Regresa:
	 *   - Estructura BoundingBox actual.
	 */
	BoundingBox obtenerBoundingBox() const {
		return dominio;
	}
};

}

#endif // DIAGRAMA_VORONOI_H