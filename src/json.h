#ifndef JSON_H
#define JSON_H

#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

// Imprime un error en formato JSON y termina el programa sin usar excepciones.
inline void fallar(const string& mensaje) {
    string escapado;
    for (size_t i = 0; i < mensaje.size(); ++i) {
        if (mensaje[i] == 34 || mensaje[i] == 92) {
            escapado += static_cast<char>(92);
        }
        escapado += mensaje[i];
    }
    cout << "{\"error\":\"" << escapado << "\"}" << endl;
    exit(1);
}

// Valor JSON genérico: puede ser nulo, booleano, número, texto, lista u objeto.
struct ValorJson {
    enum Tipo { NULO, BOOLEANO, NUMERO, TEXTO, LISTA, OBJETO };

    Tipo tipo = NULO;
    bool booleano = false;
    double numero = 0;
    string texto;
    vector<ValorJson> lista;
    map<string, ValorJson> objeto;

    bool esObjeto() const { return tipo == OBJETO; }
    bool esLista() const { return tipo == LISTA; }
    bool esNumero() const { return tipo == NUMERO; }

    bool tiene(const string& clave) const {
        return tipo == OBJETO && objeto.count(clave) > 0;
    }

    const ValorJson& operator[](const string& clave) const {
        auto it = objeto.find(clave);
        if (it == objeto.end()) fallar("Falta la clave '" + clave + "' en el JSON");
        return it->second;
    }
};

// Parser recursivo descendente: recorre el texto una sola vez, O(longitud).
class LectorJson {
private:
    string texto;
    size_t pos = 0;

    void saltarEspacios() {
        while (pos < texto.size() && isspace(static_cast<unsigned char>(texto[pos]))) ++pos;
    }

    void esperar(char c) {
        saltarEspacios();
        if (pos >= texto.size() || texto[pos] != c) {
            fallar(string("JSON invalido: se esperaba '") + c + "'");
        }
        ++pos;
    }

    string leerTexto() {
        esperar('"');
        string resultado;
        while (pos < texto.size() && texto[pos] != '"') {
            char c = texto[pos++];
            if (c == '\\' && pos < texto.size()) {
                char e = texto[pos++];
                if (e == 'n') resultado += '\n';
                else if (e == 't') resultado += '\t';
                else if (e == 'u') pos += 4; // Caracteres unicode no se usan en el reto
                else resultado += e;
            } else {
                resultado += c;
            }
        }
        esperar('"');
        return resultado;
    }

    ValorJson leerValor() {
        saltarEspacios();
        if (pos >= texto.size()) fallar("JSON invalido: fin inesperado");

        ValorJson valor;
        char c = texto[pos];

        if (c == '{') {
            valor.tipo = ValorJson::OBJETO;
            ++pos;
            saltarEspacios();
            if (texto[pos] == '}') { ++pos; return valor; }
            while (true) {
                string clave = leerTexto();
                esperar(':');
                valor.objeto[clave] = leerValor();
                saltarEspacios();
                if (texto[pos] == ',') { ++pos; continue; }
                esperar('}');
                break;
            }
        } else if (c == '[') {
            valor.tipo = ValorJson::LISTA;
            ++pos;
            saltarEspacios();
            if (texto[pos] == ']') { ++pos; return valor; }
            while (true) {
                valor.lista.push_back(leerValor());
                saltarEspacios();
                if (texto[pos] == ',') { ++pos; continue; }
                esperar(']');
                break;
            }
        } else if (c == '"') {
            valor.tipo = ValorJson::TEXTO;
            valor.texto = leerTexto();
        } else if (texto.compare(pos, 4, "true") == 0) {
            valor.tipo = ValorJson::BOOLEANO;
            valor.booleano = true;
            pos += 4;
        } else if (texto.compare(pos, 5, "false") == 0) {
            valor.tipo = ValorJson::BOOLEANO;
            pos += 5;
        } else if (texto.compare(pos, 4, "null") == 0) {
            pos += 4;
        } else {
            size_t usados = 0;
            valor.tipo = ValorJson::NUMERO;
            string trozo = texto.substr(pos, 40);
            char* fin = nullptr;
            valor.numero = strtod(trozo.c_str(), &fin);
            usados = fin - trozo.c_str();
            if (usados == 0) fallar("JSON invalido: valor inesperado");
            pos += usados;
        }
        return valor;
    }

public:
    ValorJson leer(const string& entrada) {
        texto = entrada;
        pos = 0;
        return leerValor();
    }
};

// Convierte un número a texto sin ceros de más: 41 -> "41", 12.5 -> "12.5"
inline string numeroJson(double x) {
    ostringstream salida;
    salida.precision(15);
    salida << x;
    return salida.str();
}

inline string textoJson(const string& s) {
    string resultado = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') resultado += '\\';
        resultado += c;
    }
    return resultado + "\"";
}

// Escribe de regreso un valor tal como vino (sirve para devolver los IDs de nodos)
inline string escribirJson(const ValorJson& v) {
    switch (v.tipo) {
        case ValorJson::NULO: return "null";
        case ValorJson::BOOLEANO: return v.booleano ? "true" : "false";
        case ValorJson::NUMERO: return numeroJson(v.numero);
        case ValorJson::TEXTO: return textoJson(v.texto);
        case ValorJson::LISTA: {
            string s = "[";
            for (size_t i = 0; i < v.lista.size(); ++i) s += (i ? "," : "") + escribirJson(v.lista[i]);
            return s + "]";
        }
        case ValorJson::OBJETO: {
            string s = "{";
            bool primero = true;
            for (const auto& par : v.objeto) {
                s += (primero ? "" : ",") + textoJson(par.first) + ":" + escribirJson(par.second);
                primero = false;
            }
            return s + "}";
        }
    }
    return "null";
}

#endif