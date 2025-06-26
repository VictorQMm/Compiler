#include <iostream>
#include <fstream>
#include <regex>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <string>
#include <sstream> // pegar varios tipos em uma unica string
#include <unordered_set>

using namespace std;

struct Simbolo {
    string lexema;
    string tipo;
    int linha;
};

bool isPr(const string& palavra) {
    static const unordered_set<string> palavrasReservadas = {
        "Program", "read", "write", "integer", "boolean", "double", "function", "procedure", "begin", "end",
        "and", "array", "case", "const", "div", "do", "downto", "else", "file", "for", "goto", "if", "in",
        "label", "mod", "nil", "not", "of", "or", "packed", "record", "repeat", "set", "then", "to", "type",
        "until", "with", "var", "while", "true", "false"
    };
    return palavrasReservadas.count(palavra);
}

string tipoLex(const string& lex) {
    static regex numReal(R"(^\d+\.\d+$)");
    static regex numInt(R"(^\d+$)");
    static regex id(R"(^[a-zA-Z_][a-zA-Z0-9_]*$)");
    static regex numRuim(R"(^\d*\.\d+\.\d*$)"); // detecta números com múltiplos ponto
    static regex strLiteral(R"(^\".*\"$)");

    if (isPr(lex)) return "Palavra reservada";
    if (regex_match(lex, strLiteral)) return "String literal";
    if (regex_match(lex, numReal)) return "Numero real";
    if (regex_match(lex, numInt)) return "Numero inteiro";
    if (regex_match(lex, id)) return "Identificador";
    if (regex_match(lex, numRuim)) return "numero_invalido";

    static vector<string> simbolosCompostos = { ":=", "<=", ">=", "<>", "++", "--" };
    for (auto& sc : simbolosCompostos) {
        if (sc == lex) return "simbolo_composto";
    }

    static vector<string> simbolosSimples = { "+", "-", "*", "/", "=", "^", "<", ">", ";", ".", ":", ",", "(", ")", "{", "}", "[", "]" };
    for (auto& ss : simbolosSimples) {
        if (ss == lex) return "simbolo";
    }

    return "desconhecido";
}

pair<vector<Simbolo>, vector<string>> analisarLexico(const string& arquivo) { // separar em tokens
    ifstream arq(arquivo);
    vector<Simbolo> tabela;
    vector<string> erros;

    if (!arq.is_open()) {
        erros.push_back("Erro: Nao abriu arquivo '" + arquivo + "'\n");
        return {tabela, erros};
    }

    string linha;
    int numLinha = 0;
    bool comentAberto = false;
    
    regex separador(R"((<=|>=|:=|<>|\+\+|--|\".*?\"|[a-zA-Z_][a-zA-Z0-9_]*|\d*\.\d+\.\d*|\d+\.\d+|[+*/=<>;:.,()\[\]{}^$])|\s+)");
    regex coment1(R"(\{.*?\}|\(\*.*?\*\))");

    while (getline(arq, linha)) { // linha por linha
        numLinha++;

        if (comentAberto) {
            size_t fimCom = linha.find("*)");
            if (fimCom != string::npos) { // string::npos = não encontrado
                comentAberto = false;
                linha = linha.substr(fimCom + 2); // pula 2 caracteres
            } else {
                continue;
            }
        }

        string linhaSemComentarios = regex_replace(linha, coment1, " ");

        size_t poscoment1 = linhaSemComentarios.find('{');
        if (poscoment1 != string::npos) {
            if (linhaSemComentarios.find('}', poscoment1) == string::npos) {
                erros.push_back("Erro lexico linha " + to_string(numLinha) + ": Comentario '{' nao fechado\n");
                linhaSemComentarios = linhaSemComentarios.substr(0, poscoment1);
            }
        }

        size_t poscoment2 = linhaSemComentarios.find("(*");
        if (poscoment2 != string::npos) {
            if (linhaSemComentarios.find("*)", poscoment2 + 2) == string::npos) {
                erros.push_back("Erro lexico linha " + to_string(numLinha) + ": Comentario '(*' nao fechado\n");
                comentAberto = true;
                linhaSemComentarios = linhaSemComentarios.substr(0, poscoment2);
            }
        }

        sregex_token_iterator it(linhaSemComentarios.begin(), linhaSemComentarios.end(), separador, {0}); 
        sregex_token_iterator end;

        for (; it != end; ++it) {
            string lex = it->str();
            if (lex.empty() || all_of(lex.begin(), lex.end(), ::isspace)) continue; //espc branco
            
            string t = tipoLex(lex);

            if (t == "desconhecido" || t == "numero_invalido") {
                string msg = "Erro lexico linha " + to_string(numLinha) + ": ";
                if (t == "numero_invalido") msg += "Numero invalido '" + lex + "'\n";
                else if (lex.length() == 1 && !isalnum(lex[0])) msg += "Caractere '" + lex + "' nao identificado\n";
                else msg += "Sequencia '" + lex + "' nao identificada\n";
                erros.push_back(msg);
                // CORREÇÃO 1: Adiciona o token inválido à tabela com um tipo de erro.
                tabela.push_back({lex, "erro_lexico", numLinha}); 
            } else {
                tabela.push_back({lex, t, numLinha}); //add na tabela
            }
        }
    }
    
    if (comentAberto) {
        erros.push_back("Erro lexico linha " + to_string(numLinha) + ": Comentario '(*' nao fechado no fim do arquivo\n");
    }

    arq.close();
    return {tabela, erros};
}


