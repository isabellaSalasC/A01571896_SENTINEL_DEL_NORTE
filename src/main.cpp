// SENTINELA DEL NORTE · TC2038
// Contrato: lee UN objeto JSON de stdin ({"mode":"...","input":{...}})
// y escribe UN objeto JSON en stdout.

#include <iostream>
#include <iterator>
#include <string>

#include "arbolExpanMin.h"
#include "grafo.h"
#include "json.h"
#include "viajante.h"
#include "matching.h"
#include "palindrome.h"
#include "common_substr.h"
#include "flujoMaximo.h"
#include "alternativaFlujoMaximo.h"
#include "diagramaVoronoi.h"


using namespace std;

// ---------- Módulo I · MST ----------
string resolverMST(const ValorJson& entrada) {
    LectorRed lector;
    Red red = lector.leer(entrada);

    // Kruskal por defecto; "algorithm":"prim" para comparar
    const ValorJson* algoritmo = buscarClave(entrada, {"algorithm", "algoritmo", "method"});
    bool usarPrim = algoritmo && algoritmo->texto == "prim";
    ResultadoMST mst = usarPrim ? mstPrim(red) : mstKruskal(red);

    string salida = "{\"edges\":[";
    for (size_t i = 0; i < mst.enlaces.size(); ++i) {
        const Enlace& e = mst.enlaces[i];
        salida += (i ? "," : "");
        salida += "{\"u\":" + escribirJson(red.idsNodos[e.origen]) +
                  ",\"v\":" + escribirJson(red.idsNodos[e.destino]) +
                  ",\"cost\":" + numeroJson(e.costo) + "}";
    }
    salida += "],\"cost\":" + numeroJson(mst.costoTotal);
    salida += ",\"connected\":" + string(mst.conexo ? "true" : "false");
    salida += ",\"algorithm\":\"" + string(usarPrim ? "prim" : "kruskal") + "\"}";
    return salida;
}

// ---------- Módulo I · TSP ----------
string resolverTSPJson(const ValorJson& entrada) {
    LectorRed lector;
    Red red = lector.leer(entrada);

    int inicio = 0;
    const ValorJson* origen = buscarClave(entrada, {"start", "origin", "source", "origen", "inicio"});
    if (origen) {
        inicio = lector.buscarIndice(*origen);
        if (inicio == -1) throw runtime_error("Nodo inicial no existe");
    }

    ResultadoTSP tsp = resolverTSP(red, inicio);
    if (!tsp.factible) return "{\"error\":\"No existe una ruta que visite todos los nodos\"}";

    string salida = "{\"route\":[";
    for (size_t i = 0; i < tsp.ruta.size(); ++i) {
        salida += (i ? "," : "") + escribirJson(red.idsNodos[tsp.ruta[i]]);
    }
    salida += "],\"cost\":" + numeroJson(tsp.costoTotal);
    salida += ",\"exact\":" + string(tsp.esOptimo ? "true" : "false") + "}";
    return salida;
}
// ---------- Módulo I · Flujo maximo ----------
string resolverMaxFlow(const ValorJson& entrada) {
    LectorRed lector;
    Red red = lector.leer(entrada);

    int totalNodos = red.numNodos();

    int fuente = 0;
    const ValorJson* origen = buscarClave(entrada, {"source", "start", "origin", "origen", "fuente", "inicio"});
    if (origen) {
        fuente = lector.buscarIndice(*origen);
        if (fuente == -1) {
            throw runtime_error("Nodo fuente/origen no existe");
        }
    }

    int sumidero = totalNodos - 1;
    const ValorJson* destino = buscarClave(entrada, {"target", "destination", "sink", "destino", "sumidero", "fin"});
    if (destino) {
        sumidero = lector.buscarIndice(*destino);
        if (sumidero == -1) {
            throw runtime_error("Nodo destino/sumidero no existe");
        }
    }

    size_t totalEnlaces = red.enlaces.size();

    // Instanciacion de ambas redes
    GrafoFlujo redEK(totalNodos);
    Sentinel::GrafoFlujoDinic redDinic(totalNodos);

    for (size_t i = 0; i < totalEnlaces; ++i) {
        const Enlace& e = red.enlaces[i];
        long long cap = static_cast<long long>(e.capacidad);
        redEK.agregarArista(e.origen, e.destino, cap);
        redDinic.agregarArista(e.origen, e.destino, cap);
    }

    // Calculo de flujo maximo en ambos algoritmos
    long long flujoEK = redEK.obtenerFlujoMaximo(fuente, sumidero);
    long long flujoDinic = redDinic.obtenerFlujoMaximo(fuente, sumidero);

    bool coinciden = (flujoEK == flujoDinic);

    return "{\"max_flow\":" + to_string(flujoDinic) +
           ",\"edmonds_karp_flow\":" + to_string(flujoEK) +
           ",\"dinic_flow\":" + to_string(flujoDinic) +
           ",\"match\":" + string(coinciden ? "true" : "false") +
           ",\"source\":" + escribirJson(red.idsNodos[fuente]) +
           ",\"target\":" + escribirJson(red.idsNodos[sumidero]) + "}";
}

