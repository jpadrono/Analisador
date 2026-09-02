#ifndef ANALISADOR_LEXER_HPP
#define ANALISADOR_LEXER_HPP

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

enum class TokenKind {
    // Palavras reservadas.
    ARRAY,
    BOOLEAN,
    BREAK,
    CHAR,
    CONTINUE,
    DO,
    ELSE,
    FALSE_LITERAL,
    FUNCTION,
    IF,
    INTEGER,
    OF,
    STRING,
    STRUCT,
    TRUE_LITERAL,
    TYPE,
    VAR,
    WHILE,

    // Identificadores e literais regulares.
    IDENTIFIER,
    NUMERAL,
    CHARACTER,
    STRING_VALUE,

    // Operadores.
    ASSIGN,
    AND,
    OR,
    LESS_THAN,
    GREATER_THAN,
    LESS_OR_EQUAL,
    GREATER_OR_EQUAL,
    EQUAL_EQUAL,
    NOT_EQUAL,
    PLUS,
    PLUS_PLUS,
    MINUS,
    MINUS_MINUS,
    TIMES,
    DIVIDE,
    NOT,

    // Delimitadores.
    COLON,
    SEMICOLON,
    COMMA,
    LEFT_SQUARE,
    RIGHT_SQUARE,
    LEFT_BRACE,
    RIGHT_BRACE,
    LEFT_PARENTHESIS,
    RIGHT_PARENTHESIS,
    DOT,

    END_OF_FILE
};

struct Token {
    TokenKind kind;
    std::string lexeme;
    std::size_t line;
    std::size_t column;
    bool hasSecondary;
    std::size_t secondary;
};

struct LexicalError {
    std::string message;
    std::string lexeme;
    std::size_t line;
    std::size_t column;
};

struct ConstantEntry {
    TokenKind kind;
    std::string value;
};

class Lexer {
public:
    explicit Lexer(std::string source);

    std::vector<Token> scanAll();

    const std::vector<LexicalError>& errors() const;
    const std::vector<std::string>& identifiers() const;
    const std::vector<ConstantEntry>& constants() const;

private:
    std::string source_;
    std::size_t current_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;

    std::vector<LexicalError> errors_;
    std::vector<std::string> identifiers_;
    std::unordered_map<std::string, std::size_t> identifierIndexes_;
    std::vector<ConstantEntry> constants_;

    bool atEnd() const;
    char peek(std::size_t offset = 0) const;
    char advance();
    bool match(char expected);
    void skipWhitespace();

    Token scanIdentifierOrKeyword();
    Token scanNumeral();
    bool scanCharacter(Token& token);
    bool scanString(Token& token);
    bool scanSymbol(Token& token);

    Token makeToken(TokenKind kind, std::size_t start,
                    std::size_t line, std::size_t column,
                    bool hasSecondary = false, std::size_t secondary = 0) const;
    void addError(std::string message, std::size_t start,
                  std::size_t line, std::size_t column);
    std::size_t internIdentifier(const std::string& lexeme);
    std::size_t addConstant(TokenKind kind, std::string value);
};

std::string tokenKindName(TokenKind kind);
std::string escapeForDisplay(const std::string& text);

#endif