class Sintatico {
    const vector<Simbolo>& tokens;
    vector<string> erros;
    size_t posToken;

    Simbolo atual() {
        if (posToken < tokens.size()) return tokens[posToken];
        return {"EOF", "EOF", tokens.empty() ? 0 : tokens.back().linha + 1};
    }

    void avanca() {
        if (posToken < tokens.size()) posToken++;
    }

    void sincroniza() {
        avanca();
        while (atual().tipo != "EOF") {
            if (atual().lexema == ";") {
                return;
            }
            string lex = atual().lexema;
            if (lex == "if" || lex == "while" || lex == "read" ||
                lex == "write" || lex == "begin" || lex == "var" ||
                lex == "end" || atual().tipo == "Identificador") {
                return;
            }
            avanca();
        }
    }

    bool casa(const string& esperado, bool isLexema = true) { //esperado -> lex or tipo
        if (atual().tipo == "EOF") {
            erros.push_back("Erro sintatico linha " + std::to_string(tokens.empty() ? 1 : tokens.back().linha + 1) + ": Fim de arquivo inesperado. Esperava '" + esperado + "'.\n");
            return false;
        }
        string valorAtual = isLexema ? atual().lexema : atual().tipo;
        if (valorAtual == esperado) {
            avanca();
            return true;
        } else {
            string tipoEsperadoStr = isLexema ? "lexema" : "tipo";
            string msg = "Erro sintatico linha " + to_string(atual().linha) + ": ";
            msg += "Esperava " + tipoEsperadoStr + " '" + esperado + "', mas encontrou '" + atual().lexema + "'.\n";
            erros.push_back(msg);
            return false;
        }
    }

    void prog() {
        if (!casa("Program")) {
            sincroniza();
        }
        if (!casa("Identificador", false)) {
            sincroniza();
        }
        if (!casa(";")) {
            sincroniza();
        }
        bloco();
        casa(".");
        if (atual().tipo != "EOF") {
            erros.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Tokens adicionais depois do fim do programa.\n");
        }
    }

    void bloco() {
        if (atual().lexema == "var") {
            declVar();
        }
        if (!casa("begin")) {
            sincroniza();
        }
        listaComandos();
        casa("end");
    }

    void declVar() {
        casa("var");
        while (atual().lexema != "begin" && atual().tipo != "EOF") {
            if (atual().tipo != "Identificador") {
                erros.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Esperava um identificador para iniciar a declaracao.\n");
                sincroniza();
                if (atual().lexema == "begin" || atual().tipo == "EOF") break;
                continue;
            }
            listaIds();
            if (!casa(":")) {
                if (atual().lexema == "integer" || atual().lexema == "boolean" || atual().lexema == "double") {
                    erros.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Falta ':' antes do tipo '" + atual().lexema + "'.\n");
                } else {
                    sincroniza();
                    if (atual().lexema == "begin" || atual().tipo == "EOF") break;
                    continue;
                }
            }
            if (!tipo()) {
                sincroniza();
                if (atual().lexema == "begin" || atual().tipo == "EOF") break;
                continue;
            }
            if (!casa(";")) {
                if (atual().lexema == "begin" || atual().tipo == "Identificador") {
                    erros.push_back("Erro sintatico linha " + to_string(tokens[posToken > 0 ? posToken - 1 : 0].linha) + ": Falta ';' no final da declaracao.\n");
                } else {
                    sincroniza();
                }
            }
        }
    }

    void listaIds() {
        casa("Identificador", false);
        while (atual().lexema == ",") {
            avanca();
            casa("Identificador", false);
        }
    }

    bool tipo() {
        string lex = atual().lexema;
        if (lex == "integer" || lex == "boolean" || lex == "double") {
            avanca();
            return true;
        }
        erros.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Esperava um tipo (integer, double, boolean), mas encontrou '" + lex + "'.\n");
        return false;
    }

    void listaComandos() {
        while (atual().lexema != "end" && atual().tipo != "EOF") {
            int linhaAnterior = atual().linha;
            comando();
            if (atual().lexema == "end") {
                break;
            }
            if (atual().lexema != ";") {
                if (atual().lexema != "end" && atual().tipo != "EOF") {
                    erros.push_back("Erro sintatico linha " + to_string(linhaAnterior) + ": Falta ';' no final da instrucao.\n");
                    sincroniza();
                }
            } else {
                avanca();
            }
        }
    }

