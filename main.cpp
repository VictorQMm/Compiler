#include <iostream>
#include <fstream>
#include <regex>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <string>
#include <sstream> // pegar varios tipos em uma unica string

using namespace std;

struct Simbolo {
    string lexema;
    string tipo;
    int linha;
};

bool ehPalavraReservada(const string& palavra) {
    static vector<string> palavrasReservadas = {
        "Program", "read", "write", "integer", "boolean", "double", "function", "procedure", "begin", "end", "and", "array", "case", "const",
        "div", "do", "downto", "else", "file", "for", "goto", "if", "in", "label","mod", "nil", "not", "of", "or", "packed", "record", "repeat",
        "set", "then", "to", "type", "until", "with", "var", "while"
    };
    return find(palavrasReservadas.begin(), palavrasReservadas.end(), palavra) != palavrasReservadas.end();
}

string classificarLexema(const string& lexema) {
    static regex numReal(R"(^\d+\.\d+$)");
    static regex numInt(R"(^\d+$)");
    static regex id(R"(^[a-zA-Z_][a-zA-Z0-9_]*$)");
    static regex numInvalido(R"(^\d*\.\d*\.\d*.*$)");

    if (ehPalavraReservada(lexema)) return "Palavra reservada";
    if (regex_match(lexema, numReal)) return "Numero real";
    if (regex_match(lexema, numInt)) return "Numero inteiro";
    if (regex_match(lexema, id)) return "Identificador";
    if (regex_match(lexema, numInvalido)) return "numero_invalido";

    static vector<string> simbolosCompostos = {
        ":=", "<=", ">=", "<>", "++", "--"
    };
    if (find(simbolosCompostos.begin(), simbolosCompostos.end(), lexema) != simbolosCompostos.end())
        return "simbolo_composto";

    static vector<string> simbolos = {
        "+", "-", "*", "/", "=", "^", "<", ">", ";", ".", ":", ",", "(", ")", "{", "}", "[", "]"
    };
    if (find(simbolos.begin(), simbolos.end(), lexema) != simbolos.end())
        return "simbolo";

    return "desconhecido";
}

vector<Simbolo> tabelaSimbolos;
vector<string> errosLexicos;


pair<vector<Simbolo>, vector<string>> analisarLexico(const string& caminhoEntrada) {
    ifstream arquivoEntrada(caminhoEntrada); 
    vector<Simbolo> tabelaSimbolos;          //tokens
    vector<string> errosLexicos;            

  
    if (!arquivoEntrada.is_open()) {
        errosLexicos.push_back("Erro: Nao foi possivel abrir o arquivo de entrada '" + caminhoEntrada + "'.");
        return {tabelaSimbolos, errosLexicos};
    }


    string linha;
    int numeroLinha = 0;

    regex separador(R"((<=|>=|:=|<>|\+\+|--|[a-zA-Z_][a-zA-Z0-9_]*|\d*\.\d*\.\d*.*|\d+\.\d+|\d+|[\+\*/=<>;:\.,\(\)\[\]\{\}\^\-]|\s+))");
    regex comentarioChave(R"(\{[^}]*\})");
    regex comentarioParentesesEstrela(R"(\(\*[\s\S]*?\*\))");
    regex comentarioAberto(R"(\{[^}]*$)");

    while (getline(arquivoEntrada, linha)) {
        numeroLinha++;

        if (regex_search(linha, comentarioAberto)) {
            errosLexicos.push_back("Erro lexico na linha " + to_string(numeroLinha) + ": Comentario aberto '{' nao fechado.");
            continue; // Pula a token desta linha
        }

        string linhaSemComentarios = regex_replace(linha, comentarioChave, "");
        linhaSemComentarios = regex_replace(linhaSemComentarios, comentarioParentesesEstrela, "");

        sregex_token_iterator i(linhaSemComentarios.begin(), linhaSemComentarios.end(), separador, {-1, 0});
        sregex_token_iterator fim;

        for (; i != fim; ++i) {
            string lexema = i->str();
            if (lexema.empty() || regex_match(lexema, regex(R"(^\s+$)"))) {
                continue;
            }

            string tipo = classificarLexema(lexema);

            if (tipo == "desconhecido" || tipo == "numero_invalido") {
                string mensagemErro;
                if (tipo == "numero_invalido") {
                    mensagemErro = "Erro lexico na linha " + to_string(numeroLinha) + ": Numero invalido '" + lexema + "'.";
                } else if (lexema.length() == 1) {
                    mensagemErro = "Erro lexico na linha " + to_string(numeroLinha) + ": Caractere '" + lexema + "' nao identificado.";
                } else {
                    mensagemErro = "Erro lexico na linha " + to_string(numeroLinha) + ": Sequencia '" + lexema + "' nao identificada.";
                }
                errosLexicos.push_back(mensagemErro);
            } else {
                tabelaSimbolos.push_back({lexema, tipo, numeroLinha});
            }
        }
    }

 arquivoEntrada.close(); 
return {tabelaSimbolos, errosLexicos};



int main() {
    string arquivoEntrada = "codigo.txt";
    string arquivoSaida = "tabela.txt";

    analisarLexico(arquivoEntrada, arquivoSaida);

    return 0;
}