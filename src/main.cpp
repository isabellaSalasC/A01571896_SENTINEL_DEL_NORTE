/*
 * Programa: SENTINELA DEL NORTE · TC2038
 * Descripcion: Nucleo algoritmico (MST, TSP, flujo maximo, Voronoi, firmas,
 *              palindromos y subcadena comun) controlado por JSON.
 * Autores: Sophia Alexandra Ramos Garcia, Marian Guadalupe Willis Lozano,
 *          Regina Elizabeth Flores Sosa, Isabella Salas Corpus
 * Matriculas: A00841358, A00842035, A01770709, A01571896
 * Fecha: 22/10/2026
 */

// Contrato: lee UN objeto JSON de stdin ({"mode":"...","input":{...}})
// y escribe UN objeto JSON en stdout.

#include <iostream>
#include <cmath>
#include <algorithm>
#include <iterator>
#include <queue>
#include <string>

#include "arbolExpanMin.h"
#include "grafo.h"
#include "json.h"
#include "viajante.h"
#include "matching.h"
#include "palindrome.h"
#include "common_substr.h"
#include "flujoMaximo.h"
#include "diagramaVoronoi.h"

using namespace std;

// ---------- Módulo I · MST ----------
string resolverMST(const ValorJson& entrada) {
    LectorRed lector;
    Red red = lector.leer(entrada);

    // Kruskal por defecto; "algorithm":"prim" para comparar
    auto* algoritmo = buscarClave(entrada, {"algorithm", "algoritmo", "method"});
    bool usarPrim = algoritmo && algoritmo->texto == "prim";
    ResultadoMST mst = usarPrim ? mstPrim(red) : mstKruskal(red);

    // Cada enlace seleccionado se escribe como arreglo [u, v, costo]
    string salida = "{\"edges\":[";
    for (size_t i = 0; i < mst.enlaces.size(); ++i) {
        auto& e = mst.enlaces[i];
        salida += (i ? "," : "");
        salida += "[" + escribirJson(red.idsNodos[e.origen]) + "," + escribirJson(red.idsNodos[e.destino]) +
                  "," + numeroJson(e.costo) + "]";
    }
    salida += "],\"cost\":" + numeroJson(mst.costoTotal);
    salida += ",\"total_cost\":" + numeroJson(mst.costoTotal);
    salida += ",\"connected\":" + string(mst.conexo ? "true" : "false");
    salida += ",\"algorithm\":\"" + string(usarPrim ? "prim" : "kruskal") + "\"}";
    return salida;
}

// ---------- Módulo I · TSP ----------
// Resuelve el viajante sobre una matriz de distancias directas (ruta cerrada desde el nodo 0).
string resolverTSPMatriz(const ValorJson& matriz) {
    int n = matriz.lista.size();
    vector<vector<double>> dist(n, vector<double>(n, 0));
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n && j < (int)matriz.lista[i].lista.size(); ++j) {
            dist[i][j] = matriz.lista[i].lista[j].numero;
        }
    }
    ResultadoTSP tsp;
    if (n <= 1) {
        tsp.ruta = {0, 0};
        tsp.costoTotal = 0;
        tsp.esOptimo = true;
    } else {
        tsp = n <= LIMITE_EXACTO ? tspExacto(dist, 0) : tspAproximado(dist, 0);
    }
    string salida = "{\"route\":[";
    for (size_t i = 0; i < tsp.ruta.size(); ++i) salida += (i ? "," : "") + to_string(tsp.ruta[i]);
    salida += "],\"cost\":" + numeroJson(tsp.costoTotal);
    salida += ",\"exact\":" + string(tsp.esOptimo ? "true" : "false") + "}";
    return salida;
}