    void comando() {
        string tipoToken = atual().tipo;
        string lex = atual().lexema;

        if (tipoToken == "Identificador") atribuicao();
        else if (lex == "read") leitura();
        else if (lex == "write") escrita();
        else if (lex == "if") se();
        else if (lex == "while") enquanto();
        else if (lex == "begin") blocoInicioFim();
        else {
            erros.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Comando invalido ou inesperado '" + lex + "'.\n");
            sincroniza();
        }
    }

    void atribuicao() {
        casa("Identificador", false);
        if (atual().lexema == ":=") {
            avanca();
            expressao();
        } else if (atual().lexema == "=") {
            int linhaDoErro = atual().linha; 
            avanca();
            erros.push_back("Erro sintatico linha " + to_string(linhaDoErro) + ": Operador de atribuicao invalido '='. Use ':='.\n");
            expressao();
        } else {
            sincroniza();
        }
    }

    void leitura() {
        casa("read");
        casa("(");
        listaIds();
        casa(")");
    }

    void escrita() {
        casa("write");
        casa("(");
        listaExp();
        casa(")");
    }

    void se() { // if -> expr(then) -> comand(else)
        casa("if");
        expressao();
        if (!casa("then")) {
           // Nao sincroniza, apenas reporta o erro e tenta continuar
        }
        comando();
        if (atual().lexema == "else") {
            avanca();
            comando();
        }
    }

    void enquanto() {
        casa("while");
        expressao();
        if(!casa("do")){
            sincroniza();
        } else {
            comando();
        }
    }

    void blocoInicioFim() {
        casa("begin");
        listaComandos();
        casa("end");
    }

    void listaExp() {
        expressao();
        while (atual().lexema == ",") {
            avanca();
            expressao();
        }
    }

    void expressao() {
        expressaoSimples();
        while (atual().lexema == "=" || atual().lexema == "<>" || atual().lexema == "<" ||
               atual().lexema == ">" || atual().lexema == "<=" || atual().lexema == ">=") {
            avanca();
            expressaoSimples();
        }
    }

    void expressaoSimples() {
        termo();
        while (atual().lexema == "+" || atual().lexema == "-" || atual().lexema == "or") {
            avanca();
            termo();
        }
    }

    void termo() {
        fator();
        while (atual().lexema == "*" || atual().lexema == "/" || atual().lexema == "and" ||
               atual().lexema == "div" || atual().lexema == "mod") {
            avanca();
            fator();
        }
    }

    void fator() {
        string t = atual().tipo;
        string lex = atual().lexema;

        if (t == "Identificador" || t == "Numero inteiro" || t == "Numero real" || t == "String literal") {
            avanca();
        } else if (lex == "true" || lex == "false") {
            avanca();
        } else if (lex == "(") {
            avanca();
            expressao();
            casa(")");
        } else if (lex == "not") {
            avanca();
            fator();
        // CORREÇÃO 2: Reconhece um token de erro léxico e o consome para evitar mais erros.
        } else if (t == "erro_lexico") {
            avanca();
        } else {
            erros.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Token inesperado '" + lex + "' na expressao.\n");
            sincroniza();
        }
    }

public:
    Sintatico(const vector<Simbolo>& toks) : posToken(0), tokens(toks) {}

    void analisar() {
        if (tokens.empty()) {
            erros.push_back("Erro sintatico: Nao ha tokens para analisar.\n");
            return;
        }
        prog();
    }

    vector<string> getErros() {
        return erros;
    }
};


int main() {
    
    string arquivo = "codigo.txt";
    string saida = "tabela.txt";

    auto resultado = analisarLexico(arquivo);
    vector<Simbolo> tabela = resultado.first;
    vector<string> errosLex = resultado.second;

    ofstream arqSaida(saida);
    if (!arqSaida.is_open()) {
        cerr << "Erro: Nao abriu arquivo de saida '" << saida << "'\n";
        return 1;
    }

    arqSaida << left << setw(20) << "Lexema" << setw(25) << "Tipo" << setw(10) << "Linha" << "\n";
    arqSaida << string(55, '-') << "\n";
    for (const auto& simb : tabela) {
        arqSaida << left << setw(20) << simb.lexema << setw(25) << simb.tipo << setw(10) << simb.linha << "\n";
    }
    arqSaida.close();

    cout << "Analise lexica terminada. Tabela de simbolos salva em '" << saida << "'.\n";

    if (!errosLex.empty()) {
        cout << "\n- Erros Lexicos Encontrados -\n";
        for (const auto& e : errosLex) cout << e;
    }
    
    if (!errosLex.empty() && errosLex[0].find("Nao abriu arquivo") != string::npos) {
        return 1;
    }

    cout << "\n- Iniciando Analise Sintatica -\n";
    Sintatico sint(tabela);
    sint.analisar();

    vector<string> errosSint = sint.getErros();

    if (!errosSint.empty()) {
        cout << "\n- Erros Sintaticos Encontrados -\n";
        for (const auto& e : errosSint) cout << e;
    } else if (errosLex.empty()) {
        cout << "Analise sintatica concluida sem erros.\n";
    }

    return 0;
}
