#ifndef GRAFO_H
#define GRAFO_H

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "json.h"

using namespace std;

// Enlace generico
struct Enlace {
    int origen = -1;
    int destino = -1;
    double costo = 0.0;       
    double capacidad = 0.0;
};

struct Red {
    vector<ValorJson> idsNodos;
    vector<Enlace> enlaces;
    int numNodos() const { return static_cast<int>(idsNodos.size()); }
};

inline const ValorJson* buscarClave(const ValorJson& obj, const vector<string>& nombres) {
    for (const string& nombre : nombres) {
        if (obj.tiene(nombre)) return &obj[nombre];
    }
    return nullptr;
}

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
        int nuevo = static_cast<int>(red.idsNodos.size());
        indicePorId[clave] = nuevo;
        red.idsNodos.push_back(id);
        return nuevo;
    }

public:
    Red leer(const ValorJson& entrada) {
        const ValorJson* nodos = buscarClave(entrada, {"nodes", "nodos", "vertices"});
        if (nodos && nodos->esLista()) {
            for (const ValorJson& nodo : nodos->lista) {
                const ValorJson* id = nodo.esObjeto() ? buscarClave(nodo, {"id", "name", "nombre"}) : &nodo;
                if (!id) throw runtime_error("Nodo sin id");
                indiceDe(*id);
            }
        }

        const ValorJson* enlaces = buscarClave(entrada, {"edges", "links", "enlaces", "aristas"});
        if (enlaces && enlaces->esLista()) {
            for (const ValorJson& e : enlaces->lista) {
                const ValorJson *a = nullptr, *b = nullptr, *w = nullptr, *cap = nullptr;
                if (e.esLista()) {
                    if (e.lista.size() < 3) throw runtime_error("Enlace incompleto");
                    a = &e.lista[0];
                    b = &e.lista[1];
                    w = &e.lista[2];
                    if (e.lista.size() >= 4) cap = &e.lista[3];
                } else {
                    a = buscarClave(e, {"u", "from", "source", "src", "origen", "a"});
                    b = buscarClave(e, {"v", "to", "target", "dst", "destino", "b"});
                    w = buscarClave(e, {"cost", "weight", "w", "distance", "costo", "distancia", "peso"});
                    cap = buscarClave(e, {"capacity", "capacidad", "cap"});
                }
                if (!a || !b) throw runtime_error("Enlace con campos faltantes");

                double valorCosto = (w != nullptr) ? w->numero : 0.0;
                double valorCapacidad = (cap != nullptr) ? cap->numero : valorCosto;

                red.enlaces.push_back({indiceDe(*a), indiceDe(*b), valorCosto, valorCapacidad});
            }
        }

        const ValorJson* matriz = buscarClave(entrada, {"matrix", "distances", "matriz", "distancias"});
        if (matriz && matriz->esLista()) {
            int n = static_cast<int>(matriz->lista.size());
            for (int i = red.numNodos(); i < n; ++i) {
                ValorJson id;
                id.tipo = ValorJson::NUMERO;
                id.numero = i;
                indiceDe(id);
            }
            for (int i = 0; i < n; ++i) {
                for (int j = i + 1; j < n; ++j) {
                    double w = matriz->lista[i].lista[j].numero;
                    if (w >= 0) red.enlaces.push_back({i, j, w, w});
                }
            }
        }

        return red;
    }

    int buscarIndice(const ValorJson& id) const {
        auto it = indicePorId.find(claveNodo(id));
        return it == indicePorId.end() ? -1 : it->second;
    }
};

#endif // GRAFO_H
