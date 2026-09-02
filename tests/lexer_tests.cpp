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

void expectKinds(const std::string& test, const std::string& source,
                 const std::vector<TokenKind>& expected,
                 std::size_t expectedErrors = 0) {
    Lexer lexer(source);
    const std::vector<Token> actual = lexer.scanAll();

    if (lexer.errors().size() != expectedErrors) {
        fail(test, "quantidade de erros esperada = " +
                       std::to_string(expectedErrors) + ", obtida = " +
                       std::to_string(lexer.errors().size()));
    }

    if (actual.size() != expected.size() + 1) {
        fail(test, "quantidade de tokens (incluindo EOF) incorreta");
        return;
    }

    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (actual[i].kind != expected[i]) {
            fail(test, "token " + std::to_string(i) + ": esperado " +
                           tokenKindName(expected[i]) + ", obtido " +
                           tokenKindName(actual[i].kind));
        }
    }
    if (actual.back().kind != TokenKind::END_OF_FILE) {
        fail(test, "fluxo nao terminou com EOF");
    }
}

void testKeywords() {
    expectKinds(
        "palavras reservadas",
        "array boolean break char continue do else false function if integer "
        "of string struct true type var while",
        {TokenKind::ARRAY, TokenKind::BOOLEAN, TokenKind::BREAK,
         TokenKind::CHAR, TokenKind::CONTINUE, TokenKind::DO,
         TokenKind::ELSE, TokenKind::FALSE_LITERAL, TokenKind::FUNCTION,
         TokenKind::IF, TokenKind::INTEGER, TokenKind::OF, TokenKind::STRING,
         TokenKind::STRUCT, TokenKind::TRUE_LITERAL, TokenKind::TYPE,
         TokenKind::VAR, TokenKind::WHILE});
}

void testRegularTokens() {
    Lexer lexer("Nome9 Nome9 007 'A' \"texto\"");
    const std::vector<Token> tokens = lexer.scanAll();
    const std::vector<TokenKind> expected = {
        TokenKind::IDENTIFIER, TokenKind::IDENTIFIER, TokenKind::NUMERAL,
        TokenKind::CHARACTER, TokenKind::STRING_VALUE, TokenKind::END_OF_FILE};

    if (!lexer.errors().empty() || tokens.size() != expected.size()) {
        fail("tokens regulares", "fluxo ou erros inesperados");
        return;
    }
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (tokens[i].kind != expected[i]) {
            fail("tokens regulares", "categoria incorreta na posicao " +
                                         std::to_string(i));
        }
    }
    if (!tokens[0].hasSecondary || tokens[0].secondary != tokens[1].secondary) {
        fail("tokens regulares", "identificador repetido deveria reutilizar indice");
    }
    if (lexer.constants().size() != 3) {
        fail("tokens regulares", "tabela de constantes deveria ter tres entradas");
    }
}

void testSymbols() {
    expectKinds(
        "operadores e delimitadores",
        "= && || < > <= >= == != + ++ - -- * / ! : ; , [ ] { } ( ) .",
        {TokenKind::ASSIGN, TokenKind::AND, TokenKind::OR,
         TokenKind::LESS_THAN, TokenKind::GREATER_THAN,
         TokenKind::LESS_OR_EQUAL, TokenKind::GREATER_OR_EQUAL,
         TokenKind::EQUAL_EQUAL, TokenKind::NOT_EQUAL, TokenKind::PLUS,
         TokenKind::PLUS_PLUS, TokenKind::MINUS, TokenKind::MINUS_MINUS,
         TokenKind::TIMES, TokenKind::DIVIDE, TokenKind::NOT,
         TokenKind::COLON, TokenKind::SEMICOLON, TokenKind::COMMA,
         TokenKind::LEFT_SQUARE, TokenKind::RIGHT_SQUARE,
         TokenKind::LEFT_BRACE, TokenKind::RIGHT_BRACE,
         TokenKind::LEFT_PARENTHESIS, TokenKind::RIGHT_PARENTHESIS,
         TokenKind::DOT});
}

void testWhitespaceAndPositions() {
    Lexer lexer("  var\r\n\tNome");
    const std::vector<Token> tokens = lexer.scanAll();
    if (!lexer.errors().empty() || tokens.size() != 3) {
        fail("espacos", "fluxo inesperado");
        return;
    }
    if (tokens[0].line != 1 || tokens[0].column != 3 ||
        tokens[1].line != 2 || tokens[1].column != 2) {
        fail("espacos", "linha/coluna incorreta para CRLF ou tabulacao");
    }
}

void testErrors() {
    expectKinds("underscore", "nome_invalido", {TokenKind::IDENTIFIER,
                                                   TokenKind::IDENTIFIER}, 1);
    expectKinds("operadores isolados", "& |", {}, 2);
    expectKinds("caractere desconhecido", "@", {}, 1);
    expectKinds("caractere vazio", "''", {}, 1);
    expectKinds("caractere longo", "'ab'", {}, 1);
    expectKinds("caractere sem fechamento", "'a", {}, 1);
    expectKinds("string sem fechamento", "\"abc", {}, 1);
}

void testNoCommentsWereInvented() {
    expectKinds("comentarios ausentes", "//abc",
                {TokenKind::DIVIDE, TokenKind::DIVIDE, TokenKind::IDENTIFIER});
}

}  // namespace

int main() {
    testKeywords();
    testRegularTokens();
    testSymbols();
    testWhitespaceAndPositions();
    testErrors();
    testNoCommentsWereInvented();

    if (failures != 0) {
        std::cerr << failures << " teste(s) falharam.\n";
        return EXIT_FAILURE;
    }

    std::cout << "Todos os testes lexicos passaram.\n";
    return EXIT_SUCCESS;
}
