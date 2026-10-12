#include "lalr.hpp"

#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>

namespace {

typedef std::uint64_t TermSet;                         // um bit por terminal
typedef std::pair<int, std::size_t> Item;              // (regra, posição do ponto)
typedef std::map<Item, TermSet> ItemSet;               // item -> lookaheads
typedef std::vector<Item> Core;                        // itens sem lookahead

class Builder {
public:
    Builder(const std::vector<Rule>& rules, int numTerminals, int numSymbols)
        : rules_(rules), numTerminals_(numTerminals), numSymbols_(numSymbols),
          rulesOf_(numSymbols), nullable_(numSymbols, false), first_(numSymbols, 0) {
        for (std::size_t r = 0; r < rules_.size(); ++r) {
            rulesOf_[rules_[r].lhs].push_back(static_cast<int>(r));
        }
        computeFirst();
    }

    LalrTable build(int endOfFile) {
        std::vector<ItemSet> kernels;
        std::vector<std::map<int, int>> transitions;
        std::map<Core, int> stateOfCore;
        std::vector<int> pending;

        ItemSet start;
        start[Item(0, 0)] = TermSet(1) << endOfFile;
        kernels.push_back(start);
        transitions.emplace_back();
        stateOfCore[coreOf(start)] = 0;
        pending.push_back(0);

        // Estados com o mesmo núcleo são fundidos; quando os lookaheads de um
        // estado crescem, ele é reprocessado para propagá-los aos sucessores.
        while (!pending.empty()) {
            const int state = pending.back();
            pending.pop_back();

            std::map<int, ItemSet> moves;
            for (const auto& entry : closure(kernels[state])) {
                const Rule& rule = rules_[entry.first.first];
                const std::size_t dot = entry.first.second;
                if (dot < rule.rhs.size()) {
                    moves[rule.rhs[dot]][Item(entry.first.first, dot + 1)] |= entry.second;
                }
            }

            for (const auto& move : moves) {
                const Core core = coreOf(move.second);
                const auto found = stateOfCore.find(core);
                int target;
                if (found == stateOfCore.end()) {
                    target = static_cast<int>(kernels.size());
                    kernels.push_back(move.second);
                    transitions.emplace_back();
                    stateOfCore[core] = target;
                    pending.push_back(target);
                } else {
                    target = found->second;
                    bool grew = false;
                    for (const auto& entry : move.second) {
                        TermSet& lookahead = kernels[target][entry.first];
                        if ((lookahead | entry.second) != lookahead) {
                            lookahead |= entry.second;
                            grew = true;
                        }
                    }
                    if (grew) {
                        pending.push_back(target);
                    }
                }
                transitions[state][move.first] = target;
            }
        }

        LalrTable table;
        table.shiftReduceConflicts = 0;
        table.reduceReduceConflicts = 0;
        table.action.assign(kernels.size(),
                            std::vector<Action>(numSymbols_, Action{ActionKind::ERROR, 0}));

        for (std::size_t state = 0; state < kernels.size(); ++state) {
            std::vector<Action>& row = table.action[state];
            for (const auto& transition : transitions[state]) {
                row[transition.first] = Action{ActionKind::SHIFT, transition.second};
            }

            for (const auto& entry : closure(kernels[state])) {
                const int rule = entry.first.first;
                if (entry.first.second != rules_[rule].rhs.size()) {
                    continue;
                }
                const Action reduce = rule == 0 ? Action{ActionKind::ACCEPT, 0}
                                                : Action{ActionKind::REDUCE, rule};
                for (int terminal = 0; terminal < numTerminals_; ++terminal) {
                    if (((entry.second >> terminal) & 1) == 0) {
                        continue;
                    }
                    Action& current = row[terminal];
                    if (current.kind == ActionKind::ERROR) {
                        current = reduce;
                    } else if (current.kind == ActionKind::SHIFT) {
                        ++table.shiftReduceConflicts;
                    } else {
                        ++table.reduceReduceConflicts;
                        if (reduce.value < current.value) {
                            current = reduce;
                        }
                    }
                }
            }
        }
        return table;
    }

private:
    const std::vector<Rule>& rules_;
    const int numTerminals_;
    const int numSymbols_;
    std::vector<std::vector<int>> rulesOf_;
    std::vector<bool> nullable_;
    std::vector<TermSet> first_;

    void computeFirst() {
        for (int terminal = 0; terminal < numTerminals_; ++terminal) {
            first_[terminal] = TermSet(1) << terminal;
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (const Rule& rule : rules_) {
                TermSet first = first_[rule.lhs];
                bool allNullable = true;
                for (int symbol : rule.rhs) {
                    first |= first_[symbol];
                    if (!nullable_[symbol]) {
                        allNullable = false;
                        break;
                    }
                }
                if (first != first_[rule.lhs]) {
                    first_[rule.lhs] = first;
                    changed = true;
                }
                if (allNullable && !nullable_[rule.lhs]) {
                    nullable_[rule.lhs] = true;
                    changed = true;
                }
            }
        }
    }

    // FIRST(rhs[from..] lookahead)
    TermSet firstOf(const Rule& rule, std::size_t from, TermSet lookahead) const {
        TermSet result = 0;
        for (std::size_t i = from; i < rule.rhs.size(); ++i) {
            result |= first_[rule.rhs[i]];
            if (!nullable_[rule.rhs[i]]) {
                return result;
            }
        }
        return result | lookahead;
    }

    ItemSet closure(ItemSet items) const {
        bool changed = true;
        while (changed) {
            changed = false;
            for (const auto& entry : items) {
                const Rule& rule = rules_[entry.first.first];
                const std::size_t dot = entry.first.second;
                if (dot == rule.rhs.size() || rule.rhs[dot] < numTerminals_) {
                    continue;
                }
                const TermSet lookahead = firstOf(rule, dot + 1, entry.second);
                for (int produced : rulesOf_[rule.rhs[dot]]) {
                    TermSet& target = items[Item(produced, 0)];
                    if ((target | lookahead) != target) {
                        target |= lookahead;
                        changed = true;
                    }
                }
            }
        }
        return items;
    }

    static Core coreOf(const ItemSet& items) {
        Core core;
        for (const auto& entry : items) {
            core.push_back(entry.first);
        }
        return core;
    }
};

}  // namespace

LalrTable buildLalrTable(const std::vector<Rule>& rules, int numTerminals,
                         int numSymbols, int endOfFile) {
    if (numTerminals > 64) {
        throw std::logic_error("mais de 64 terminais nao cabem no conjunto de lookaheads");
    }
    return Builder(rules, numTerminals, numSymbols).build(endOfFile);
}
