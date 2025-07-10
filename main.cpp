#include <iostream>
#include <fstream>
#include <regex>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <string>
#include <sstream> // pegar varios tipos em uma unica string
#include <unordered_set>
#include <unordered_map>

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

    while (getline(arq, linha)) {
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

        size_t poscomment2 = linhaSemComentarios.find("(*");
        if (poscomment2 != string::npos) {
            if (linhaSemComentarios.find("*)", poscomment2 + 2) == string::npos) {
                erros.push_back("Erro lexico linha " + to_string(numLinha) + ": Comentario '(*' nao fechado\n");
                comentAberto = true;
                linhaSemComentarios = linhaSemComentarios.substr(0, poscomment2);
            }
        }

        sregex_token_iterator it(linhaSemComentarios.begin(), linhaSemComentarios.end(), separador, {0}); 
        sregex_token_iterator end;

        for (; it != end; ++it) {
            string lex = it->str();
            if (lex.empty() || all_of(lex.begin(), lex.end(), ::isspace)) continue;
            
            string t = tipoLex(lex);

            if (t == "desconhecido" || t == "numero_invalido") {
                string msg = "Erro lexico linha " + to_string(numLinha) + ": ";
                if (t == "numero_invalido") msg += "Numero invalido '" + lex + "'\n";
                else if (lex.length() == 1 && !isalnum(lex[0])) msg += "Caractere '" + lex + "' nao identificado\n";
                else msg += "Sequencia '" + lex + "' nao identificada\n";
                erros.push_back(msg);
                tabela.push_back({lex, "erro_lexico", numLinha}); 
            } else {
                tabela.push_back({lex, t, numLinha});
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
    vector<string> errosSintaticos;
    vector<string> errosSemanticos;
    size_t posToken;
    unordered_map<string, string> tabelaSimbolos;

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

    bool casa(const string& esperado, bool isLexema = true) {
        if (atual().tipo == "EOF") {
            errosSintaticos.push_back("Erro sintatico linha " + to_string(tokens.empty() ? 1 : tokens.back().linha + 1) + ": Fim de arquivo inesperado. Esperava '" + esperado + "'.\n");
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
            errosSintaticos.push_back(msg);
            return false;
        }
    }

    bool estaDeclarada(const string& id, int linha) {
        if (tabelaSimbolos.find(id) == tabelaSimbolos.end()) {
            errosSemanticos.push_back("Erro semantico linha " + to_string(linha) + ": Variavel '" + id + "' nao declarada.\n");
            return false;
        }
        return true;
    }

    void declararVariavel(const string& id, const string& tipo, int linha) {
        if (tabelaSimbolos.find(id) != tabelaSimbolos.end()) {
            errosSemanticos.push_back("Erro semantico linha " + to_string(linha) + ": Redeclaracao da variavel '" + id + "'.\n");
        } else {
            tabelaSimbolos[id] = tipo;
        }
    }

    string tipoExpressao() {
        size_t posInicial = posToken;
        string tipo = tipoExpressaoSimples();
        while (atual().lexema == "=" || atual().lexema == "<>" || atual().lexema == "<" ||
               atual().lexema == ">" || atual().lexema == "<=" || atual().lexema == ">=") {
            string op = atual().lexema;
            int linha = atual().linha;
            avanca();
            string tipo2 = tipoExpressaoSimples();
            if (tipo != tipo2) {

                errosSemanticos.push_back("Erro semantico linha " + to_string(linha) + ": Tipos incompativeis na operacao relacional '" + op + "' (" + tipo + " e " + tipo2 + ").\n");
            }
            tipo = "boolean";
        }
        posToken = posInicial;
        return tipo;
    }

    string tipoExpressaoSimples() {
        string tipo = tipoTermo();
        while (atual().lexema == "+" || atual().lexema == "-" || atual().lexema == "or") {
            string op = atual().lexema;
            int linha = atual().linha;
            avanca();
            string tipo2 = tipoTermo();
            if (op == "or") {
                if (tipo != "boolean" || tipo2 != "boolean") {
                    errosSemanticos.push_back("Erro semantico linha " + to_string(linha) + ": Operador 'or' requer operandos booleanos, encontrou " + tipo + " e " + tipo2 + ".\n");
                }
                tipo = "boolean";
            } else {
                if ((tipo == "integer" && tipo2 == "integer") || (tipo == "double" && tipo2 == "double") ||
                    (tipo == "integer" && tipo2 == "double") || (tipo == "double" && tipo2 == "integer")) {
                    tipo = (tipo == "double" || tipo2 == "double") ? "double" : "integer";
                } else {
                    errosSemanticos.push_back("Erro semantico linha " + to_string(linha) + ": Tipos incompativeis na operacao '" + op + "' (" + tipo + " e " + tipo2 + ").\n");
                }
            }
        }
        return tipo;
    }

    string tipoTermo() {
        string tipo = tipoFator();
        while (atual().lexema == "*" || atual().lexema == "/" || atual().lexema == "and" ||
               atual().lexema == "div" || atual().lexema == "mod") {
            string op = atual().lexema;
            int linha = atual().linha;
            avanca();
            string tipo2 = tipoFator();
            if (op == "and") {
                if (tipo != "boolean" || tipo2 != "boolean") {
                    errosSemanticos.push_back("Erro semantico linha " + to_string(linha) + ": Operador 'and' requer operandos booleanos, encontrou " + tipo + " e " + tipo2 + ".\n");
                }
                tipo = "boolean";
            } else if (op == "div" || op == "mod") {
                if (tipo != "integer" || tipo2 != "integer") {
                    errosSemanticos.push_back("Erro semantico linha " + to_string(linha) + ": Operador '" + op + "' requer operandos inteiros, encontrou " + tipo + " e " + tipo2 + ".\n");
                }
                tipo = "integer";
            } else {
                if ((tipo == "integer" && tipo2 == "integer") || (tipo == "double" && tipo2 == "double") ||
                    (tipo == "integer" && tipo2 == "double") || (tipo == "double" && tipo2 == "integer")) {
                    tipo = (tipo == "double" || tipo2 == "double") ? "double" : "integer";
                } else {
                    errosSemanticos.push_back("Erro semantico linha " + to_string(linha) + ": Tipos incompativeis na operacao '" + op + "' (" + tipo + " e " + tipo2 + ").\n");
                }
            }
        }
        return tipo;
    }

    string tipoFator() {
        string t = atual().tipo;
        string lex = atual().lexema;
        int linha = atual().linha;

        if (t == "Identificador") {
            if (estaDeclarada(lex, linha)) {
                return tabelaSimbolos[lex];
            }
            return "desconhecido";
        } else if (t == "Numero inteiro") {
            return "integer";
        } else if (t == "Numero real") {
            return "double";
        } else if (t == "String literal") {
            return "string";
        } else if (lex == "true" || lex == "false") {
            return "boolean";
        } else if (lex == "(") {
            avanca();
            string tipo = tipoExpressao();
            casa(")");
            return tipo;
        } else if (lex == "not") {
            avanca();
            string tipo = tipoFator();
            if (tipo != "boolean") {
                errosSemanticos.push_back("Erro semantico linha " + to_string(linha) + ": Operador 'not' requer operando booleano, encontrou " + tipo + ".\n");
            }
            return "boolean";
        } else if (t == "erro_lexico") {
            return "desconhecido";
        } else {
            errosSemanticos.push_back("Erro semantico linha " + to_string(linha) + ": Token inesperado '" + lex + "' na expressao.\n");
            return "desconhecido";
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
            errosSintaticos.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Tokens adicionais depois do fim do programa.\n");
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

                errosSintaticos.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Esperava um identificador para iniciar a declaracao.\n");
                sincroniza();
                if (atual().lexema == "begin" || atual().tipo == "EOF") break;
                continue;
            }
            vector<string> ids;
            listaIds(ids);
            if (!casa(":")) {
                if (atual().lexema == "integer" || atual().lexema == "boolean" || atual().lexema == "double") {
    
                    errosSintaticos.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Falta ':' antes do tipo '" + atual().lexema + "'.\n");
                } else {
                    sincroniza();
                    if (atual().lexema == "begin" || atual().tipo == "EOF") break;
                    continue;
                }
            }
            string tipo;
            if (!this->tipo(tipo)) {
                sincroniza();
                if (atual().lexema == "begin" || atual().tipo == "EOF") break;
                continue;
            }
            for (const auto& id : ids) {
                declararVariavel(id, tipo, tokens[posToken > 0 ? posToken - 1 : 0].linha);
            }
            if (!casa(";")) {
                if (atual().lexema == "begin" || atual().tipo == "Identificador") {
    
                    errosSintaticos.push_back("Erro sintatico linha " + to_string(tokens[posToken > 0 ? posToken - 1 : 0].linha) + ": Falta ';' no final da declaracao.\n");
                } else {
                    sincroniza();
                }
            }
        }
    }

    void listaIds(vector<string>& ids) {
        ids.push_back(atual().lexema);
        casa("Identificador", false);
        while (atual().lexema == ",") {
            avanca();
            ids.push_back(atual().lexema);
            casa("Identificador", false);
        }
    }

    bool tipo(string& tipoRet) {
        string lex = atual().lexema;
        if (lex == "integer" || lex == "boolean" || lex == "double") {
            tipoRet = lex;
            avanca();
            return true;
        }
        errosSintaticos.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Esperava um tipo (integer, double, boolean), mas encontrou '" + lex + "'.\n");
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
    
                    errosSintaticos.push_back("Erro sintatico linha " + to_string(linhaAnterior) + ": Falta ';' no final da instrucao.\n");
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
            errosSintaticos.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Comando invalido ou inesperado '" + lex + "'.\n");
            sincroniza();
        }
    }

    void atribuicao() {
        string id = atual().lexema;
        int linha = atual().linha;
        string tipoId = (tabelaSimbolos.find(id) != tabelaSimbolos.end()) ? tabelaSimbolos[id] : "desconhecido";
        casa("Identificador", false);
        if (!estaDeclarada(id, linha)) {
        }
        if (atual().lexema == ":=") {
            avanca();
            string tipoExp = tipoExpressao();
            expressao();
            if (tipoId != "desconhecido" && tipoExp != "desconhecido") {
                if (tipoId == "integer" && tipoExp != "integer") {
                    errosSemanticos.push_back("Erro semantico linha " + to_string(linha) + ": Atribuicao de tipo '" + tipoExp + "' para variavel '" + id + "' do tipo integer.\n");
                } else if (tipoId == "double" && tipoExp != "integer" && tipoExp != "double") {
                    errosSemanticos.push_back("Erro semantico linha " + to_string(linha) + ": Atribuicao de tipo '" + tipoExp + "' para variavel '" + id + "' do tipo double.\n");
                } else if (tipoId == "boolean" && tipoExp != "boolean") {
                    errosSemanticos.push_back("Erro semantico linha " + to_string(linha) + ": Atribuicao de tipo '" + tipoExp + "' para variavel '" + id + "' do tipo boolean.\n");
                }
            }
        } else if (atual().lexema == "=") {
            int linhaDoErro = atual().linha; 
            avanca();
            errosSintaticos.push_back("Erro sintatico linha " + to_string(linhaDoErro) + ": Operador de atribuicao invalido '='. Use ':='.\n");
            expressao();
        } else {
            sincroniza();
        }
    }

    void leitura() {
        casa("read");
        casa("(");
        vector<string> ids;
        listaIds(ids);
        for (const auto& id : ids) {
            estaDeclarada(id, tokens[posToken > 0 ? posToken - 1 : 0].linha);
        }
        casa(")");
    }

    void escrita() {
        casa("write");
        casa("(");
        listaExp();
        casa(")");
    }

    void se() {
        casa("if");
        string tipoExp = tipoExpressao();
        expressao();
        if (tipoExp != "boolean" && tipoExp != "desconhecido") {
            errosSemanticos.push_back("Erro semantico linha " + to_string(atual().linha) + ": Expressao do 'if' deve ser booleana, encontrou " + tipoExp + ".\n");
        }
        if (!casa("then")) {
        }
        comando();
        if (atual().lexema == "else") {
            avanca();
            comando();
        }
    }

    void enquanto() {
        casa("while");
        string tipoExp = tipoExpressao();
        expressao();
        if (tipoExp != "boolean" && tipoExp != "desconhecido") {
            errosSemanticos.push_back("Erro semantico linha " + to_string(atual().linha) + ": Expressao do 'while' deve ser booleana, encontrou " + tipoExp + ".\n");
        }
        if (!casa("do")) {
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
        int linha = atual().linha;

        if (t == "Identificador") {
            estaDeclarada(lex, linha);
            avanca();
        } else if (t == "Numero inteiro" || t == "Numero real" || t == "String literal") {
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
        } else if (t == "erro_lexico") {
            avanca();
        } else {
            errosSintaticos.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Token inesperado '" + lex + "' na expressao.\n");
            sincroniza();
        }
    }

public:
    Sintatico(const vector<Simbolo>& toks) : posToken(0), tokens(toks) {}

    void analisar() {
        if (tokens.empty()) {
            errosSintaticos.push_back("Erro sintatico: Nao ha tokens para analisar.\n");
            return;
        }
        prog();
    }

    vector<string> getErrosSintaticos() {
        return errosSintaticos;
    }
    vector<string> getErrosSemanticos() {
        return errosSemanticos;
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

    cout << "\n- Iniciando Analise Sintatica e Semantica -\n";
    Sintatico sint(tabela);
    sint.analisar();

    vector<string> errosSint = sint.getErrosSintaticos();
    vector<string> errosSem = sint.getErrosSemanticos();

    if (!errosSint.empty()) {
        cout << "\n- Erros Sintaticos Encontrados -\n";
        for (const auto& e : errosSint) cout << e;
    }

    if (!errosSem.empty()) {
        cout << "\n- Erros Semanticos Encontrados -\n";
        for (const auto& e : errosSem) cout << e;
    }

    if (errosLex.empty() && errosSint.empty() && errosSem.empty()) {
        cout << "\nAnalises lexica, sintatica e semantica concluidas sem erros.\n";
    }

    return 0;
}