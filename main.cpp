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

bool isPalavraReservada(const string& palavra) {
    static vector<string> palavras = {
        "Program", "read", "write", "integer", "boolean", "double", "function", "procedure", "begin", "end",
        "and", "array", "case", "const", "div", "do", "downto", "else", "file", "for", "goto", "if", "in",
        "label", "mod", "nil", "not", "of", "or", "packed", "record", "repeat", "set", "then", "to", "type",
        "until", "with", "var", "while"
    };
    return find(palavras.begin(), palavras.end(), palavra) != palavras.end();
}

string tipoDoLexema(const string& lex) {
    static regex numReal(R"(^\d+\.\d+$)");
    static regex numInt(R"(^\d+$)");
    static regex id(R"(^[a-zA-Z_][a-zA-Z0-9_]*$)");
    static regex numRuim(R"(^\d+\.\d*\.\d+.*$)"); // detecta números com múltiplos pontos

    if (isPalavraReservada(lex)) return "Palavra reservada";
    if (regex_match(lex, numReal)) return "Numero real";
    if (regex_match(lex, numInt)) return "Numero inteiro";
    if (regex_match(lex, id)) return "Identificador";
    if (regex_match(lex, numRuim)) return "numero_invalido";

    static vector<string> simbolosCompostos = { ":=", "<=", ">=", "<>", "++", "--", ".." };
    if (find(simbolosCompostos.begin(), simbolosCompostos.end(), lex) != simbolosCompostos.end()) return "simbolo_composto";

    static vector<string> simbolosSimples = { "+", "-", "*", "/", "=", "^", "<", ">", ";", ".", ":", ",", "(", ")", "{", "}", "[", "]" };
    if (find(simbolosSimples.begin(), simbolosSimples.end(), lex) != simbolosSimples.end()) return "simbolo";

    return "desconhecido";
}

pair<vector<Simbolo>, vector<string>> analisarLexico(const string& arquivo) {
    ifstream arq(arquivo); 
    vector<Simbolo> tabela; //tokens
    vector<string> erros;

    if (!arq.is_open()) {
        erros.push_back("Erro: Nao abriu arquivo '" + arquivo + "'");
        return {tabela, erros};
    }

    string linha;
    int numLinha = 0;
    bool comentAberto = false;

    regex separador(R"((<=|>=|:=|<>|\+\+|--|\.\.|[a-zA-Z_][a-zA-Z0-9_]*|\d+\.\d*\.\d+.*|\d+\.\d+|\d+|[\+\*/=<>;:\.,\(\)\[\]\{\}\^\-]))"); // separa l em lexemas
    regex coment1(R"(\{[^}]*\})");
    regex coment2(R"(\(\*[\s\S]*?\*\))");

    while (getline(arq, linha)) { // linha por linha
        numLinha++;

        if (comentAberto) {
            size_t fimCom = linha.find("*)");
            if (fimCom != string::npos) { // string::npos = não encontrado
                comentAberto = false;
                linha = linha.substr(fimCom + 2); // pula 2 caracteres após o coisa
            } else {
                continue;
            }
        }

        linha = regex_replace(linha, coment1, "");
        linha = regex_replace(linha, coment2, "");

        size_t poscoment1 = linha.find('{');
        if (poscoment1 != string::npos) {
            size_t posFecha1 = linha.find('}', poscoment1);
            if (posFecha1 == string::npos) {
                erros.push_back("Erro lexico linha " + to_string(numLinha) + ": Comentario '{' nao fechado");
                linha = linha.substr(0, poscoment1);
            }
        }

        size_t poscoment2 = linha.find("(*");
        if (poscoment2 != string::npos) {
            size_t posFecha2 = linha.find("*)", poscoment2 + 2);
            if (posFecha2 == string::npos) {
                erros.push_back("Erro lexico linha " + to_string(numLinha) + ": Comentario '(*' nao fechado");
                comentAberto = true;
                linha = linha.substr(0, poscoment2);
            }
        }

        sregex_token_iterator it(linha.begin(), linha.end(), separador, 0); // apenas tokens válidos
        sregex_token_iterator end; 

        for (/*sregex_token_iterator it*/; it != end; ++it) {
            string lex = it->str();
            if (lex.empty()) continue; //espc branco

            string t = tipoDoLexema(lex);

            if (t == "desconhecido" || t == "numero_invalido") {
                string msg = "Erro lexico linha " + to_string(numLinha) + ": ";
                if (t == "numero_invalido") msg += "Numero invalido '" + lex + "'";
                else if (lex.length() == 1) msg += "Caractere '" + lex + "' nao identificado";
                else msg += "Sequencia '" + lex + "' nao identificada";
                erros.push_back(msg);
            } else {
                tabela.push_back({lex, t, numLinha}); //add na tabela
            }
        }
    }

    arq.close();
    return {tabela, erros};
}

