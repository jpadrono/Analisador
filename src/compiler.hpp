#ifndef COMPILADOR_COMPILER_HPP
#define COMPILADOR_COMPILER_HPP

#include "lalr.hpp"
#include "lexer.hpp"

#include <string>
#include <vector>

struct CompileResult {
    std::string code;                 // assembly da máquina virtual; vazio se houver erro
    std::vector<std::string> errors;  // "linha:coluna: erro <fase>: mensagem"
};

// Análises sintática, de escopo e de tipos e geração de código de um
// programa que o Lexer já leu sem erros léxicos.
CompileResult compile(const std::vector<Token>& tokens, const Lexer& lexer);

// Tabela la-LR(1) da Simple Script Language, construída uma única vez.
const LalrTable& sslParseTable();

#endif
