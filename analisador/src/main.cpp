#include "lexer.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

std::string readInput(const std::string& path) {
    if (path == "-") {
        return std::string(std::istreambuf_iterator<char>(std::cin),
                           std::istreambuf_iterator<char>());
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("nao foi possivel abrir o arquivo: " + path);
    }

    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

void printTokens(const std::vector<Token>& tokens) {
    std::cout << std::left
              << std::setw(8) << "POSICAO"
              << std::setw(22) << "TOKEN"
              << std::setw(8) << "SEC."
              << "LEXEMA\n";
    std::cout << std::string(70, '-') << '\n';

    for (const Token& token : tokens) {
        const std::string position = std::to_string(token.line) + ":" +
                                     std::to_string(token.column);
        const std::string secondary = token.hasSecondary
                                          ? std::to_string(token.secondary)
                                          : "-";
        const std::string lexeme = token.kind == TokenKind::END_OF_FILE
                                       ? "<fim do arquivo>"
                                       : escapeForDisplay(token.lexeme);
        std::cout << std::setw(8) << position
                  << std::setw(22) << tokenKindName(token.kind)
                  << std::setw(8) << secondary
                  << lexeme << '\n';
    }
}

void printTables(const Lexer& lexer) {
    std::cout << "\nTABELA DE IDENTIFICADORES\n";
    if (lexer.identifiers().empty()) {
        std::cout << "(vazia)\n";
    } else {
        for (std::size_t i = 0; i < lexer.identifiers().size(); ++i) {
            std::cout << i << ": " << escapeForDisplay(lexer.identifiers()[i]) << '\n';
        }
    }

    std::cout << "\nTABELA DE CONSTANTES\n";
    if (lexer.constants().empty()) {
        std::cout << "(vazia)\n";
    } else {
        for (std::size_t i = 0; i < lexer.constants().size(); ++i) {
            const ConstantEntry& entry = lexer.constants()[i];
            std::cout << i << ": " << tokenKindName(entry.kind)
                      << " = " << escapeForDisplay(entry.value) << '\n';
        }
    }
}

void printUsage(const char* executable) {
    std::cerr << "Uso: " << executable << " [--tables] <arquivo|->\n"
              << "  --tables  mostra tambem as tabelas de tokens secundarios\n"
              << "  -         le o codigo-fonte da entrada padrao\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    bool showTables = false;
    std::string path;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--tables") {
            showTables = true;
        } else if (argument == "--help" || argument == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (path.empty()) {
            path = argument;
        } else {
            printUsage(argv[0]);
            return 2;
        }
    }

    if (path.empty()) {
        printUsage(argv[0]);
        return 2;
    }

    try {
        Lexer lexer(readInput(path));
        const std::vector<Token> tokens = lexer.scanAll();
        printTokens(tokens);

        if (showTables) {
            printTables(lexer);
        }

        if (!lexer.errors().empty()) {
            std::cerr << "\nForam encontrados " << lexer.errors().size()
                      << " erro(s) lexico(s):\n";
            for (const LexicalError& error : lexer.errors()) {
                std::cerr << error.line << ':' << error.column
                          << ": erro lexico: " << error.message;
                if (!error.lexeme.empty()) {
                    std::cerr << " [lexema: "
                              << escapeForDisplay(error.lexeme) << ']';
                }
                std::cerr << '\n';
            }
            return 1;
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "erro: " << error.what() << '\n';
        return 2;
    }
}
