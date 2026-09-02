#include "lexer.hpp"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace {

bool isAsciiLetter(char c) {
    const unsigned char value = static_cast<unsigned char>(c);
    return (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z');
}

bool isAsciiDigit(char c) {
    const unsigned char value = static_cast<unsigned char>(c);
    return value >= '0' && value <= '9';
}

bool isWhitespace(char c) {
    switch (c) {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
        case '\v':
        case '\f':
            return true;
        default:
            return false;
    }
}

const std::unordered_map<std::string, TokenKind> KEYWORDS = {
    {"array", TokenKind::ARRAY},
    {"boolean", TokenKind::BOOLEAN},
    {"break", TokenKind::BREAK},
    {"char", TokenKind::CHAR},
    {"continue", TokenKind::CONTINUE},
    {"do", TokenKind::DO},
    {"else", TokenKind::ELSE},
    {"false", TokenKind::FALSE_LITERAL},
    {"function", TokenKind::FUNCTION},
    {"if", TokenKind::IF},
    {"integer", TokenKind::INTEGER},
    {"of", TokenKind::OF},
    {"string", TokenKind::STRING},
    {"struct", TokenKind::STRUCT},
    {"true", TokenKind::TRUE_LITERAL},
    {"type", TokenKind::TYPE},
    {"var", TokenKind::VAR},
    {"while", TokenKind::WHILE},
};

}  // namespace

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

std::vector<Token> Lexer::scanAll() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespace();

        if (atEnd()) {
            tokens.push_back(Token{TokenKind::END_OF_FILE, "", line_, column_,
                                   false, 0});
            return tokens;
        }

        const char current = peek();
        if (isAsciiLetter(current)) {
            tokens.push_back(scanIdentifierOrKeyword());
        } else if (isAsciiDigit(current)) {
            tokens.push_back(scanNumeral());
        } else if (current == '\'') {
            Token token{TokenKind::END_OF_FILE, "", 0, 0, false, 0};
            if (scanCharacter(token)) {
                tokens.push_back(std::move(token));
            }
        } else if (current == '"') {
            Token token{TokenKind::END_OF_FILE, "", 0, 0, false, 0};
            if (scanString(token)) {
                tokens.push_back(std::move(token));
            }
        } else {
            Token token{TokenKind::END_OF_FILE, "", 0, 0, false, 0};
            if (scanSymbol(token)) {
                tokens.push_back(std::move(token));
            }
        }
    }
}

const std::vector<LexicalError>& Lexer::errors() const {
    return errors_;
}

const std::vector<std::string>& Lexer::identifiers() const {
    return identifiers_;
}

const std::vector<ConstantEntry>& Lexer::constants() const {
    return constants_;
}

bool Lexer::atEnd() const {
    return current_ >= source_.size();
}

char Lexer::peek(std::size_t offset) const {
    const std::size_t position = current_ + offset;
    return position < source_.size() ? source_[position] : '\0';
}

char Lexer::advance() {
    if (atEnd()) {
        return '\0';
    }

    const char c = source_[current_++];
    if (c == '\r') {
        // CRLF representa uma única quebra de linha.
        if (!atEnd() && source_[current_] == '\n') {
            ++current_;
        }
        ++line_;
        column_ = 1;
    } else if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (atEnd() || peek() != expected) {
        return false;
    }
    advance();
    return true;
}

void Lexer::skipWhitespace() {
    while (!atEnd() && isWhitespace(peek())) {
        advance();
    }
}

Token Lexer::scanIdentifierOrKeyword() {
    const std::size_t start = current_;
    const std::size_t line = line_;
    const std::size_t column = column_;

    advance();
    while (isAsciiLetter(peek()) || isAsciiDigit(peek())) {
        advance();
    }

    const std::string lexeme = source_.substr(start, current_ - start);
    const auto keyword = KEYWORDS.find(lexeme);
    if (keyword != KEYWORDS.end()) {
        return makeToken(keyword->second, start, line, column);
    }

    return makeToken(TokenKind::IDENTIFIER, start, line, column,
                     true, internIdentifier(lexeme));
}

Token Lexer::scanNumeral() {
    const std::size_t start = current_;
    const std::size_t line = line_;
    const std::size_t column = column_;

    do {
        advance();
    } while (isAsciiDigit(peek()));

    const std::string value = source_.substr(start, current_ - start);
    return makeToken(TokenKind::NUMERAL, start, line, column,
                     true, addConstant(TokenKind::NUMERAL, value));
}

