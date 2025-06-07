#include <iostream>
#include <fstream>
#include <regex>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <string> // getline

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
    static regex numReal(R"(^(?:\d+\.\d+)$)");
    static regex numInt(R"(^\d+$)");
    static regex id(R"(^[a-zA-Z_][a-zA-Z0-9_]*$)");

    if (ehPalavraReservada(lexema)) return "Palavra reservada";
    if (regex_match(lexema, numReal)) return "Numero real";
    if (regex_match(lexema, numInt)) return "Numero inteiro";
    if (regex_match(lexema, id)) return "Identificador";

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

void analisarLexico(const string& caminhoEntrada, const string& caminhoSaida) {
    ifstream arquivoEntrada(caminhoEntrada);
    ofstream arquivoSaida(caminhoSaida);

    if (!arquivoEntrada.is_open()) {
        cerr << "Erro: Não foi possível abrir o arquivo de entrada '" << caminhoEntrada << "'." << endl;
        return;
    }
    if (!arquivoSaida.is_open()) {
        cerr << "Erro: Não foi possível criar/abrir o arquivo de saída '" << caminhoSaida << "'." << endl;
        return;
    }

    vector<Simbolo> tabelaSimbolos;
    string linha;
    int numeroLinha = 0;

    regex separador(R"((<=|>=|:=|<>|\+\+|--|[\+\*/=<>;:\.,\(\)\[\]\{\}\\-]|\s+))");
    regex comentarioChave(R"(\{[^}]*\})");
    regex comentarioParentesesEstrela(R"(\(\*[\s\S]*?\*\))"); 

    while (getline(arquivoEntrada, linha)) {
        numeroLinha++;

        string linhaSemComentarios = regex_replace(linha, comentarioChave, "");
        linhaSemComentarios = regex_replace(linhaSemComentarios, comentarioParentesesEstrela, "");

        sregex_token_iterator iterador(linhaSemComentarios.begin(), linhaSemComentarios.end(), separador, {-1, 0});
        sregex_token_iterator fim;

        for (; iterador != fim; ++iterador) {
            string lexema = iterador->str();
            if (lexema.empty() || regex_match(lexema, regex(R"(^\s+$)"))) {
                continue; // Pula lexemas em branco
            }

            string tipo = classificarLexema(lexema);

            if (tipo == "desconhecido") {
                cerr << "Erro léxico na linha " << numeroLinha << ": Lexema '" << lexema << "' não reconhecido." << endl;
            } else {
                tabelaSimbolos.push_back({lexema, tipo, numeroLinha});
            }
        }
    }

    arquivoSaida << setw(15) << "Lexema" << setw(20) << "Tipo" << setw(10) << "Linha" << "\n";
    arquivoSaida << string(45, '-') << "\n"; // ajuste
    for (const auto& simbolo : tabelaSimbolos) {
        arquivoSaida << setw(15) << simbolo.lexema << setw(20) << simbolo.tipo << setw(10) << simbolo.linha << "\n";
    }

    arquivoEntrada.close();
    arquivoSaida.close();
}

int main() {
    string arquivoEntrada = "codigo.txt";
    string arquivoSaida = "tabela.txt";

    analisarLexico(arquivoEntrada, arquivoSaida);
    cout << "Análise léxica finalizada. Tabela salva em '" << arquivoSaida << "'." << endl;

    return 0;
}