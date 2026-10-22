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

    // Instanciacion de ambas estructuras de flujo
    GrafoFlujo redEK(totalNodos);
    Sentinel::GrafoFlujoDinic redDinic(totalNodos);

    for (size_t i = 0; i < totalEnlaces; ++i) {
        const Enlace& e = red.enlaces[i];
        long long cap = static_cast<long long>(e.capacidad);
        redEK.agregarArista(e.origen, e.destino, cap);
        redDinic.agregarArista(e.origen, e.destino, cap);
    }

    // Ejecucion incondicional de Edmonds-Karp y Dinic
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