class Sintatico {
    const vector<Simbolo>& tokens;
    vector<string> erros;
    int posToken;

    Simbolo atual() {
        if (posToken < (int)tokens.size()) return tokens[posToken];
        return {"EOF", "EOF", -1};
    }

    void avanca() {
        if (posToken < (int)tokens.size()) posToken++;
    }

    bool casa(const string& esperado, bool lexema = true) { //esperado -> lex or tipo
        if (posToken >= (int)tokens.size()) {
            int linhaErro = tokens.empty() ? 0 : tokens.back().linha;
            erros.push_back("Erro sintatico linha " + to_string(linhaErro) + ": Esperava '" + esperado + "'\n");
            return false;
        }

        Simbolo a = atual();
        if ((lexema && a.lexema == esperado) || (!lexema && a.tipo == esperado)) {
            avanca();
            return true;
        } else { // a != esperado
            string tipoEsperado = lexema ? "lexema" : "tipo";
            erros.push_back("Erro sintatico linha " + to_string(a.linha) + ": Esperava " + tipoEsperado + " '" + esperado + "', achou '" + a.lexema + "'\n");
            avanca();
            return false;
        }
    }

    void sincroniza(const vector<string>& pontos) {
        while (posToken < (int)tokens.size()) {
            if (find(pontos.begin(), pontos.end(), atual().lexema) != pontos.end() || atual().tipo == "EOF") break;
            avanca();
        }
    }

    void prog() { // program -> id; -> bloco.
        if (!casa("Program")) { sincroniza({";", ".", "begin"}); return; }
        if (!casa("Identificador", false)) { sincroniza({";", ".", "begin"}); return; }
        if (!casa(";")) { sincroniza({"begin", "."}); return; }
        bloco();
        if (!casa(".")) { sincroniza({}); return; }

        if (posToken < (int)tokens.size()) {
            erros.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Coisa a mais depois do fim do programa");
            sincroniza({});
        }
    }

    void bloco() { // declVar -> begin -> tk_comand -> end
        if (atual().lexema == "var") declVar(); // se prox = var
        if (!casa("begin")) { sincroniza({"end", "."}); return; }
        listaComandos();
        if (!casa("end")) { sincroniza({"."}); return; }
    }

    void declVar() { // declVar -> var -> listaIds: -> tipo;
        if (!casa("var")) { sincroniza({"begin", "end"}); return; }
        while (atual().tipo == "Identificador") {
            listaIds();
            if (!casa(":")) { sincroniza({";", "begin", "end"}); continue; }
            if (!tipo()) { sincroniza({";", "begin", "end"}); continue; }
            if (!casa(";")) { sincroniza({"Identificador", "begin", "end"}); continue; }
        }
    }

    void listaIds() { // id , id
        if (!casa("Identificador", false)) { sincroniza({":", ";", "begin", "end"}); return; }
        while (atual().lexema == ",") {
            avanca();
            if (!casa("Identificador", false)) { sincroniza({":", ";", "begin", "end"}); return; }
        }
    }

    bool tipo() { 
        string lex = atual().lexema;
        if (lex == "integer" || lex == "boolean" || lex == "double") {
            avanca();
            return true;
        }
        erros.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Esperava tipo valido, achou '" + lex + "'");
        return false;
    }

    void listaComandos() { // comand ; comand
        while (atual().lexema != "end" && atual().tipo != "EOF") {
            comando();
            if (atual().lexema == ";") {
                avanca();
            } else if (atual().lexema != "end" && atual().tipo != "EOF") {
                erros.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Esperava ';', achou '" + atual().lexema + "'\n");
                sincroniza({";", "end"});
            }
        }
    }

    void comando() {
        string lex = atual().lexema;
        string tipo = atual().tipo;

        if (tipo == "Identificador") {
            if (posToken + 1 < (int)tokens.size() && tokens[posToken + 1].lexema == ":=") { // se :=  atribuiçãop
                atribuicao();
            } else {
                erros.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Identificador sem atribuicao");
                avanca();
                sincroniza({";", "end"});
            }
        } else if (lex == "read") leitura();
        else if (lex == "write") escrita();
        else if (lex == "if") se();
        else if (lex == "while") enquanto();
        else if (lex == "begin") blocoInicioFim();
        else if (lex == "end" || lex == ";" || tipo == "EOF") {} // fim de bloco
        else {
            erros.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Comando estranho com '" + lex + "'");
            avanca();
            sincroniza({";", "end"});
        }
    }

