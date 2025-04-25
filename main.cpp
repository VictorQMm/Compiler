#include <iostream>
#include <fstream>
#include <regex>
#include <unordered_set>
#include <vector>
#include <iomanip>

using namespace std;

struct Simbolo {
    string lexema;
    string tipo; 
};

bool ehPalavraReservada(const string& palavra) {
    static unordered_set<string> palavrasReservadas = {
        "Program", "read", "write", "integer", "boolean", "double", "function", "procedure", "begin",
        "end", "and", "array", "case", "const", "div", "do",
        "downto", "else", "file", "for", "goto", "if", "in", "label",
        "mod", "nil", "not", "of", "or" ,"packed", "record", "repeat", "set", 
        "then", "to", "type", "until", "with", "var", "while"
    };
    return palavrasReservadas.count(palavra) > 0;
}

string classificarLexema(const string& lexema) {
    static regex numeroReal(R"(\d+\.\d+)");
    static regex numeroInteiro(R"(\d+)");
    static regex identificador(R"([a-zA-Z_][a-zA-Z0-9_]*)");

    if (ehPalavraReservada(lexema)) return "palavra_reservada";
    if (regex_match(lexema, numeroInteiro)) return "numero_inteiro";
    if (regex_match(lexema, numeroReal)) return "numero_real";
    if (regex_match(lexema, identificador)) return "identificador";

    // Símbolos válidos
    static unordered_set<string> simbolos_compostos = {
        ":=", "<=", ">=", "<>", "++", "--", 
    };
    if (simbolos_compostos.count(lexema)) return "simbolo composto";


    static unordered_set<string> simbolos = {
         "+", "-", "*", "/", "=", "^", "<", ">", ";", ".", ":", ",", "{", "}", "[", "]"
    };
    if (simbolos.count(lexema)) return "simbolo";

    return "desconhecido";
}

void analisarLexico(const string& caminhoEntrada, const string& caminhoSaida) {
    ifstream arquivoEntrada(caminhoEntrada);
    ofstream arquivoSaida(caminhoSaida);
    string linha;
    vector<Simbolo> tabelaSimbolos;

 
    regex comentarioC(R"(\{[^}]*\})");
    regex comentarioA(R"(\(\*[^*]*\*\))");

    while (getline(arquivoEntrada, linha)) {
 
        linha = regex_replace(linha, comentarioC, "");
        linha = regex_replace(linha, comentarioA, "");

      
        regex separador(R"((:=|<=|>=|<>|[\s\+\-\*/=<>;:.,\(\)\[\]]))");
        sregex_token_iterator iterador(linha.begin(), linha.end(), separador, {-1, 0}); // 0 separadores -1 lexemas
        sregex_token_iterator fim;

        for (; iterador != fim; ++iterador) {
            string lexema = iterador->str();
            if (lexema.empty() || lexema == " ") continue;

            string tipo = classificarLexema(lexema);
            tabelaSimbolos.push_back({lexema, tipo});
        }
    }

   // cria tabela de saida 
    arquivoSaida << setw(15) << "Lexema" << setw(20) << "Tipo" << "\n";
    arquivoSaida << string(35, '-') << "\n";
    for (const auto& simbolo : tabelaSimbolos)
        arquivoSaida << setw(15) << simbolo.lexema << setw(20) << simbolo.tipo << "\n";
}


int main() {
    string arquivoEntrada = "codigo.txt";
    string arquivoSaida = "tabela.txt";

    analisarLexico(arquivoEntrada, arquivoSaida);
    cout << "Análise léxica finalizada. Tabela salva em '" << arquivoSaida << "'." << endl;

    return 0;
}