// ---------- Módulo I · Voronoi ----------
string resolverVoronoi(const ValorJson& entrada) {
    
    const ValorJson* listaSitios = buscarClave(entrada, {"sites", "centrales", "nodes", "nodos", "points", "puntos"});
    if (!listaSitios || !listaSitios->esLista()) {
        throw runtime_error("Falta la lista de centrales/sitios en el input");
    }

    vector<Sentinel::Punto2D> sitios;
    double xMin = numeric_limits<double>::max();
    double xMax = -numeric_limits<double>::max();
    double yMin = numeric_limits<double>::max();
    double yMax = -numeric_limits<double>::max();

    size_t totalSitios = listaSitios->lista.size();
    for (size_t i = 0; i < totalSitios; ++i) {
        const ValorJson& item = listaSitios->lista[i];
        Sentinel::Punto2D p;
        p.id = static_cast<int>(i);

        const ValorJson* idVal = buscarClave(item, {"id", "name", "nombre"});
        p.nombre = (idVal) ? idVal->texto : ("C" + to_string(i));

        const ValorJson* xVal = buscarClave(item, {"x", "lat", "latitude", "px"});
        const ValorJson* yVal = buscarClave(item, {"y", "lon", "longitude", "py"});

        if (xVal && yVal) {
            p.x = xVal->numero;
            p.y = yVal->numero;
        } else if (item.esLista() && item.lista.size() >= 2) {
            p.x = item.lista[0].numero;
            p.y = item.lista[1].numero;
        } else {
            throw runtime_error("Coordenadas invalidas en nodo/sitio");
        }

        if (p.x < xMin) xMin = p.x;
        if (p.x > xMax) xMax = p.x;
        if (p.y < yMin) yMin = p.y;
        if (p.y > yMax) yMax = p.y;

        sitios.push_back(p);
    }

    Sentinel::BoundingBox bbox;
    const ValorJson* bboxJson = buscarClave(entrada, {"bbox", "bounding_box", "dominio"});
    if (bboxJson && bboxJson->esObjeto()) {
        const ValorJson* xMinVal = buscarClave(*bboxJson, {"xMin", "xmin", "min_x"});
        const ValorJson* xMaxVal = buscarClave(*bboxJson, {"xMax", "xmax", "max_x"});
        const ValorJson* yMinVal = buscarClave(*bboxJson, {"yMin", "ymin", "min_y"});
        const ValorJson* yMaxVal = buscarClave(*bboxJson, {"yMax", "ymax", "max_y"});

        if (xMinVal) bbox.xMin = xMinVal->numero;
        if (xMaxVal) bbox.xMax = xMaxVal->numero;
        if (yMinVal) bbox.yMin = yMinVal->numero;
        if (yMaxVal) bbox.yMax = yMaxVal->numero;
    } else {
        double margenX = max(1.0, (xMax - xMin) * 0.2);
        double margenY = max(1.0, (yMax - yMin) * 0.2);
        bbox.xMin = xMin - margenX;
        bbox.xMax = xMax + margenX;
        bbox.yMin = yMin - margenY;
        bbox.yMax = yMax + margenY;
    }

    Sentinel::DiagramaVoronoi voronoi(sitios, bbox);

    const ValorJson* consultas = buscarClave(entrada, {"queries", "contrataciones", "consultas", "puntos_consulta"});
    string salidaAsignaciones = "[";
    if (consultas && consultas->esLista()) {
        size_t totalConsultas = consultas->lista.size();
        for (size_t i = 0; i < totalConsultas; ++i) {
            const ValorJson& item = consultas->lista[i];
            Sentinel::Punto2D q;
            const ValorJson* xVal = buscarClave(item, {"x", "lat", "latitude", "px"});
            const ValorJson* yVal = buscarClave(item, {"y", "lon", "longitude", "py"});

            if (xVal && yVal) {
                q.x = xVal->numero;
                q.y = yVal->numero;
            } else if (item.esLista() && item.lista.size() >= 2) {
                q.x = item.lista[0].numero;
                q.y = item.lista[1].numero;
            }

            Sentinel::Punto2D centralCercana = voronoi.consultarCentralMasCercana(q);
            double dist = q.distancia(centralCercana);

            if (i > 0) {
                salidaAsignaciones += ",";
            }
            salidaAsignaciones += "{\"query\":{\"x\":" + numeroJson(q.x) + ",\"y\":" + numeroJson(q.y) + "}" +
                                  ",\"assigned_central\":" + textoJson(centralCercana.nombre) +
                                  ",\"distance\":" + numeroJson(dist) + "}";
        }
    }
    salidaAsignaciones += "]";

    const vector<Sentinel::CeldaVoronoi>& regiones = voronoi.obtenerRegiones();
    string salidaRegiones = "[";
    size_t totalRegiones = regiones.size();
    for (size_t i = 0; i < totalRegiones; ++i) {
        const Sentinel::CeldaVoronoi& celda = regiones[i];
        if (i > 0) {
            salidaRegiones += ",";
        }
        salidaRegiones += "{\"central\":" + textoJson(celda.nombreCentral) +
                          ",\"site\":{\"x\":" + numeroJson(celda.sitioCentral.x) + ",\"y\":" + numeroJson(celda.sitioCentral.y) + "}" +
                          ",\"polygon\":[";
        size_t totalVertices = celda.verticesPoligono.size();
        for (size_t j = 0; j < totalVertices; ++j) {
            const Sentinel::Punto2D& v = celda.verticesPoligono[j];
            if (j > 0) {
                salidaRegiones += ",";
            }
            salidaRegiones += "{\"x\":" + numeroJson(v.x) + ",\"y\":" + numeroJson(v.y) + "}";
        }
        salidaRegiones += "]}";
    }
    salidaRegiones += "]";

    return "{\"assignments\":" + salidaAsignaciones + ",\"regions\":" + salidaRegiones + "}";
}