bool Lexer::scanCharacter(Token& token) {
    const std::size_t start = current_;
    const std::size_t line = line_;
    const std::size_t column = column_;
    advance();  // aspa inicial

    if (atEnd()) {
        addError("literal de caractere nao terminado", start, line, column);
        return false;
    }

    if (peek() == '\'') {
        advance();
        addError("literal de caractere vazio; era esperado exatamente um caractere",
                 start, line, column);
        return false;
    }

    const std::size_t valueStart = current_;
    advance();  // exatamente um caractere segundo c = '\'' . any . '\''
    const std::string value = source_.substr(valueStart, current_ - valueStart);

    if (atEnd()) {
        addError("literal de caractere nao terminado", start, line, column);
        return false;
    }

    if (peek() != '\'') {
        while (!atEnd() && peek() != '\'' && peek() != '\n' && peek() != '\r') {
            advance();
        }
        const bool foundClosingQuote = !atEnd() && peek() == '\'';
        if (foundClosingQuote) {
            advance();
        }
        addError(foundClosingQuote
                     ? "literal de caractere deve conter exatamente um caractere"
                     : "literal de caractere nao terminado",
                 start, line, column);
        return false;
    }

    advance();  // aspa final
    token = makeToken(TokenKind::CHARACTER, start, line, column,
                      true, addConstant(TokenKind::CHARACTER, value));
    return true;
}

bool Lexer::scanString(Token& token) {
    const std::size_t start = current_;
    const std::size_t line = line_;
    const std::size_t column = column_;
    advance();  // aspa inicial
    const std::size_t valueStart = current_;

    while (!atEnd() && peek() != '"') {
        advance();
    }

    if (atEnd()) {
        addError("literal de string nao terminado", start, line, column);
        return false;
    }

    const std::string value = source_.substr(valueStart, current_ - valueStart);
    advance();  // aspa final
    token = makeToken(TokenKind::STRING_VALUE, start, line, column,
                      true, addConstant(TokenKind::STRING_VALUE, value));
    return true;
}

bool Lexer::scanSymbol(Token& token) {
    const std::size_t start = current_;
    const std::size_t line = line_;
    const std::size_t column = column_;
    const char c = advance();

    switch (c) {
        case ':': token = makeToken(TokenKind::COLON, start, line, column); break;
        case ';': token = makeToken(TokenKind::SEMICOLON, start, line, column); break;
        case ',': token = makeToken(TokenKind::COMMA, start, line, column); break;
        case '[': token = makeToken(TokenKind::LEFT_SQUARE, start, line, column); break;
        case ']': token = makeToken(TokenKind::RIGHT_SQUARE, start, line, column); break;
        case '{': token = makeToken(TokenKind::LEFT_BRACE, start, line, column); break;
        case '}': token = makeToken(TokenKind::RIGHT_BRACE, start, line, column); break;
        case '(': token = makeToken(TokenKind::LEFT_PARENTHESIS, start, line, column); break;
        case ')': token = makeToken(TokenKind::RIGHT_PARENTHESIS, start, line, column); break;
        case '.': token = makeToken(TokenKind::DOT, start, line, column); break;
        case '=':
            token = makeToken(match('=') ? TokenKind::EQUAL_EQUAL : TokenKind::ASSIGN,
                              start, line, column);
            break;
        case '<':
            token = makeToken(match('=') ? TokenKind::LESS_OR_EQUAL : TokenKind::LESS_THAN,
                              start, line, column);
            break;
        case '>':
            token = makeToken(match('=') ? TokenKind::GREATER_OR_EQUAL : TokenKind::GREATER_THAN,
                              start, line, column);
            break;
        case '!':
            token = makeToken(match('=') ? TokenKind::NOT_EQUAL : TokenKind::NOT,
                              start, line, column);
            break;
        case '+':
            token = makeToken(match('+') ? TokenKind::PLUS_PLUS : TokenKind::PLUS,
                              start, line, column);
            break;
        case '-':
            token = makeToken(match('-') ? TokenKind::MINUS_MINUS : TokenKind::MINUS,
                              start, line, column);
            break;
        case '*': token = makeToken(TokenKind::TIMES, start, line, column); break;
        case '/': token = makeToken(TokenKind::DIVIDE, start, line, column); break;
        case '&':
            if (match('&')) {
                token = makeToken(TokenKind::AND, start, line, column);
                break;
            }
            addError("'&' isolado nao e token valido; use '&&'", start, line, column);
            return false;
        case '|':
            if (match('|')) {
                token = makeToken(TokenKind::OR, start, line, column);
                break;
            }
            addError("'|' isolado nao e token valido; use '||'", start, line, column);
            return false;
        default:
            addError("caractere nao pertence ao alfabeto lexico da linguagem",
                     start, line, column);
            return false;
    }
    return true;
}

