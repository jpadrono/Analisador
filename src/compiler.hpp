#ifndef COMPILADOR_COMPILER_HPP
#define COMPILADOR_COMPILER_HPP

#include "lalr.hpp"
#include "lexer.hpp"

#include <string>
#include <vector>

// Análises sintática, de escopo e de tipos de um programa que o Lexer já leu
// sem erros léxicos. Devolve os erros no formato
// "linha:coluna: erro <fase>: mensagem"; vazio se o programa for válido.
std::vector<std::string> analyze(const std::vector<Token>& tokens, const Lexer& lexer);

// Tabela la-LR(1) da Simple Script Language, construída uma única vez.
const LalrTable& sslParseTable();

#endif
