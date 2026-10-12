#include "../src/compiler.hpp"
#include "../src/lexer.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void fail(const std::string& test, const std::string& message) {
    ++failures;
    std::cerr << "FALHA [" << test << "]: " << message << '\n';
}

std::vector<std::string> analyzeSource(const std::string& source) {
    Lexer lexer(source);
    const std::vector<Token> tokens = lexer.scanAll();
    if (!lexer.errors().empty()) {
        return {"erro lexico"};
    }
    return analyze(tokens, lexer);
}

std::string joinErrors(const std::vector<std::string>& errors) {
    std::string all;
    for (const std::string& error : errors) all += "\n  " + error;
    return all;
}

void expectValid(const std::string& test, const std::string& source) {
    const std::vector<std::string> errors = analyzeSource(source);
    if (!errors.empty()) {
        fail(test, "erros inesperados:" + joinErrors(errors));
    }
}

// O programa deve gerar exatamente um erro contendo `fragment`.
void expectError(const std::string& test, const std::string& source,
                 const std::string& fragment) {
    const std::vector<std::string> errors = analyzeSource(source);
    if (errors.size() != 1 || errors[0].find(fragment) == std::string::npos) {
        fail(test, "esperado um erro com '" + fragment + "', obtido:" + joinErrors(errors));
    }
}

void testTable() {
    const LalrTable& table = sslParseTable();
    if (table.shiftReduceConflicts != 1 || table.reduceReduceConflicts != 0) {
        fail("tabela", "esperado 1 conflito empilha/reduz (else pendente) e 0 reduz/reduz, obtido " +
                           std::to_string(table.shiftReduceConflicts) + " e " +
                           std::to_string(table.reduceReduceConflicts));
    }
}

void testValidPrograms() {
    // Exemplo 7.4.1 do livro, com while sem bloco (B exige ao menos uma declaração).
    expectValid("fatorial",
                "function fatorial(n: integer): integer {\n"
                "    var i, f: integer;\n"
                "    i = 2;\n"
                "    f = 1;\n"
                "    while (i <= n) f = f * i++;\n"
                "    return f;\n"
                "}\n"
                "function somaFat(n: integer): integer {\n"
                "    var i, s: integer;\n"
                "    i = 0;\n"
                "    s = 0;\n"
                "    while (++i <= n) s = s + fatorial(i);\n"
                "    return s;\n"
                "}\n");
    expectValid("controle",
                "function g(n: integer): integer {\n"
                "    var i: integer;\n"
                "    i = 0;\n"
                "    do {\n"
                "        var t: integer;\n"
                "        if (i == 5) break; else continue;\n"
                "    } while (i < n);\n"
                "    if (true) if (false) i = 1; else i = 2;\n"
                "    return i;\n"
                "}\n");
    expectValid("tipos estruturados",
                "type V = array [3] of integer\n"
                "type P = struct { a: integer; v: V; c: char }\n"
                "type Inteiro = integer\n"
                "function h(p: P, s: string): Inteiro {\n"
                "    var x: integer;\n"
                "    var q: P;\n"
                "    p.v[2] = p.a;\n"
                "    x = p.v[1];\n"
                "    s[x] = p.c;\n"
                "    q = p;\n"
                "    s = s + \"!\";\n"
                "    return x;\n"
                "}\n");
}

void testErrors() {
    const std::string head = "type V = array [3] of integer\n"
                             "type P = struct { i: integer }\n"
                             "function f(x: integer): integer {\n"
                             "    var a: integer;\n"
                             "    var s: string;\n"
                             "    var v: V;\n"
                             "    var p: P;\n";
    const std::string tail = "}\n";

    expectError("sintatico", head + "    a = 1\n" + tail, "erro sintatico");
    expectError("redeclaracao", head + "    var a: char;\n    a = 1;\n" + tail, "[ERR_REDECL]");
    expectError("nao declarado", head + "    b = 1;\n" + tail, "[ERR_NOT_DECL]");
    expectError("tipo esperado", head + "    var b: x;\n    a = 1;\n" + tail, "[ERR_TYPE_EXPECTED]");
    expectError("condicao", head + "    if (a) a = 1;\n" + tail, "[ERR_BOOL_TYPE_EXPECTED]");
    expectError("atribuicao", head + "    a = 'c';\n" + tail, "[ERR_TYPE_MISMATCH]");
    expectError("caractere de string", head + "    a = s[0];\n" + tail, "(integer e char)");
    expectError("operador", head + "    a = -true;\n" + tail, "[ERR_INVALID_TYPE]");
    expectError("nao struct", head + "    a.i = 1;\n" + tail, "[ERR_KIND_NOT_STRUCT]");
    expectError("campo", head + "    p.j = 1;\n" + tail, "[ERR_FIELD_NOT_DECL]");
    expectError("nao array", head + "    a[0] = 1;\n" + tail, "[ERR_KIND_NOT_ARRAY]");
    expectError("indice", head + "    v['c'] = 1;\n" + tail, "[ERR_INVALID_INDEX_TYPE]");
    expectError("nao variavel", head + "    f = 1;\n" + tail, "[ERR_KIND_NOT_VAR]");
    expectError("nao funcao", head + "    a = x(1);\n" + tail, "[ERR_KIND_NOT_FUNC]");
    expectError("argumentos demais", head + "    a = f(1, 2);\n" + tail, "[ERR_TOO_MANY_ARGS]");
    expectError("tipo do argumento", head + "    a = f('c');\n" + tail, "[ERR_PARAM_TYPE]");
    expectError("argumentos de menos",
                "function g(x: integer, y: integer): integer {\n"
                "    var a: integer;\n    a = g(1);\n}\n",
                "[ERR_TOO_FEW_ARGS]");
    expectError("retorno", head + "    return 'c';\n" + tail, "retorno de 'f'");
    expectError("break fora de laco", head + "    break;\n" + tail, "break fora");
    expectError("continue fora de laco", head + "    while (true) a = 1;\n    continue;\n" + tail,
                "continue fora");
    expectError("campo fora do escopo", head + "    i = 1;\n" + tail, "[ERR_NOT_DECL]");
    expectError("bloco vazio", "function f(x: integer): integer { x = 1; }\n", "erro sintatico");
}

void testScopes() {
    // Bloco interno pode redeclarar nome externo; parâmetros e locais da
    // função compartilham o mesmo nível; recursão é permitida.
    expectValid("escopos",
                "function f(x: integer): integer {\n"
                "    var y: integer;\n"
                "    { var x: char; x = 'a'; }\n"
                "    y = f(x);\n"
                "    return y;\n"
                "}\n");
    expectError("parametro e local no mesmo nivel",
                "function f(x: integer): integer { var x: integer; x = 1; }\n",
                "[ERR_REDECL]");
    expectError("variavel de bloco interno some no fim do bloco",
                "function f(x: integer): integer {\n"
                "    var y: integer;\n"
                "    { var z: integer; z = 1; }\n"
                "    z = 2;\n"
                "}\n",
                "[ERR_NOT_DECL]");
}

}  // namespace

int main() {
    testTable();
    testValidPrograms();
    testErrors();
    testScopes();

    if (failures != 0) {
        std::cerr << failures << " teste(s) falharam.\n";
        return EXIT_FAILURE;
    }

    std::cout << "Todos os testes do compilador passaram.\n";
    return EXIT_SUCCESS;
}