Token Lexer::makeToken(TokenKind kind, std::size_t start,
                       std::size_t line, std::size_t column,
                       bool hasSecondary, std::size_t secondary) const {
    return Token{kind, source_.substr(start, current_ - start), line, column,
                 hasSecondary, secondary};
}

void Lexer::addError(std::string message, std::size_t start,
                     std::size_t line, std::size_t column) {
    errors_.push_back(LexicalError{std::move(message),
                                  source_.substr(start, current_ - start),
                                  line, column});
}

std::size_t Lexer::internIdentifier(const std::string& lexeme) {
    const auto found = identifierIndexes_.find(lexeme);
    if (found != identifierIndexes_.end()) {
        return found->second;
    }

    const std::size_t index = identifiers_.size();
    identifiers_.push_back(lexeme);
    identifierIndexes_.emplace(lexeme, index);
    return index;
}

std::size_t Lexer::addConstant(TokenKind kind, std::string value) {
    const std::size_t index = constants_.size();
    constants_.push_back(ConstantEntry{kind, std::move(value)});
    return index;
}

std::string tokenKindName(TokenKind kind) {
    switch (kind) {
        case TokenKind::ARRAY: return "ARRAY";
        case TokenKind::BOOLEAN: return "BOOLEAN";
        case TokenKind::BREAK: return "BREAK";
        case TokenKind::CHAR: return "CHAR";
        case TokenKind::CONTINUE: return "CONTINUE";
        case TokenKind::DO: return "DO";
        case TokenKind::ELSE: return "ELSE";
        case TokenKind::FALSE_LITERAL: return "FALSE";
        case TokenKind::FUNCTION: return "FUNCTION";
        case TokenKind::IF: return "IF";
        case TokenKind::INTEGER: return "INTEGER";
        case TokenKind::OF: return "OF";
        case TokenKind::STRING: return "STRING";
        case TokenKind::STRUCT: return "STRUCT";
        case TokenKind::TRUE_LITERAL: return "TRUE";
        case TokenKind::TYPE: return "TYPE";
        case TokenKind::VAR: return "VAR";
        case TokenKind::WHILE: return "WHILE";
        case TokenKind::IDENTIFIER: return "ID";
        case TokenKind::NUMERAL: return "NUMERAL";
        case TokenKind::CHARACTER: return "CHARACTER";
        case TokenKind::STRING_VALUE: return "STRINGVAL";
        case TokenKind::ASSIGN: return "EQUALS";
        case TokenKind::AND: return "AND";
        case TokenKind::OR: return "OR";
        case TokenKind::LESS_THAN: return "LESS_THAN";
        case TokenKind::GREATER_THAN: return "GREATER_THAN";
        case TokenKind::LESS_OR_EQUAL: return "LESS_OR_EQUAL";
        case TokenKind::GREATER_OR_EQUAL: return "GREATER_OR_EQUAL";
        case TokenKind::EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TokenKind::NOT_EQUAL: return "NOT_EQUAL";
        case TokenKind::PLUS: return "PLUS";
        case TokenKind::PLUS_PLUS: return "PLUS_PLUS";
        case TokenKind::MINUS: return "MINUS";
        case TokenKind::MINUS_MINUS: return "MINUS_MINUS";
        case TokenKind::TIMES: return "TIMES";
        case TokenKind::DIVIDE: return "DIVIDE";
        case TokenKind::NOT: return "NOT";
        case TokenKind::COLON: return "COLON";
        case TokenKind::SEMICOLON: return "SEMI_COLON";
        case TokenKind::COMMA: return "COMMA";
        case TokenKind::LEFT_SQUARE: return "LEFT_SQUARE";
        case TokenKind::RIGHT_SQUARE: return "RIGHT_SQUARE";
        case TokenKind::LEFT_BRACE: return "LEFT_BRACES";
        case TokenKind::RIGHT_BRACE: return "RIGHT_BRACES";
        case TokenKind::LEFT_PARENTHESIS: return "LEFT_PARENTHESIS";
        case TokenKind::RIGHT_PARENTHESIS: return "RIGHT_PARENTHESIS";
        case TokenKind::DOT: return "DOT";
        case TokenKind::END_OF_FILE: return "EOF";
    }
    throw std::logic_error("token desconhecido na conversao para nome");
}

std::string escapeForDisplay(const std::string& text) {
    std::ostringstream output;
    for (unsigned char c : text) {
        switch (c) {
            case '\\': output << "\\\\"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            case '\v': output << "\\v"; break;
            case '\f': output << "\\f"; break;
            default:
                if (c >= 32 && c <= 126) {
                    output << static_cast<char>(c);
                } else {
                    output << "\\x" << std::uppercase << std::hex
                           << std::setw(2) << std::setfill('0')
                           << static_cast<int>(c) << std::dec;
                }
        }
    }
    return output.str();
}
