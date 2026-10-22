#ifndef GRAFO_H
#define GRAFO_H

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "json.h"

using namespace std;

// Enlace no dirigido entre dos centrales (índices internos 0..n-1)
struct Enlace {
    int origen;
    int destino;
    double costo;
};

// Red leída del JSON. Los IDs originales se guardan para devolverlos igual.
struct Red {
    vector<ValorJson> idsNodos;
    vector<Enlace> enlaces;
    int numNodos() const { return idsNodos.size(); }
};

// Busca la primera clave que exista de una lista de nombres posibles.
// Así el lector funciona aunque el JSON use "edges" o "links", "cost" o "weight", etc.
inline const ValorJson* buscarClave(const ValorJson& obj, const vector<string>& nombres) {
    for (const string& nombre : nombres) {
        if (obj.tiene(nombre)) return &obj[nombre];
    }
    return nullptr;
}

// Convierte un ID (texto o número) a una clave única para el mapa
inline string claveNodo(const ValorJson& id) {
    return id.tipo == ValorJson::TEXTO ? id.texto : numeroJson(id.numero);
}

class LectorRed {
private:
    map<string, int> indicePorId;
    Red red;

    int indiceDe(const ValorJson& id) {
        string clave = claveNodo(id);
        auto it = indicePorId.find(clave);
        if (it != indicePorId.end()) return it->second;
        int nuevo = red.idsNodos.size();
        indicePorId[clave] = nuevo;
        red.idsNodos.push_back(id);
        return nuevo;
    }

public:
    Red leer(const ValorJson& entrada) {
        // 1. Nodos (opcional): ["AT","CH"] o [{"id":"AT",...}]
        const ValorJson* nodos = buscarClave(entrada, {"nodes", "nodos", "vertices"});
        if (nodos && nodos->esLista()) {
            for (const ValorJson& nodo : nodos->lista) {
                const ValorJson* id = nodo.esObjeto() ? buscarClave(nodo, {"id", "name", "nombre"}) : &nodo;
                if (!id) fallar("Nodo sin id");
                indiceDe(*id);
            }
        }

        // 2. Enlaces: [["AT","CH",120.5]] o [{"u":"AT","v":"CH","cost":120.5}]
        const ValorJson* enlaces = buscarClave(entrada, {"edges", "links", "enlaces", "aristas"});
        if (enlaces && enlaces->esLista()) {
            for (const ValorJson& e : enlaces->lista) {
                const ValorJson *a, *b, *w;
                if (e.esLista()) {
                    if (e.lista.size() < 3) fallar("Enlace incompleto");
                    a = &e.lista[0];
                    b = &e.lista[1];
                    w = &e.lista[2];
                } else {
                    a = buscarClave(e, {"u", "from", "source", "src", "origen", "a"});
                    b = buscarClave(e, {"v", "to", "target", "dst", "destino", "b"});
                    w = buscarClave(e, {"cost", "weight", "w", "distance", "costo", "distancia", "peso"});
                }
                if (!a || !b || !w) fallar("Enlace con campos faltantes");
                red.enlaces.push_back({indiceDe(*a), indiceDe(*b), w->numero});
            }
        }

        // 3. Alternativa: matriz de distancias completa
        const ValorJson* matriz = buscarClave(entrada, {"matrix", "distances", "matriz", "distancias"});
        if (matriz && matriz->esLista()) {
            int n = matriz->lista.size();
            for (int i = red.numNodos(); i < n; ++i) {
                ValorJson id;
                id.tipo = ValorJson::NUMERO;
                id.numero = i;
                indiceDe(id);
            }
            for (int i = 0; i < n; ++i) {
                for (int j = i + 1; j < n; ++j) {
                    double w = matriz->lista[i].lista[j].numero;
                    if (w >= 0) red.enlaces.push_back({i, j, w});
                }
            }
        }

        return red;
    }

    // Índice de un nodo dado su ID; -1 si no existe
    int buscarIndice(const ValorJson& id) const {
        auto it = indicePorId.find(claveNodo(id));
        return it == indicePorId.end() ? -1 : it->second;
    }
};

#endif