    void atribuicao() {
        if (!casa("Identificador", false)) { sincroniza({";", "end"}); return; }
        if (!casa(":=")) { sincroniza({";", "end"}); return; }
        expressao();
    }

    void leitura() {
        if (!casa("read")) { sincroniza({";", "end"}); return; }
        if (!casa("(")) { sincroniza({";", "end"}); return; }
        listaIds();
        if (!casa(")")) { sincroniza({";", "end"}); return; }
    }

    void escrita() {
        if (!casa("write")) { sincroniza({";", "end"}); return; }
        if (!casa("(")) { sincroniza({";", "end"}); return; }
        listaExp();
        if (!casa(")")) { sincroniza({";", "end"}); return; }
    }

    void se() { // if -> expr(then) -> comand(else)
        if (!casa("if")) { sincroniza({";", "end"}); return; }
        expressao();
        if (!casa("then")) { sincroniza({";", "end"}); return; }
        comando();
        if (atual().lexema == "else") {
            avanca();
            comando();
        }
    }

    void enquanto() {
        if (!casa("while")) { sincroniza({";", "end"}); return; }
        expressao();
        if (!casa("do")) { sincroniza({";", "end"}); return; }
        comando();
    }

    void blocoInicioFim() {
        if (!casa("begin")) { sincroniza({"end", ";"}); return; }
        listaComandos();
        if (!casa("end")) { sincroniza({";", "end"}); return; }
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

        if (t == "Identificador" || t == "Numero inteiro" || t == "Numero real") {
            avanca();
        } else if (lex == "true" || lex == "false") {
            avanca();
        } else if (lex == "(") {
            avanca();
            expressao();
            if (!casa(")")) { sincroniza({";", "end"}); return; }
        } else if (lex == "not") {
            avanca();
            fator();
        } else {
            erros.push_back("Erro sintatico linha " + to_string(atual().linha) + ": Token inesperado '" + lex + "'");
            avanca();
            sincroniza({";", "end"});
        }
    }

public:
    Sintatico(const vector<Simbolo>& toks) : tokens(toks), posToken(0) {}

    void analisar() {
        if (tokens.empty()) {
            erros.push_back("Erro sintatico: Nao tem token nenhum");
            return;
        }
        prog(); // começa com Program ?
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
        cerr << "Erro: Nao abriu arquivo de saida '" << saida << "'";
        return 1;
    }

    arqSaida << setw(15) << "Lexema" << setw(20) << "Tipo" << setw(10) << "Linha" << "\n";
    arqSaida << string(45, '-') << "\n";
    for (auto& simb : tabela) {
        arqSaida << setw(15) << simb.lexema << setw(20) << simb.tipo << setw(10) << simb.linha << "\n";
    }
    if (!errosLex.empty()) {
        arqSaida << "\nErros Lexicos:\n";
        for (auto& e : errosLex) arqSaida << e << "\n";
    }
    arqSaida.close();

    cout << "Analise lexica terminada. Tabela salva em '" << saida << "'.\n";

    if (!errosLex.empty()) {
        cout << "- Erros Lexicos -\n";
        for (auto& e : errosLex) cout << e << "\n";
    }

    if (tabela.empty() && !errosLex.empty() && errosLex[0].find("Nao abriu arquivo") != string::npos) {
        cout << "Por favor, verifique se o arquivo 'codigo.txt' existe no diretorio correto.\n";
        return 1;
    }

    cout << "- Analise Sintatica -\n";
    Sintatico sint(tabela);
    sint.analisar();

    vector<string> errosSint = sint.getErros();

    if (!errosSint.empty()) {
        cout << "\n- Erros Sintaticos -\n";
        for (auto& e : errosSint) cout << e;
        ofstream arqSaidaErro(saida, ios::app);
        if (arqSaidaErro.is_open()) {
            arqSaidaErro << "\nErros Sintaticos:\n";
            for (auto& e : errosSint) arqSaidaErro << e;
            arqSaidaErro.close();
        }
    } else {
        cout << "Analise sintatica concluida sem erros.\n";
    }

    return 0;
}