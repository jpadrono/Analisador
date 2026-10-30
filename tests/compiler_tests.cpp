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

CompileResult compileSource(const std::string& source) {
    Lexer lexer(source);
    const std::vector<Token> tokens = lexer.scanAll();
    if (!lexer.errors().empty()) {
        CompileResult result;
        result.errors.push_back("erro lexico");
        return result;
    }
    return compile(tokens, lexer);
}

std::string joinErrors(const CompileResult& result) {
    std::string all;
    for (const std::string& error : result.errors) all += "\n  " + error;
    return all;
}

void expectCode(const std::string& test, const std::string& source,
                const std::string& expected) {
    const CompileResult result = compileSource(source);
    if (!result.errors.empty()) {
        fail(test, "erros inesperados:" + joinErrors(result));
    } else if (result.code != expected) {
        fail(test, "codigo gerado:\n" + result.code + "esperado:\n" + expected);
    }
}

// O programa deve gerar exatamente um erro contendo `fragment`.
void expectError(const std::string& test, const std::string& source,
                 const std::string& fragment) {
    const CompileResult result = compileSource(source);
    if (result.errors.size() != 1 ||
        result.errors[0].find(fragment) == std::string::npos) {
        fail(test, "esperado um erro com '" + fragment + "', obtido:" + joinErrors(result));
    }
    if (!result.code.empty()) {
        fail(test, "nao deveria haver codigo quando ha erro");
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

// Exemplo 7.4.1 do livro, com while sem bloco (B exige ao menos uma declaração).
void testFactorial() {
    expectCode("fatorial",
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
               "}\n",
               "BEGIN_FUNC 0, 1, 3\n"
               "\tLOAD_REF 1\n\tLOAD_CONST 0\n\tSTORE_REF 1\n"
               "\tLOAD_REF 2\n\tLOAD_CONST 1\n\tSTORE_REF 1\n"
               "L0:\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tLOAD_REF 0\n\tDE_REF 1\n\tLE\n\tTJMP_FW L1\n"
               "\tLOAD_REF 2\n\tLOAD_REF 2\n\tDE_REF 1\n"
               "\tLOAD_REF 1\n\tDUP\n\tDUP\n\tDE_REF 1\n\tINC\n\tSTORE_REF 1\n\tDE_REF 1\n\tDEC\n"
               "\tMUL\n\tSTORE_REF 1\n"
               "\tJMP_BW L0\n"
               "L1:\n"
               "\tLOAD_REF 2\n\tDE_REF 1\n\tRET\n"
               "END_FUNC\n"
               "BEGIN_FUNC 1, 1, 3\n"
               "\tLOAD_REF 1\n\tLOAD_CONST 2\n\tSTORE_REF 1\n"
               "\tLOAD_REF 2\n\tLOAD_CONST 3\n\tSTORE_REF 1\n"
               "L2:\n"
               "\tLOAD_REF 1\n\tDUP\n\tDUP\n\tDE_REF 1\n\tINC\n\tSTORE_REF 1\n\tDE_REF 1\n"
               "\tLOAD_REF 0\n\tDE_REF 1\n\tLE\n\tTJMP_FW L3\n"
               "\tLOAD_REF 2\n\tLOAD_REF 2\n\tDE_REF 1\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tCALL 0\n\tADD\n\tSTORE_REF 1\n"
               "\tJMP_BW L2\n"
               "L3:\n"
               "\tLOAD_REF 2\n\tDE_REF 1\n\tRET\n"
               "END_FUNC\n");
}

// if/else, do-while, break, continue e o else pendente (liga-se ao if interno).
void testControlFlow() {
    expectCode("controle",
               "function g(n: integer): integer {\n"
               "    var i: integer;\n"
               "    i = 0;\n"
               "    do {\n"
               "        var t: integer;\n"
               "        if (i == 5) break; else continue;\n"
               "    } while (i < n);\n"
               "    if (true) if (false) i = 1; else i = 2;\n"
               "    return i;\n"
               "}\n",
               "BEGIN_FUNC 0, 1, 3\n"
               "\tLOAD_REF 1\n\tLOAD_CONST 0\n\tSTORE_REF 1\n"
               "L0:\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tLOAD_CONST 1\n\tEQ\n\tTJMP_FW L3\n"
               "\tJMP_FW L1\n"
               "\tJMP_FW L4\n"
               "L3:\n"
               "\tJMP_FW L2\n"
               "L4:\n"
               "L2:\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tLOAD_REF 0\n\tDE_REF 1\n\tLT\n"
               "\tNOT\n\tTJMP_BW L0\n"
               "L1:\n"
               "\tLOAD_TRUE\n\tTJMP_FW L5\n"
               "\tLOAD_FALSE\n\tTJMP_FW L6\n"
               "\tLOAD_REF 1\n\tLOAD_CONST 2\n\tSTORE_REF 1\n"
               "\tJMP_FW L7\n"
               "L6:\n"
               "\tLOAD_REF 1\n\tLOAD_CONST 3\n\tSTORE_REF 1\n"
               "L7:\n"
               "L5:\n"
               "\tLOAD_REF 1\n\tDE_REF 1\n\tRET\n"
               "END_FUNC\n");
}

// Tamanhos, endereçamento de campos/elementos e caracteres de string.
void testStructuredTypes() {
    expectCode("tipos estruturados",
               "type V = array [3] of integer\n"
               "type P = struct { a: integer; v: V; c: char }\n"
               "type Inteiro = integer\n"
               "function h(p: P, s: string): Inteiro {\n"
               "    var x: integer;\n"
               "    p.v[2] = p.a;\n"
               "    x = p.v[1];\n"
               "    s[x] = p.c;\n"
               "    return x;\n"
               "}\n",
               "BEGIN_FUNC 0, 6, 7\n"
               "\tLOAD_REF 0\n\tADD 1\n\tLOAD_CONST 1\n\tMUL 1\n\tADD\n"
               "\tLOAD_REF 0\n\tADD 0\n\tDE_REF 1\n\tSTORE_REF 1\n"
               "\tLOAD_REF 6\n"
               "\tLOAD_REF 0\n\tADD 1\n\tLOAD_CONST 2\n\tMUL 1\n\tADD\n\tDE_REF 1\n"
               "\tSTORE_REF 1\n"
               "\tLOAD_REF 5\n\tLOAD_REF 6\n\tDE_REF 1\n"
               "\tLOAD_REF 0\n\tADD 4\n\tDE_REF 1\n"
               "\tCALL_LIB 0, 1\n\tPOP\n"
               "\tLOAD_REF 6\n\tDE_REF 1\n\tRET\n"
               "END_FUNC\n");
    expectCode("copia de struct e leitura de caractere",
               "type P = struct { a, b: integer }\n"
               "function k(p: P, s: string): char {\n"
               "    var q: P;\n"
               "    q = p;\n"
               "    return s[q.b];\n"
               "}\n",
               "BEGIN_FUNC 0, 3, 5\n"
               "\tLOAD_REF 3\n\tLOAD_REF 0\n\tDE_REF 2\n\tSTORE_REF 2\n"
               "\tLOAD_REF 2\n\tLOAD_REF 3\n\tADD 1\n\tDE_REF 1\n\tCALL_LIB 0, 0\n\tRET\n"
               "END_FUNC\n");
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
    const CompileResult result = compileSource(
        "function f(x: integer): integer {\n"
        "    var y: integer;\n"
        "    { var x: char; x = 'a'; }\n"
        "    y = f(x);\n"
        "    return y;\n"
        "}\n");
    if (!result.errors.empty()) {
        fail("escopos", "erros inesperados:" + joinErrors(result));
    }
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
    testFactorial();
    testControlFlow();
    testStructuredTypes();
    testErrors();
    testScopes();

    if (failures != 0) {
        std::cerr << failures << " teste(s) falharam.\n";
        return EXIT_FAILURE;
    }

    std::cout << "Todos os testes do compilador passaram.\n";
    return EXIT_SUCCESS;
}