// Resuelve el viajante: con "distances" usa la matriz directa; con enlaces usa caminos minimos.
string resolverTSPJson(const ValorJson& entrada) {
    auto* matriz = buscarClave(entrada, {"distances", "matrix", "matriz", "distancias"});
    if (matriz && matriz->esLista() && !matriz->lista.empty() && matriz->lista[0].esLista()) {
        return resolverTSPMatriz(*matriz);
    }

    LectorRed lector;
    Red red = lector.leer(entrada);

    int inicio = 0;
    auto* origen = buscarClave(entrada, {"start", "origin", "source", "origen", "inicio"});
    if (origen) {
        inicio = lector.buscarIndice(*origen);
        if (inicio == -1) fallar("Nodo inicial no existe");
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

// ---------- Módulo II · Lectura de transmisiones y firmas ----------
// Quita saltos de linea y espacios de una secuencia
string limpiarTexto(const string& texto) {
    string limpio;
    for (size_t i = 0; i < texto.size(); ++i) {
        if (!isspace(static_cast<unsigned char>(texto[i]))) limpio += texto[i];
    }
    return limpio;
}

// Texto de un valor: una cadena directa o el campo de texto de un objeto
string textoDe(const ValorJson& valor) {
    if (valor.tipo == ValorJson::TEXTO) return limpiarTexto(valor.texto);
    if (valor.esObjeto()) {
        auto* campo = buscarClave(valor, {"data", "text", "sequence", "content", "value", "hex", "payload"});
        if (campo && campo->tipo == ValorJson::TEXTO) return limpiarTexto(campo->texto);
    }
    return "";
}

// Indica si el nombre de una clave corresponde a firmas y no a transmisiones
bool esClaveDeFirmas(const string& clave) {
    return clave.find("mcode") != string::npos || clave.find("sig") != string::npos ||
           clave.find("pattern") != string::npos || clave.find("firma") != string::npos;
}

// Lee las dos transmisiones aceptando varias formas del JSON
vector<string> leerTransmisiones(const ValorJson& entrada) {
    vector<string> lectura;
    auto* lista = buscarClave(entrada, {"transmissions", "transmisiones", "sequences", "texts", "strings"});
    if (lista && lista->esLista()) {
        for (size_t i = 0; i < lista->lista.size(); ++i) lectura.push_back(textoDe(lista->lista[i]));
    }
    if (lectura.size() < 2) {
        lectura.clear();
        vector<vector<string>> grupos = {
            {"transmission1", "t1", "T1", "transmision1", "text1", "sequence1", "a", "A", "s1", "string1", "first"},
            {"transmission2", "t2", "T2", "transmision2", "text2", "sequence2", "b", "B", "s2", "string2", "second"}};
        for (size_t g = 0; g < grupos.size(); ++g) {
            auto* valor = buscarClave(entrada, grupos[g]);
            if (valor) lectura.push_back(textoDe(*valor));
        }
    }
    if (lectura.size() < 2 && entrada.esObjeto()) {
        lectura.clear();
        for (auto par = entrada.objeto.begin(); par != entrada.objeto.end(); ++par) {
            if (par->second.tipo == ValorJson::TEXTO && par->first != "mode" && !esClaveDeFirmas(par->first)) {
                lectura.push_back(textoDe(par->second));
            }
        }
    }
    if (lectura.size() < 2) fallar("Faltan las dos transmisiones en el input");
    return lectura;
}

// Lee las firmas de referencia aceptando varias formas del JSON
vector<string> leerFirmas(const ValorJson& entrada) {
    vector<string> firmas;
    auto* lista = buscarClave(entrada, {"signatures", "patterns", "firmas", "mcodes", "signature", "pattern", "codes", "mcode"});
    if (lista && lista->esLista()) {
        for (size_t i = 0; i < lista->lista.size(); ++i) firmas.push_back(textoDe(lista->lista[i]));
    } else if (lista && lista->tipo == ValorJson::TEXTO) {
        firmas.push_back(textoDe(*lista));
    }
    if (firmas.empty()) {
        vector<string> nombres = {"mcode1", "mcode2", "mcode3", "signature1", "signature2", "signature3",
                                  "pattern1", "pattern2", "pattern3", "m1", "m2", "m3", "firma1", "firma2", "firma3"};
        for (size_t i = 0; i < nombres.size(); ++i) {
            if (entrada.tiene(nombres[i])) firmas.push_back(textoDe(entrada[nombres[i]]));
        }
    }
    if (firmas.empty() && entrada.esObjeto()) {
        for (auto par = entrada.objeto.begin(); par != entrada.objeto.end(); ++par) {
            bool otraLista = par->second.esLista() && par->first.find("transm") == string::npos &&
                             par->first != "sequences" && par->first != "texts";
            if (otraLista) {
                for (size_t i = 0; i < par->second.lista.size(); ++i) firmas.push_back(textoDe(par->second.lista[i]));
                break;
            }
        }
    }
    if (firmas.empty()) fallar("Faltan las firmas en el input");
    return firmas;
}

// Arma la respuesta de lineas de texto (formato del reto) como lista JSON
string lineasJson(const vector<string>& lineas) {
    string salida = "[";
    for (size_t i = 0; i < lineas.size(); ++i) salida += (i ? "," : "") + textoJson(lineas[i]);
    return salida + "]";
}

// ---------- Módulo II · Matching (función de matching.h) ----------
// Busca las firmas en las dos transmisiones y regresa la primera posicion (base 1) de cada una.
string resolverMatching(const ValorJson& entrada) {
    vector<string> transmisiones = leerTransmisiones(entrada);
    vector<string> firmas = leerFirmas(entrada);

    // KMP devuelve posiciones en base 0; el reto pide base 1 (se suma 1 al imprimir)
    // todasFirmas regresa primero las firmas de T1 y luego las de T2
    vector<ResultadoMatching> resultados = todasFirmas(transmisiones[0], transmisiones[1], firmas);

    string salida = "{\"results\":[";
    vector<string> lineas;
    for (size_t i = 0; i < resultados.size(); ++i) {
        salida += (i ? "," : "");
        if (resultados[i].encontrado) {
            string posicion = to_string(resultados[i].posicionInicial + 1);
            salida += "{\"found\":true,\"position\":" + posicion + "}";
            lineas.push_back("true " + posicion);
        } else {
            salida += "{\"found\":false}";
            lineas.push_back("false");
        }
    }
    // "matches": una lista por transmision con la posicion (base 1) de cada firma o -1
    string coincidencias = "[";
    size_t porTransmision = firmas.size();
    for (size_t t = 0; t < 2; ++t) {
        coincidencias += (t ? ",[" : "[");
        for (size_t f = 0; f < porTransmision; ++f) {
            auto& r = resultados[t * porTransmision + f];
            coincidencias += (f ? "," : "") + to_string(r.encontrado ? r.posicionInicial + 1 : -1);
        }
        coincidencias += "]";
    }
    return salida + "],\"matches\":" + coincidencias + "],\"lines\":" + lineasJson(lineas) + "}";
}


// ---------- Módulo II · Palindrome (Manacher) ----------
// Palindromo mas largo de cada transmision (posiciones base 1).
string resolverPalindrome(const ValorJson& entrada) {
    vector<string> transmisiones = leerTransmisiones(entrada);
    vector<ResultadoPalindromo> resultados = analizarPalindromos(transmisiones[0], transmisiones[1]);

    string salida = "{\"results\":[";
    string rangos = "[";
    vector<string> lineas;
    for (size_t i = 0; i < resultados.size(); ++i) {
        string inicio = to_string(resultados[i].posicionInicial);
        string fin = to_string(resultados[i].posicionFinal);
        salida += (i ? "," : "");
        salida += "{\"start\":" + inicio + ",\"end\":" + fin + "}";
        rangos += (i ? "," : "") + string("[") + inicio + "," + fin + "]";
        lineas.push_back(inicio + " " + fin);
    }
    return salida + "],\"ranges\":" + rangos + "],\"lines\":" + lineasJson(lineas) + "}";
}


// ---------- Modulo II · Longest Common Substring ----------
// Arreglo de sufijos por duplicacion de prefijos, O(n log^2 n).
vector<int> arregloSufijos(const vector<int>& texto) {
    int n = texto.size();
    vector<int> sa(n), rango(n), temporal(n);
    for (int i = 0; i < n; ++i) {
        sa[i] = i;
        rango[i] = texto[i];
    }
    for (int k = 1; k < n; k <<= 1) {
        auto menor = [&](int a, int b) {
            if (rango[a] != rango[b]) return rango[a] < rango[b];
            int ra = a + k < n ? rango[a + k] : -1;
            int rb = b + k < n ? rango[b + k] : -1;
            return ra < rb;
        };
        sort(sa.begin(), sa.end(), menor);
        temporal[sa[0]] = 0;
        for (int i = 1; i < n; ++i) temporal[sa[i]] = temporal[sa[i - 1]] + (menor(sa[i - 1], sa[i]) ? 1 : 0);
        rango = temporal;
        if (rango[sa[n - 1]] == n - 1) break;
    }
    return sa;
}

// Subcadena comun mas larga con arreglo de sufijos + LCP (para entradas grandes).
// Empates: menor inicio en S y luego menor inicio en T. Regresa {inicio, fin} base 1 en S.
ResultadoSubcadena subcadenaComunSufijos(const string& S, const string& T) {
    int n = S.size();
    int m = T.size();
    vector<int> texto;
    for (int i = 0; i < n; ++i) texto.push_back(static_cast<unsigned char>(S[i]) + 2);
    texto.push_back(1);
    for (int j = 0; j < m; ++j) texto.push_back(static_cast<unsigned char>(T[j]) + 2);
    int total = texto.size();
    vector<int> sa = arregloSufijos(texto);

    vector<int> rango(total), lcp(total, 0);
    for (int i = 0; i < total; ++i) rango[sa[i]] = i;
    int h = 0;
    for (int i = 0; i < total; ++i) {
        if (rango[i] > 0) {
            int j = sa[rango[i] - 1];
            while (i + h < total && j + h < total && texto[i + h] == texto[j + h]) ++h;
            lcp[rango[i]] = h;
            if (h > 0) --h;
        } else {
            h = 0;
        }
    }

    // Longitud maxima: par adyacente con un sufijo de S y otro de T
    int mejor = 0;
    for (int i = 1; i < total; ++i) {
        bool unoEnS = sa[i - 1] < n, otroEnS = sa[i] < n;
        bool unoEnT = sa[i - 1] > n, otroEnT = sa[i] > n;
        if (((unoEnS && otroEnT) || (unoEnT && otroEnS)) && lcp[i] > mejor) mejor = lcp[i];
    }
    if (mejor == 0) return {0, 0};

    // Grupos de sufijos con LCP >= mejor: se toma el menor inicio en S y en T de cada grupo
    int mejorS = -1, mejorT = -1;
    int i = 0;
    while (i < total) {
        int j = i;
        while (j + 1 < total && lcp[j + 1] >= mejor) ++j;
        int minS = -1, minT = -1;
        for (int k = i; k <= j; ++k) {
            if (sa[k] < n) {
                if (minS == -1 || sa[k] < minS) minS = sa[k];
            } else if (sa[k] > n) {
                if (minT == -1 || sa[k] - n - 1 < minT) minT = sa[k] - n - 1;
            }
        }
        if (j > i && minS != -1 && minT != -1) {
            if (mejorS == -1 || minS < mejorS || (minS == mejorS && minT < mejorT)) {
                mejorS = minS;
                mejorT = minT;
            }
        }
        i = j + 1;
    }
    return {mejorS + 1, mejorS + mejor};
}

// Subcadena comun mas larga de las dos transmisiones (posiciones en la primera, base 1).
string resolverCommonSubstring(const ValorJson& entrada) {
    vector<string> transmisiones = leerTransmisiones(entrada);
    auto& primera = transmisiones[0];
    auto& segunda = transmisiones[1];

    // DP clasico para entradas chicas; arreglo de sufijos cuando la tabla seria enorme
    bool chico = static_cast<double>(primera.size()) * static_cast<double>(segunda.size()) <= 4e7;
    ResultadoSubcadena resultado = chico ? longestCommonSubstring(primera, segunda)
                                         : subcadenaComunSufijos(primera, segunda);

    string inicio = to_string(resultado.posicionInicial);
    string fin = to_string(resultado.posicionFinal);
    return "{\"start\":" + inicio + ",\"end\":" + fin + ",\"range\":[" + inicio + "," + fin + "],\"lines\":" + lineasJson({inicio + " " + fin}) + "}";
}


// ---------- Módulo I · Flujo máximo (Edmonds-Karp) ----------
// Copia la entrada agregando "cost" a los enlaces que solo traen capacidad,
// para poder reutilizar el lector de red.
ValorJson prepararEnlacesFlujo(const ValorJson& entrada) {
    ValorJson copia = entrada;
    // Si solo viene la cantidad de nodos ("n"), se crean los nodos 0..n-1
    if (!copia.tiene("nodes") && copia.tiene("n") && copia["n"].esNumero()) {
        ValorJson nodos;
        nodos.tipo = ValorJson::LISTA;
        for (int i = 0; i < static_cast<int>(copia["n"].numero); ++i) {
            ValorJson id;
            id.tipo = ValorJson::NUMERO;
            id.numero = i;
            nodos.lista.push_back(id);
        }
        copia.objeto["nodes"] = nodos;
    }
    vector<string> nombresLista = {"edges", "links", "enlaces", "aristas"};
    for (size_t n = 0; n < nombresLista.size(); ++n) {
        if (!copia.tiene(nombresLista[n]) || !copia.objeto[nombresLista[n]].esLista()) continue;
        vector<ValorJson>& enlaces = copia.objeto[nombresLista[n]].lista;
        for (size_t i = 0; i < enlaces.size(); ++i) {
            if (!enlaces[i].esObjeto()) continue;
            if (buscarClave(enlaces[i], {"cost", "weight", "w", "distance", "costo", "distancia", "peso"})) continue;
            auto* capacidad = buscarClave(enlaces[i], {"capacity", "capacidad", "cap", "c", "bandwidth"});
            ValorJson costo;
            costo.tipo = ValorJson::NUMERO;
            costo.numero = capacidad ? capacidad->numero : 0;
            enlaces[i].objeto["cost"] = costo;
        }
    }
    return copia;
}

// Calcula el flujo maximo entre source y sink, el flujo por enlace y el corte minimo.
// Los enlaces son dirigidos (u -> v); con "directed": false cada uno vale en ambos sentidos.
string resolverFlujoMaximo(const ValorJson& original) {
    ValorJson entrada = prepararEnlacesFlujo(original);
    LectorRed lector;
    Red red = lector.leer(entrada);

    auto* origen = buscarClave(entrada, {"source", "origin", "origen", "src", "s", "from", "start"});
    auto* destino = buscarClave(entrada, {"sink", "target", "destino", "dst", "t", "to", "end"});
    if (!origen || !destino) fallar("Faltan source y sink en el input");
    int s = lector.buscarIndice(*origen);
    int t = lector.buscarIndice(*destino);
    if (s == -1 || t == -1) fallar("Origen o destino no existe");

    // La capacidad se lee de "capacity"; si no viene, se usa el costo/peso.
    // Se guarda la posición de cada arco para recuperar el flujo por enlace.
    auto* listaEnlaces = buscarClave(entrada, {"edges", "links", "enlaces", "aristas"});
    bool dirigido = !(entrada.tiene("directed") && !entrada["directed"].booleano);
    int n = red.numNodos();
    int m = red.enlaces.size();
    GrafoFlujo grafo(n);
    vector<double> capacidades(m, 0);
    vector<int> posicion(m, -1);
    vector<ValorJson> idsEnlace(m);
    for (int i = 0; i < m; ++i) {
        double cap = red.enlaces[i].costo;
        ValorJson id;
        id.tipo = ValorJson::NUMERO;
        id.numero = i;
        if (listaEnlaces && i < (int)listaEnlaces->lista.size() && listaEnlaces->lista[i].esObjeto()) {
            auto* c = buscarClave(listaEnlaces->lista[i], {"capacity", "capacidad", "cap", "c", "bandwidth"});
            if (c) cap = c->numero;
            auto* idx = buscarClave(listaEnlaces->lista[i], {"id"});
            if (idx) id = *idx;
        }
        capacidades[i] = cap;
        idsEnlace[i] = id;
        int u = red.enlaces[i].origen, v = red.enlaces[i].destino;
        if (u == v) continue;
        long long capacidad = static_cast<long long>(cap);
        posicion[i] = grafo.obtenerAdyacencia()[u].size();   // arco u->v en adj[u]
        grafo.agregarArista(u, v, capacidad);
        if (!dirigido) grafo.agregarArista(v, u, capacidad); // su inverso queda en adj[u]
    }

    long long flujo = (s == t) ? 0 : grafo.obtenerFlujoMaximo(s, t);
    auto& ady = grafo.obtenerAdyacencia();

    // Corte mínimo: nodos alcanzables desde s en la red residual
    vector<bool> alcanzable(n, false);
    vector<int> pila;
    alcanzable[s] = true;
    pila.push_back(s);
    while (!pila.empty()) {
        int x = pila.back();
        pila.pop_back();
        for (size_t k = 0; k < ady[x].size(); ++k) {
            if (ady[x][k].capacidadResidual() > 0 && !alcanzable[ady[x][k].destino]) {
                alcanzable[ady[x][k].destino] = true;
                pila.push_back(ady[x][k].destino);
            }
        }
    }

    string aristas = "[";
    string corte = "[";
    double capacidadCorte = 0;
    bool primeraArista = true, primeraCorte = true;
    for (int i = 0; i < m; ++i) {
        int u = red.enlaces[i].origen, v = red.enlaces[i].destino;
        long long neto = 0;
        if (posicion[i] >= 0) {
            neto = ady[u][posicion[i]].flujo;
            if (!dirigido) neto += ady[u][posicion[i] + 1].flujo;
        }
        aristas += string(primeraArista ? "" : ",") + "{\"id\":" + escribirJson(idsEnlace[i]) +
                   ",\"u\":" + escribirJson(red.idsNodos[u]) + ",\"v\":" + escribirJson(red.idsNodos[v]) +
                   ",\"flow\":" + to_string(neto) + ",\"capacity\":" + numeroJson(capacidades[i]) + "}";
        primeraArista = false;
        if (alcanzable[u] != alcanzable[v]) {
            corte += string(primeraCorte ? "" : ",") + escribirJson(idsEnlace[i]);
            capacidadCorte += capacidades[i];
            primeraCorte = false;
        }
    }

    string salida = "{\"max_flow\":" + to_string(flujo) +
                    ",\"flow\":" + to_string(flujo) +
                    ",\"value\":" + to_string(flujo) +
                    ",\"source\":" + escribirJson(red.idsNodos[s]) +
                    ",\"sink\":" + escribirJson(red.idsNodos[t]) +
                    ",\"edges\":" + aristas + "]" +
                    ",\"min_cut\":{\"capacity\":" + numeroJson(capacidadCorte) + ",\"edges\":" + corte + "]}";
    auto* demanda = buscarClave(entrada, {"demand", "demanda"});
    if (demanda && demanda->esNumero()) {
        salida += ",\"demand\":" + numeroJson(demanda->numero);
        salida += ",\"demand_satisfied\":" + string(flujo >= demanda->numero ? "true" : "false");
    }
    return salida + "}";
}


// ---------- Módulo I · Diagrama de Voronoi (dual de Delaunay) ----------
// Lee un punto como {"x":..,"y":..} o como [x, y]. Regresa falso si no tiene esa forma.
bool leerPunto(const ValorJson& valor, double& x, double& y) {
    if (valor.esLista() && valor.lista.size() >= 2) {
        x = valor.lista[0].numero;
        y = valor.lista[1].numero;
        return true;
    }
    auto* px = buscarClave(valor, {"x", "lon", "lng", "longitude", "X"});
    auto* py = buscarClave(valor, {"y", "lat", "latitude", "Y"});
    if (!px || !py) return false;
    x = px->numero;
    y = py->numero;
    return true;
}

// Lee el dominio como [xmin,ymin,xmax,ymax], [[x0,y0],[x1,y1]] u objeto con xmin/ymin/xmax/ymax.
bool leerDominio(const ValorJson& entrada, Sentinel::BoundingBox& dominio) {
    auto* d = buscarClave(entrada, {"domain", "dominio", "bounds", "bbox", "boundingBox", "bounding_box", "box"});
    if (!d) return false;
    if (d->esLista() && d->lista.size() >= 4 && d->lista[0].esNumero()) {
        dominio.xMin = d->lista[0].numero;
        dominio.yMin = d->lista[1].numero;
        dominio.xMax = d->lista[2].numero;
        dominio.yMax = d->lista[3].numero;
        return true;
    }
    double x0, y0, x1, y1;
    if (d->esLista() && d->lista.size() >= 2 && leerPunto(d->lista[0], x0, y0) && leerPunto(d->lista[1], x1, y1)) {
        dominio.xMin = min(x0, x1);
        dominio.yMin = min(y0, y1);
        dominio.xMax = max(x0, x1);
        dominio.yMax = max(y0, y1);
        return true;
    }
    auto* a = buscarClave(*d, {"xmin", "x_min", "xMin", "minX", "min_x", "left"});
    auto* b = buscarClave(*d, {"ymin", "y_min", "yMin", "minY", "min_y", "bottom"});
    auto* c = buscarClave(*d, {"xmax", "x_max", "xMax", "maxX", "max_x", "right"});
    auto* e = buscarClave(*d, {"ymax", "y_max", "yMax", "maxY", "max_y", "top"});
    if (a && b && c && e) {
        dominio.xMin = a->numero;
        dominio.yMin = b->numero;
        dominio.xMax = c->numero;
        dominio.yMax = e->numero;
        return true;
    }
    return false;
}

// Construye las regiones de Voronoi recortadas al dominio y responde las consultas de proximidad.
string resolverVoronoi(const ValorJson& entrada) {
    auto* listaSitios = buscarClave(entrada, {"sites", "sitios", "centrales", "centers", "centrals", "stations", "points"});
    if (!listaSitios || !listaSitios->esLista() || listaSitios->lista.empty()) {
        fallar("Faltan los sitios en el input");
    }

    vector<Sentinel::Punto2D> sitios;
    vector<ValorJson> ids;
    for (size_t i = 0; i < listaSitios->lista.size(); ++i) {
        auto& sitio = listaSitios->lista[i];
        auto* id = buscarClave(sitio, {"id", "name", "nombre"});
        Sentinel::Punto2D p;
        if (!leerPunto(sitio, p.x, p.y)) fallar("Sitio sin coordenadas");
        p.id = static_cast<int>(i);
        p.nombre = id ? claveNodo(*id) : to_string(i);
        sitios.push_back(p);
        ValorJson identificador;
        if (id) {
            identificador = *id;
        } else {
            identificador.tipo = ValorJson::NUMERO;
            identificador.numero = static_cast<double>(i);
        }
        ids.push_back(identificador);
    }

    Sentinel::BoundingBox dominio;
    if (!leerDominio(entrada, dominio)) {
        dominio = {sitios[0].x, sitios[0].x, sitios[0].y, sitios[0].y};
        for (size_t i = 0; i < sitios.size(); ++i) {
            dominio.xMin = min(dominio.xMin, sitios[i].x);
            dominio.xMax = max(dominio.xMax, sitios[i].x);
            dominio.yMin = min(dominio.yMin, sitios[i].y);
            dominio.yMax = max(dominio.yMax, sitios[i].y);
        }
        double margenX = (dominio.xMax - dominio.xMin) * 0.1 + 1;
        double margenY = (dominio.yMax - dominio.yMin) * 0.1 + 1;
        dominio = {dominio.xMin - margenX, dominio.xMax + margenX, dominio.yMin - margenY, dominio.yMax + margenY};
    }

    // Celda de cada sitio: el dominio recortado por el semiplano más cercano a ese sitio
    // frente a cada otro sitio (mismo diagrama que el dual de Delaunay, pero acotado).
    string celdasJson = "[";
    string poligonosJson = "[";
    double areaTotal = 0;
    for (size_t i = 0; i < sitios.size(); ++i) {
        vector<pair<double, double>> poligono = {
            {dominio.xMin, dominio.yMin}, {dominio.xMax, dominio.yMin},
            {dominio.xMax, dominio.yMax}, {dominio.xMin, dominio.yMax}};
        for (size_t j = 0; j < sitios.size() && !poligono.empty(); ++j) {
            if (j == i) continue;
            double nx = sitios[j].x - sitios[i].x, ny = sitios[j].y - sitios[i].y;
            if (nx == 0 && ny == 0) continue;
            double mx = (sitios[i].x + sitios[j].x) / 2;
            double my = (sitios[i].y + sitios[j].y) / 2;
            auto lado = [&](const pair<double, double>& p) {
                return (p.first - mx) * nx + (p.second - my) * ny;
            };
            vector<pair<double, double>> nuevo;
            for (size_t k = 0; k < poligono.size(); ++k) {
                pair<double, double> p = poligono[k], q = poligono[(k + 1) % poligono.size()];
                double dp = lado(p), dq = lado(q);
                if (dp <= 0) nuevo.push_back(p);
                if ((dp < 0 && dq > 0) || (dp > 0 && dq < 0)) {
                    double f = dp / (dp - dq);
                    nuevo.push_back({p.first + f * (q.first - p.first), p.second + f * (q.second - p.second)});
                }
            }
            poligono = nuevo;
        }
        double area = 0;
        string vertices = "[";
        for (size_t k = 0; k < poligono.size(); ++k) {
            pair<double, double> p = poligono[k];
            pair<double, double> q = poligono[(k + 1) % poligono.size()];
            area += p.first * q.second - q.first * p.second;
            vertices += string(k ? "," : "") + "[" + numeroJson(p.first) + "," + numeroJson(p.second) + "]";
        }
        area = fabs(area) / 2;
        areaTotal += area;
        poligonosJson += string(i ? "," : "") + vertices + "]";
        celdasJson += string(i ? "," : "") + "{\"id\":" + escribirJson(ids[i]) +
                      ",\"site\":" + escribirJson(ids[i]) + ",\"index\":" + to_string(i) +
                      ",\"area\":" + numeroJson(area) +
                      ",\"polygon\":" + vertices + "],\"vertices\":" + vertices + "]}";
    }
    celdasJson += "]";
    poligonosJson += "]";

    double areaDominio = (dominio.xMax - dominio.xMin) * (dominio.yMax - dominio.yMin);
    string salida = "{\"domain\":[" + numeroJson(dominio.xMin) + "," + numeroJson(dominio.yMin) + "," +
                    numeroJson(dominio.xMax) + "," + numeroJson(dominio.yMax) + "],\"domain_area\":" +
                    numeroJson(areaDominio) + ",\"total_area\":" + numeroJson(areaTotal) +
                    ",\"cells\":" + poligonosJson + ",\"regions\":" + celdasJson + ",\"queries\":[";

    // Consultas: central más cercana por fuerza bruta O(sitios) por consulta
    auto* consultas = buscarClave(entrada, {"queries", "consultas", "query", "query_points", "locations"});
    if (consultas && consultas->esLista()) {
        for (size_t i = 0; i < consultas->lista.size(); ++i) {
            double qx = 0, qy = 0;
            if (!leerPunto(consultas->lista[i], qx, qy)) fallar("Consulta sin coordenadas");
            size_t mejor = 0;
            double menor = 1e300;
            for (size_t k = 0; k < sitios.size(); ++k) {
                double d2 = (sitios[k].x - qx) * (sitios[k].x - qx) + (sitios[k].y - qy) * (sitios[k].y - qy);
                if (d2 < menor) {
                    menor = d2;
                    mejor = k;
                }
            }
            bool dentro = qx >= dominio.xMin && qx <= dominio.xMax && qy >= dominio.yMin && qy <= dominio.yMax;
            salida += (i ? "," : "");
            salida += "{\"x\":" + numeroJson(qx) + ",\"y\":" + numeroJson(qy) +
                      ",\"site\":" + escribirJson(ids[mejor]) +
                      ",\"site_id\":" + escribirJson(ids[mejor]) +
                      ",\"index\":" + to_string(mejor) +
                      ",\"distance\":" + numeroJson(sqrt(menor)) +
                      ",\"inside_domain\":" + string(dentro ? "true" : "false") + "}";
        }
    }
    return salida + "]}";
}

// Deja el nombre del modo en minusculas y sin separadores, y lo lleva al nombre canonico.
string normalizarModo(const string& crudo) {
    string limpio;
    for (size_t i = 0; i < crudo.size(); ++i) {
        if (isalnum(static_cast<unsigned char>(crudo[i]))) {
            limpio += static_cast<char>(tolower(static_cast<unsigned char>(crudo[i])));
        }
    }
    if (limpio == "mst" || limpio == "minimumspanningtree" || limpio == "kruskal" || limpio == "prim") return "mst";
    if (limpio == "tsp" || limpio == "travelingsalesman" || limpio == "viajante") return "tsp";
    if (limpio == "maxflow" || limpio == "flow" || limpio == "maximumflow" || limpio == "flujomaximo" ||
        limpio == "flujo") return "maxflow";
    if (limpio == "voronoi") return "voronoi";
    if (limpio == "matching" || limpio == "patterns" || limpio == "signatures" || limpio == "stringmatching" ||
        limpio == "kmp" || limpio == "match") return "matching";
    if (limpio == "palindrome" || limpio == "palindromes" || limpio == "manacher") return "palindrome";
    if (limpio == "common" || limpio == "commonsubstring" || limpio == "lcs" || limpio == "longestcommonsubstring" ||
        limpio == "lcsubstring" || limpio == "similarity") return "common_substring";
    return limpio;
}

// Lee la peticion JSON de stdin, elige el modo y escribe la respuesta JSON en stdout.
int main() {
    ios::sync_with_stdio(false);
    string texto((istreambuf_iterator<char>(cin)), istreambuf_iterator<char>());

    try {
        LectorJson lector;
        ValorJson peticion = lector.leer(texto);
        string modo = normalizarModo(peticion["mode"].texto);
        auto& entrada = peticion.tiene("input") ? peticion["input"] : peticion;

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
        } else if (modo == "maxflow") {
            cout << resolverFlujoMaximo(entrada) << endl;
        } else if (modo == "voronoi") {
            cout << resolverVoronoi(entrada) << endl;
        } else {
            cout << "{\"error\":\"Modo no implementado: " << modo << "\"}" << endl;
            return 1;
        }
    } catch (const exception& error) {
        cout << "{\"error\":" << textoJson(error.what()) << "}" << endl;
        return 1;
    }
    return 0;
}