// ---------- Módulo II · Matching (función de matching.h) ----------
// Lee el texto de una transmisión del JSON y quita saltos de línea/espacios
string leerTransmision(const ValorJson& entrada, const vector<string>& claves) {
    const ValorJson* valor = buscarClave(entrada, claves);
    if (!valor) throw runtime_error("Falta una transmision en el input");
    string limpio;
    for (char c : valor->texto) {
        if (!isspace(static_cast<unsigned char>(c))) limpio += c;
    }
    return limpio;
}

string resolverMatching(const ValorJson& entrada) {
    string t1 = leerTransmision(entrada, {"transmission1", "t1", "T1", "transmision1"});
    string t2 = leerTransmision(entrada, {"transmission2", "t2", "T2", "transmision2"});

    const ValorJson* listaFirmas = buscarClave(entrada, {"signatures", "patterns", "firmas", "mcodes"});
    if (!listaFirmas || !listaFirmas->esLista()) throw runtime_error("Faltan las firmas en el input");
    vector<string> firmas;
    for (const ValorJson& f : listaFirmas->lista) firmas.push_back(f.texto);

    // Se asume que todasFirmas regresa primero las de T1 y luego las de T2
    vector<ResultadoMatching> resultados = todasFirmas(t1, t2, firmas);

    string salida = "{\"results\":[";
    for (size_t i = 0; i < resultados.size(); ++i) {
        salida += (i ? "," : "");
        if (resultados[i].encontrado) {
            salida += "{\"found\":true,\"position\":" + to_string(resultados[i].posicionInicial) + "}";
        } else {
            salida += "{\"found\":false}";
        }
    }
    return salida + "]}";
}


// ---------- Módulo II · Palindrome (Manacher) ----------
string resolverPalindrome(const ValorJson& entrada) {
    string T1 = leerTransmision(entrada,
        {"transmission1", "t1", "T1", "transmision1"});

    string T2 = leerTransmision(entrada,
        {"transmission2", "t2", "T2", "transmision2"});

    vector<ResultadoPalindromo> resultados =
        analizarPalindromos(T1, T2);

    string salida = "{\"results\":[";

    for (int i = 0; i < (int)resultados.size(); i++) {
        if (i > 0) {
            salida += ",";
        }

        salida += "{\"start\":"
               + to_string(resultados[i].posicionInicial)
               + ",\"end\":"
               + to_string(resultados[i].posicionFinal)
               + "}";
    }

    salida += "]}";

    return salida;
}


// ---------- Modulo II · Longest Common Substring ----------
string resolverCommonSubstring(const ValorJson& entrada) {

    string T1 = leerTransmision(entrada,
        {"transmission1", "t1", "T1", "transmision1"});

    string T2 = leerTransmision(entrada,
        {"transmission2", "t2", "T2", "transmision2"});

    ResultadoSubcadena resultado = longestCommonSubstring(T1, T2);

    string salida = "{\"start\":"
        + to_string(resultado.posicionInicial)
        + ",\"end\":"
        + to_string(resultado.posicionFinal)
        + "}";

    return salida;
}


int main() {
    ios::sync_with_stdio(false);
    string texto((istreambuf_iterator<char>(cin)), istreambuf_iterator<char>());

    try {
        LectorJson lector;
        ValorJson peticion = lector.leer(texto);
        string modo = peticion["mode"].texto;
        const ValorJson& entrada = peticion.tiene("input") ? peticion["input"] : peticion;

        if (modo == "mst") {
            cout << resolverMST(entrada) << endl;
        } else if (modo == "tsp") {
            cout << resolverTSPJson(entrada) << endl;
        } else if (modo == "max_flow") {
            cout << resolverMaxFlow(entrada) << endl;
        } else if (modo == "voronoi") {
            cout << resolverVoronoi(entrada) << endl;
        } else if (modo == "matching") {
            cout << resolverMatching(entrada) << endl;
        } else if (modo == "palindrome") {
            cout << resolverPalindrome(entrada) << endl;
        } else if (modo == "common_substring") {
            cout << resolverCommonSubstring(entrada) << endl;
        }
        
        else {
            cout << "{\"error\":\"Modo no implementado: " << modo << "\"}" << endl;
            return 1;
        }
    } catch (const exception& error) {
        cout << "{\"error\":" << textoJson(error.what()) << "}" << endl;
        return 1;
    }
    return 0;
}
