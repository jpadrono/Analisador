#ifndef COMPILADOR_LALR_HPP
#define COMPILADOR_LALR_HPP

#include <vector>

// Regra de produção: lhs -> rhs. Símbolos são inteiros; terminais vêm primeiro.
struct Rule {
    int lhs;
    std::vector<int> rhs;
};

enum class ActionKind { ERROR, SHIFT, REDUCE, ACCEPT };

struct Action {
    ActionKind kind;
    int value;  // SHIFT: estado de destino (também é o goto dos não terminais); REDUCE: regra
};

struct LalrTable {
    // Action[estado][símbolo], indexada por terminais e não terminais, como na figura 4.6.
    std::vector<std::vector<Action>> action;
    int shiftReduceConflicts;   // resolvidos a favor do empilhamento (else pendente)
    int reduceReduceConflicts;  // resolvidos a favor da regra de menor número
};

// Constrói a tabela la-LR(1) por fusão dos estados LR(1) de mesmo núcleo (seção 3.8).
// A regra 0 deve ser a regra aumentada S' -> S, aceita com o lookahead endOfFile.
// Terminais são os símbolos 0..numTerminals-1 (no máximo 64).
LalrTable buildLalrTable(const std::vector<Rule>& rules, int numTerminals,
                         int numSymbols, int endOfFile);

#endif
