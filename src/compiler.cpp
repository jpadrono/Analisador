#include "compiler.hpp"

#include <deque>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Símbolos: os terminais são os próprios TokenKind; os não terminais vêm depois.
// ---------------------------------------------------------------------------

const int NUM_TERMINALS = static_cast<int>(TokenKind::END_OF_FILE) + 1;

enum Nonterminal {
    START_ = NUM_TERMINALS, P_, LDE_, DE_, T_, DT_, DC_, DF_, LP_, B_, LDV_, LS_,
    DV_, LI_, S_, E_, L_, R_, Y_, F_, LE_, LV_, IDD_, IDU_, ID_,
    TRUE_, FALSE_, CHR_, STR_, NUM_,
    // Marcadores semânticos: não terminais com uma única regra vazia (seção 5.1).
    NB_, NF_, MF_, MT_, ME_, MW_, MD_, MC_,
    NUM_SYMBOLS
};

#define K(name) static_cast<int>(TokenKind::name)

// A ordem deste enum é a ordem de RULES.
enum RuleId {
    START_P, P_LDE, LDE_LDE_DE, LDE_DE, DE_DF, DE_DT,
    T_INTEGER, T_CHAR, T_BOOLEAN, T_STRING, T_IDU,
    DT_ARRAY, DT_STRUCT, DT_ALIAS,
    DC_LIST, DC_LI,
    DF_FUNCTION,
    LP_LIST, LP_IDD,
    B_BLOCK, LDV_LIST, LDV_DV, LS_LIST, LS_S,
    DV_VAR, LI_LIST, LI_IDD,
    S_IF, S_IF_ELSE, S_WHILE, S_DO_WHILE, S_BLOCK, S_ASSIGN,
    S_BREAK, S_CONTINUE, S_RETURN,
    E_AND, E_OR, E_L,
    L_LT, L_GT, L_LE, L_GE, L_EQ, L_NE, L_R,
    R_PLUS, R_MINUS, R_Y,
    Y_TIMES, Y_DIVIDE, Y_F,
    F_LV, F_PRE_INC, F_PRE_DEC, F_POS_INC, F_POS_DEC, F_PAREN, F_CALL,
    F_MINUS, F_NOT, F_TRUE, F_FALSE, F_CHR, F_STR, F_NUM,
    LE_LIST, LE_E,
    LV_FIELD, LV_INDEX, LV_IDU,
    IDD_ID, IDU_ID, ID_ID,
    TRUE_TRUE, FALSE_FALSE, CHR_C, STR_S, NUM_N,
    NB_EMPTY, NF_EMPTY, MF_EMPTY, MT_EMPTY, ME_EMPTY, MW_EMPTY, MD_EMPTY, MC_EMPTY,
    NUM_RULES
};

// Gramática de Gramática.pdf com os marcadores dos capítulos 5 a 7, ID no
// acesso a campo (cap. 5) e o comando return (exemplos do cap. 7).
const std::vector<Rule> RULES = {
    {START_, {P_}},
    {P_, {LDE_}},
    {LDE_, {LDE_, DE_}},
    {LDE_, {DE_}},
    {DE_, {DF_}},
    {DE_, {DT_}},
    {T_, {K(INTEGER)}},
    {T_, {K(CHAR)}},
    {T_, {K(BOOLEAN)}},
    {T_, {K(STRING)}},
    {T_, {IDU_}},
    {DT_, {K(TYPE), IDD_, K(ASSIGN), K(ARRAY), K(LEFT_SQUARE), NUM_, K(RIGHT_SQUARE), K(OF), T_}},
    {DT_, {K(TYPE), IDD_, K(ASSIGN), K(STRUCT), NB_, K(LEFT_BRACE), DC_, K(RIGHT_BRACE)}},
    {DT_, {K(TYPE), IDD_, K(ASSIGN), T_}},
    {DC_, {DC_, K(SEMICOLON), LI_, K(COLON), T_}},
    {DC_, {LI_, K(COLON), T_}},
    {DF_, {K(FUNCTION), IDD_, NF_, K(LEFT_PARENTHESIS), LP_, K(RIGHT_PARENTHESIS), K(COLON), T_, MF_, B_}},
    {LP_, {LP_, K(COMMA), IDD_, K(COLON), T_}},
    {LP_, {IDD_, K(COLON), T_}},
    {B_, {K(LEFT_BRACE), LDV_, LS_, K(RIGHT_BRACE)}},
    {LDV_, {LDV_, DV_}},
    {LDV_, {DV_}},
    {LS_, {LS_, S_}},
    {LS_, {S_}},
    {DV_, {K(VAR), LI_, K(COLON), T_, K(SEMICOLON)}},
    {LI_, {LI_, K(COMMA), IDD_}},
    {LI_, {IDD_}},
    {S_, {K(IF), K(LEFT_PARENTHESIS), E_, K(RIGHT_PARENTHESIS), MT_, S_}},
    {S_, {K(IF), K(LEFT_PARENTHESIS), E_, K(RIGHT_PARENTHESIS), MT_, S_, K(ELSE), ME_, S_}},
    {S_, {K(WHILE), MW_, K(LEFT_PARENTHESIS), E_, K(RIGHT_PARENTHESIS), MT_, S_}},
    {S_, {K(DO), MW_, S_, K(WHILE), MD_, K(LEFT_PARENTHESIS), E_, K(RIGHT_PARENTHESIS), K(SEMICOLON)}},
    {S_, {NB_, B_}},
    {S_, {LV_, K(ASSIGN), E_, K(SEMICOLON)}},
    {S_, {K(BREAK), K(SEMICOLON)}},
    {S_, {K(CONTINUE), K(SEMICOLON)}},
    {S_, {K(RETURN), E_, K(SEMICOLON)}},
    {E_, {E_, K(AND), L_}},
    {E_, {E_, K(OR), L_}},
    {E_, {L_}},
    {L_, {L_, K(LESS_THAN), R_}},
    {L_, {L_, K(GREATER_THAN), R_}},
    {L_, {L_, K(LESS_OR_EQUAL), R_}},
    {L_, {L_, K(GREATER_OR_EQUAL), R_}},
    {L_, {L_, K(EQUAL_EQUAL), R_}},
    {L_, {L_, K(NOT_EQUAL), R_}},
    {L_, {R_}},
    {R_, {R_, K(PLUS), Y_}},
    {R_, {R_, K(MINUS), Y_}},
    {R_, {Y_}},
    {Y_, {Y_, K(TIMES), F_}},
    {Y_, {Y_, K(DIVIDE), F_}},
    {Y_, {F_}},
    {F_, {LV_}},
    {F_, {K(PLUS_PLUS), LV_}},
    {F_, {K(MINUS_MINUS), LV_}},
    {F_, {LV_, K(PLUS_PLUS)}},
    {F_, {LV_, K(MINUS_MINUS)}},
    {F_, {K(LEFT_PARENTHESIS), E_, K(RIGHT_PARENTHESIS)}},
    {F_, {IDU_, MC_, K(LEFT_PARENTHESIS), LE_, K(RIGHT_PARENTHESIS)}},
    {F_, {K(MINUS), F_}},
    {F_, {K(NOT), F_}},
    {F_, {TRUE_}},
    {F_, {FALSE_}},
    {F_, {CHR_}},
    {F_, {STR_}},
    {F_, {NUM_}},
    {LE_, {LE_, K(COMMA), E_}},
    {LE_, {E_}},
    {LV_, {LV_, K(DOT), ID_}},
    {LV_, {LV_, K(LEFT_SQUARE), E_, K(RIGHT_SQUARE)}},
    {LV_, {IDU_}},
    {IDD_, {K(IDENTIFIER)}},
    {IDU_, {K(IDENTIFIER)}},
    {ID_, {K(IDENTIFIER)}},
    {TRUE_, {K(TRUE_LITERAL)}},
    {FALSE_, {K(FALSE_LITERAL)}},
    {CHR_, {K(CHARACTER)}},
    {STR_, {K(STRING_VALUE)}},
    {NUM_, {K(NUMERAL)}},
    {NB_, {}},
    {NF_, {}},
    {MF_, {}},
    {MT_, {}},
    {ME_, {}},
    {MW_, {}},
    {MD_, {}},
    {MC_, {}},
};

#undef K

// ---------------------------------------------------------------------------
// Tabela de símbolos (capítulos 5, 6 e 7)
// ---------------------------------------------------------------------------

enum Kind {
    NO_KIND_DEF_, VAR_, PARAM_, FUNCTION_, FIELD_,
    ARRAY_TYPE_, STRUCT_TYPE_, ALIAS_TYPE_, SCALAR_TYPE_, UNIVERSAL_
};

// Figuras 5.3, 6.2 e 7.11 achatadas em uma struct. As listas encadeadas por
// pNext viraram vetores: no livro, a lista de parâmetros continua nas
// variáveis locais, que são declaradas no mesmo nível.
struct Object {
    int name = -1;               // token secundário do identificador
    Kind kind = NO_KIND_DEF_;
    Object* type = nullptr;      // var/param/campo: tipo; função: retorno; array: elemento; alias: base
    int index = 0;               // var/param: endereço (bp + index); campo: deslocamento; função: nº no pool
    int size = 0;                // em valores escalares: tamanho do tipo ou da variável
    int numElems = 0;            // array
    std::vector<Object*> list;   // função: parâmetros; struct: campos
    int nParams = 0;             // função: tamanho dos parâmetros
    int nVars = 0;               // função: tamanho dos parâmetros + variáveis (figura 7.18)
};

// Nó da pilha semântica. Diferente do livro, os terminais também são
// empilhados: o atributo do i-ésimo símbolo do lado direito fica sempre em
// A(i) e os tokens secundários não dependem de uma variável global.
struct Attr {
    int symbol = -1;
    std::size_t line = 0;
    std::size_t column = 0;
    int name = -1;               // token secundário (Id e literais)
    Object* obj = nullptr;       // IDD, IDU; MC e LE: função chamada
    Object* type = nullptr;      // T, E, L, R, Y, F, LV, MC
    int size = 0;                // T, DC, LP
    int label = -1;              // MT, ME
    int count = 0;               // LE: argumentos verificados; MF: início do código da função
    bool err = false;            // MC, LE: chamada inválida, argumentos não são verificados
    bool strElem = false;        // LV: caractere de string (s[i])
    std::vector<Object*> list;   // LI, LP, DC
};

struct Loop {
    int start;   // rótulo do início do laço
    int exit;    // destino do break
    int cont;    // destino do continue
    bool isDo;
};

// Instruções CALL_LIB da biblioteca 0 (strings), recomendadas pelo livro
// para acessar caracteres quando a linguagem tem tipos estruturados.
const char* const LIB_CHAR_AT = "CALL_LIB 0, 0";   // [ref, i]    -> [s[i]]
const char* const LIB_SET_CHAR = "CALL_LIB 0, 1";  // [ref, i, c] -> [c], com s[i] = c

class Compiler {
public:
    explicit Compiler(const Lexer& lexer) : lexer_(lexer) {
        for (Object* scalar : {&int_, &char_, &bool_, &string_}) {
            scalar->kind = SCALAR_TYPE_;
            scalar->size = 1;
        }
        universal_.kind = UNIVERSAL_;
    }

    CompileResult run(const std::vector<Token>& tokens);

private:
    const Lexer& lexer_;
    Object int_, char_, bool_, string_, universal_;  // figura 6.3

    std::deque<Object> objects_;                     // endereços estáveis
    std::vector<std::vector<Object*>> levels_;       // pilha de blocos (figura 5.1)
    std::vector<Attr> sem_;                          // pilha semântica
    std::size_t base_ = 0;                           // A(0) da regra em redução
    std::vector<Loop> loops_;
    Object* function_ = nullptr;                     // curFunction
    int numFunctions_ = 0;
    int numLabels_ = 0;
    std::string code_;
    std::vector<std::string> errors_;

    Attr& A(std::size_t i) { return sem_[base_ + i]; }
    // Símbolo à esquerda da regra em redução: left(0) é o imediatamente anterior.
    Attr& left(std::size_t k) { return sem_[base_ - 1 - k]; }

    void semantics(int rule, Attr& lhs);
    void syntaxError(const Token& token, const std::vector<Action>& row);

    // Figuras 5.4 a 5.7
    void newBlock() { levels_.emplace_back(); }
    void endBlock() { levels_.pop_back(); }
    Object* define(int name) {
        objects_.emplace_back();
        objects_.back().name = name;
        levels_.back().push_back(&objects_.back());
        return &objects_.back();
    }
    Object* search(int name) const {
        for (Object* obj : levels_.back()) {
            if (obj->name == name) return obj;
        }
        return nullptr;
    }
    Object* find(int name) const {
        for (auto level = levels_.rbegin(); level != levels_.rend(); ++level) {
            for (Object* obj : *level) {
                if (obj->name == name) return obj;
            }
        }
        return nullptr;
    }

    // Figura 6.16: compatibilidade por nome; aliases já chegam resolvidos.
    bool checkTypes(const Object* t1, const Object* t2) const {
        return t1 == t2 || t1 == nullptr || t2 == nullptr ||
               t1 == &universal_ || t2 == &universal_;
    }

    void declare(Attr& lhs, const std::vector<Object*>& ids, const Attr& t,
                 Kind kind, int firstIndex);
    void checkArgument(Attr& le, const Attr& e);

    void error(std::size_t line, std::size_t column, const std::string& phase,
               const std::string& message) {
        errors_.push_back(std::to_string(line) + ":" + std::to_string(column) +
                          ": erro " + phase + ": " + message);
    }
    void error(const Attr& at, const std::string& phase, const std::string& message) {
        error(at.line, at.column, phase, message);
    }
    void expect(const Attr& at, const Object* type, const Object* expected,
                const std::string& message) {
        if (!checkTypes(type, expected)) error(at, "de tipo", message);
    }

    std::string idName(int name) const { return "'" + lexer_.identifiers()[name] + "'"; }
    std::string typeName(const Object* t) const {
        if (t == &int_) return "integer";
        if (t == &char_) return "char";
        if (t == &bool_) return "boolean";
        if (t == &string_) return "string";
        if (t == nullptr || t == &universal_) return "<erro>";
        return lexer_.identifiers()[t->name];
    }
    std::string mismatch(const Object* t1, const Object* t2) const {
        return "tipos incompativeis (" + typeName(t1) + " e " + typeName(t2) +
               ") [ERR_TYPE_MISMATCH]";
    }

    int newLabel() { return numLabels_++; }
    void emit(const std::string& instruction) { code_ += "\t" + instruction + "\n"; }
    void emit(const std::string& instruction, int operand) {
        emit(instruction + " " + std::to_string(operand));
    }
    void placeLabel(int label) { code_ += "L" + std::to_string(label) + ":\n"; }
};

CompileResult Compiler::run(const std::vector<Token>& tokens) {
    const LalrTable& table = sslParseTable();
    std::vector<int> states(1, 0);
    sem_.assign(1, Attr());
    newBlock();  // nível 0: tipos e funções

    // Algoritmo da figura 4.6
    std::size_t position = 0;
    while (true) {
        const Token& token = tokens[position];
        const Action action = table.action[states.back()][static_cast<int>(token.kind)];

        if (action.kind == ActionKind::SHIFT) {
            Attr attr;
            attr.symbol = static_cast<int>(token.kind);
            attr.line = token.line;
            attr.column = token.column;
            attr.name = token.hasSecondary ? static_cast<int>(token.secondary) : -1;
            states.push_back(action.value);
            sem_.push_back(attr);
            ++position;
        } else if (action.kind == ActionKind::REDUCE) {
            const Rule& rule = RULES[action.value];
            base_ = sem_.size() - rule.rhs.size();

            Attr lhs;
            lhs.symbol = rule.lhs;
            lhs.line = rule.rhs.empty() ? token.line : A(0).line;
            lhs.column = rule.rhs.empty() ? token.column : A(0).column;
            semantics(action.value, lhs);

            sem_.resize(base_);
            states.resize(base_);
            states.push_back(table.action[states.back()][rule.lhs].value);
            sem_.push_back(lhs);
        } else if (action.kind == ActionKind::ACCEPT) {
            break;
        } else {
            syntaxError(token, table.action[states.back()]);
            break;
        }
    }

    CompileResult result;
    result.errors = errors_;
    if (errors_.empty()) {  // ERROR_FLAG do livro: só há código se não houve erro
        result.code = code_;
    }
    return result;
}

void Compiler::syntaxError(const Token& token, const std::vector<Action>& row) {
    std::string expected;
    for (int terminal = 0; terminal < NUM_TERMINALS; ++terminal) {
        if (row[terminal].kind != ActionKind::ERROR) {
            expected += (expected.empty() ? "" : " ") +
                        tokenKindName(static_cast<TokenKind>(terminal));
        }
    }
    const std::string found = token.kind == TokenKind::END_OF_FILE
                                  ? "fim do arquivo"
                                  : "'" + escapeForDisplay(token.lexeme) + "' (" +
                                        tokenKindName(token.kind) + ")";
    error(token.line, token.column, "sintatico",
          "token inesperado " + found + "; esperado: " + expected);
}

// Variáveis, parâmetros e campos: kind, tipo, endereço e tamanho (figuras 7.15, 7.17 e 7.19).
void Compiler::declare(Attr& lhs, const std::vector<Object*>& ids, const Attr& t,
                       Kind kind, int firstIndex) {
    int index = firstIndex;
    for (Object* p : ids) {
        p->kind = kind;
        p->type = t.type;
        p->index = index;
        p->size = t.size;
        index += t.size;
        lhs.list.push_back(p);
    }
    lhs.size = index;
}

// Um argumento de chamada (figura 6.23): LE.count argumentos já foram verificados.
void Compiler::checkArgument(Attr& le, const Attr& e) {
    if (le.err) return;
    const std::vector<Object*>& params = le.obj->list;
    if (static_cast<std::size_t>(le.count) >= params.size()) {
        error(e, "de tipo", "argumentos demais na chamada de " + idName(le.obj->name) +
                                " [ERR_TOO_MANY_ARGS]");
        le.err = true;
        return;
    }
    if (!checkTypes(params[le.count]->type, e.type)) {
        error(e, "de tipo", "argumento " + std::to_string(le.count + 1) + " de " +
                                idName(le.obj->name) + " deveria ser " +
                                typeName(params[le.count]->type) + ", mas e " +
                                typeName(e.type) + " [ERR_PARAM_TYPE]");
    }
    ++le.count;
}

void Compiler::semantics(int rule, Attr& lhs) {
    switch (rule) {
        // ----- Tipos (figuras 6.5 e 7.12) -----
        case T_INTEGER: lhs.type = &int_; lhs.size = 1; break;
        case T_CHAR: lhs.type = &char_; lhs.size = 1; break;
        case T_BOOLEAN: lhs.type = &bool_; lhs.size = 1; break;
        case T_STRING: lhs.type = &string_; lhs.size = 1; break;
        case T_IDU: {
            Object* p = A(0).obj;
            if (p->kind == ALIAS_TYPE_) {  // sinônimo: usa o tipo base
                lhs.type = p->type;
                lhs.size = p->size;
            } else if (p->kind == ARRAY_TYPE_ || p->kind == STRUCT_TYPE_) {
                lhs.type = p;
                lhs.size = p->size;
            } else {
                if (p->kind != UNIVERSAL_) {
                    error(A(0), "de tipo", idName(p->name) + " nao e um tipo [ERR_TYPE_EXPECTED]");
                }
                lhs.type = &universal_;
            }
            break;
        }

        // ----- Declarações de tipo (figuras 7.13, 7.14 e 7.16) -----
        case DT_ARRAY: {  // type IDD = array [ NUM ] of T
            Object* p = A(1).obj;
            const std::string& digits = lexer_.constants()[A(5).name].value;
            const int n = digits.size() > 6 ? 0 : std::stoi(digits);
            if (n == 0) {
                error(A(5), "de tipo", "tamanho de array invalido: " + digits);
            }
            p->kind = ARRAY_TYPE_;
            p->numElems = n;
            p->type = A(8).type;
            p->size = n * A(8).size;
            break;
        }
        case DT_STRUCT: {  // type IDD = struct NB { DC }
            Object* p = A(1).obj;
            p->kind = STRUCT_TYPE_;
            p->list = A(6).list;
            p->size = A(6).size;
            endBlock();
            break;
        }
        case DT_ALIAS: {  // type IDD = T
            Object* p = A(1).obj;
            p->kind = ALIAS_TYPE_;
            p->type = A(3).type;
            p->size = A(3).size;
            break;
        }
        case DC_LI: declare(lhs, A(0).list, A(2), FIELD_, 0); break;
        case DC_LIST:
            lhs.list = A(0).list;
            declare(lhs, A(2).list, A(4), FIELD_, A(0).size);
            break;

        // ----- Funções (figuras 6.13, 7.17 e 7.18) -----
        case NF_EMPTY: {  // function IDD . NF
            Object* f = left(0).obj;
            f->kind = FUNCTION_;
            f->index = numFunctions_++;
            function_ = f;
            newBlock();  // parâmetros e variáveis locais
            break;
        }
        case LP_IDD: declare(lhs, {A(0).obj}, A(2), PARAM_, 0); break;
        case LP_LIST:
            lhs.list = A(0).list;
            declare(lhs, {A(2).obj}, A(4), PARAM_, A(0).size);
            break;
        case MF_EMPTY: {  // function IDD NF ( LP ) : T . MF
            function_->type = left(0).type;
            function_->list = left(3).list;
            function_->nParams = left(3).size;
            function_->nVars = left(3).size;
            lhs.count = static_cast<int>(code_.size());
            break;
        }
        case DF_FUNCTION: {  // function IDD NF ( LP ) : T MF B
            const Object* f = A(1).obj;
            code_.insert(static_cast<std::size_t>(A(8).count),
                         "BEGIN_FUNC " + std::to_string(f->index) + ", " +
                             std::to_string(f->nParams) + ", " +
                             std::to_string(f->nVars) + "\n");
            code_ += "END_FUNC\n";
            endBlock();
            function_ = nullptr;
            break;
        }

        // ----- Variáveis e listas de identificadores (figura 7.19) -----
        case DV_VAR:  // var LI : T ;
            declare(lhs, A(1).list, A(3), VAR_, function_->nVars);
            function_->nVars = lhs.size;
            break;
        case LI_IDD: lhs.list.push_back(A(0).obj); break;
        case LI_LIST:
            lhs.list = A(0).list;
            lhs.list.push_back(A(2).obj);
            break;

        // ----- Identificadores (figuras 5.8 e 6.4) -----
        case IDD_ID:
            lhs.name = A(0).name;
            if (search(lhs.name) != nullptr) {
                error(A(0), "de escopo", "identificador " + idName(lhs.name) +
                                             " ja declarado neste escopo [ERR_REDECL]");
                objects_.emplace_back();  // objeto fora da tabela, só para seguir a análise
                lhs.obj = &objects_.back();
                lhs.obj->name = lhs.name;
            } else {
                lhs.obj = define(lhs.name);
            }
            break;
        case IDU_ID:
            lhs.name = A(0).name;
            lhs.obj = find(lhs.name);
            if (lhs.obj == nullptr) {
                error(A(0), "de escopo", "identificador " + idName(lhs.name) +
                                             " nao declarado [ERR_NOT_DECL]");
                lhs.obj = define(lhs.name);
                lhs.obj->kind = UNIVERSAL_;  // evita erros em cascata
            }
            break;
        case ID_ID: lhs.name = A(0).name; break;

        // ----- Blocos -----
        case NB_EMPTY: newBlock(); break;
        case S_BLOCK: endBlock(); break;  // NB B

        // ----- Seleção (figura 7.21) -----
        case MT_EMPTY:  // ( E ) . MT -- no while, desvia para a saída do laço
            lhs.label = left(3).symbol == MW_ ? loops_.back().exit : newLabel();
            emit("TJMP_FW L" + std::to_string(lhs.label));
            break;
        case ME_EMPTY:  // MT S else . ME
            lhs.label = newLabel();
            emit("JMP_FW L" + std::to_string(lhs.label));
            placeLabel(left(2).label);
            break;
        case S_IF:  // if ( E ) MT S
            expect(A(2), A(2).type, &bool_, "condicao deve ser boolean [ERR_BOOL_TYPE_EXPECTED]");
            placeLabel(A(4).label);
            break;
        case S_IF_ELSE:  // if ( E ) MT S else ME S
            expect(A(2), A(2).type, &bool_, "condicao deve ser boolean [ERR_BOOL_TYPE_EXPECTED]");
            placeLabel(A(7).label);
            break;

        // ----- Repetição (figura 7.22) -----
        case MW_EMPTY: {  // while . MW  |  do . MW
            Loop loop;
            loop.isDo = left(0).symbol == static_cast<int>(TokenKind::DO);
            loop.start = newLabel();
            loop.exit = newLabel();
            loop.cont = loop.isDo ? newLabel() : loop.start;
            placeLabel(loop.start);
            loops_.push_back(loop);
            break;
        }
        case MD_EMPTY: placeLabel(loops_.back().cont); break;  // do MW S while . MD
        case S_WHILE:  // while MW ( E ) MT S
            expect(A(3), A(3).type, &bool_, "condicao deve ser boolean [ERR_BOOL_TYPE_EXPECTED]");
            emit("JMP_BW L" + std::to_string(loops_.back().start));
            placeLabel(loops_.back().exit);
            loops_.pop_back();
            break;
        case S_DO_WHILE:  // do MW S while MD ( E ) ;
            expect(A(6), A(6).type, &bool_, "condicao deve ser boolean [ERR_BOOL_TYPE_EXPECTED]");
            emit("NOT");
            emit("TJMP_BW L" + std::to_string(loops_.back().start));
            placeLabel(loops_.back().exit);
            loops_.pop_back();
            break;
        case S_BREAK:
        case S_CONTINUE:
            if (loops_.empty()) {
                error(A(0), "semantico", std::string(rule == S_BREAK ? "break" : "continue") +
                                             " fora de comando de repeticao");
            } else if (rule == S_BREAK) {
                emit("JMP_FW L" + std::to_string(loops_.back().exit));
            } else {
                emit(std::string(loops_.back().isDo ? "JMP_FW" : "JMP_BW") + " L" +
                     std::to_string(loops_.back().cont));
            }
            break;

        // ----- Atribuição e retorno -----
        case S_ASSIGN:  // LV = E ;
            if (!checkTypes(A(0).type, A(2).type)) {
                error(A(2), "de tipo", mismatch(A(0).type, A(2).type));
            }
            if (A(0).strElem) {
                emit(LIB_SET_CHAR);
                emit("POP");
            } else {
                emit("STORE_REF", A(0).type ? A(0).type->size : 0);
            }
            break;
        case S_RETURN:  // return E ;
            if (!checkTypes(function_->type, A(1).type)) {
                error(A(1), "de tipo", "retorno de " + idName(function_->name) + ": " +
                                           mismatch(function_->type, A(1).type));
            }
            emit("RET");
            break;

        // ----- Expressões lógicas e relacionais (figuras 7.23 e 7.24) -----
        case E_AND:
        case E_OR:
            expect(A(0), A(0).type, &bool_, "operando de " + std::string(rule == E_AND ? "&&" : "||") +
                                                " deve ser boolean [ERR_BOOL_TYPE_EXPECTED]");
            expect(A(2), A(2).type, &bool_, "operando de " + std::string(rule == E_AND ? "&&" : "||") +
                                                " deve ser boolean [ERR_BOOL_TYPE_EXPECTED]");
            lhs.type = &bool_;
            emit(rule == E_AND ? "AND" : "OR");
            break;
        case L_LT: case L_GT: case L_LE: case L_GE: case L_EQ: case L_NE: {
            static const char* const OPS[] = {"LT", "GT", "LE", "GE", "EQ", "NEQ"};
            if (!checkTypes(A(0).type, A(2).type)) {
                error(A(2), "de tipo", mismatch(A(0).type, A(2).type));
            }
            lhs.type = &bool_;
            emit(OPS[rule - L_LT]);
            break;
        }

        // ----- Expressões aritméticas (figura 7.25) -----
        case R_PLUS:
        case R_MINUS:
        case Y_TIMES:
        case Y_DIVIDE: {
            static const char* const OPS[] = {"ADD", "SUB", "MUL", "DIV"};
            static const char* const SYMBOLS[] = {"+", "-", "*", "/"};
            const int op = rule == R_PLUS ? 0 : rule == R_MINUS ? 1 : rule == Y_TIMES ? 2 : 3;
            if (!checkTypes(A(0).type, A(2).type)) {
                error(A(2), "de tipo", mismatch(A(0).type, A(2).type));
            }
            // '+' também concatena strings
            if (!checkTypes(A(0).type, &int_) && !(op == 0 && checkTypes(A(0).type, &string_))) {
                error(A(0), "de tipo", std::string("operador '") + SYMBOLS[op] +
                                           "' nao se aplica a " + typeName(A(0).type) +
                                           " [ERR_INVALID_TYPE]");
            }
            lhs.type = A(0).type;
            emit(OPS[op]);
            break;
        }
        case E_L: case L_R: case R_Y: case Y_F: case F_PAREN:
            lhs.type = rule == F_PAREN ? A(1).type : A(0).type;
            break;

        // ----- Fatores (figura 7.26) -----
        case F_LV:
            lhs.type = A(0).type;
            if (A(0).strElem) {
                emit(LIB_CHAR_AT);
            } else {
                emit("DE_REF", A(0).type ? A(0).type->size : 0);
            }
            break;
        case F_PRE_INC:
        case F_PRE_DEC:
        case F_POS_INC:
        case F_POS_DEC: {
            const bool pre = rule == F_PRE_INC || rule == F_PRE_DEC;
            const bool inc = rule == F_PRE_INC || rule == F_POS_INC;
            const Attr& lv = pre ? A(1) : A(0);
            expect(lv, lv.type, &int_, std::string(inc ? "++" : "--") +
                                           " exige variavel integer [ERR_INVALID_TYPE]");
            lhs.type = &int_;
            emit("DUP");
            emit("DUP");
            emit("DE_REF", 1);
            emit(inc ? "INC" : "DEC");
            emit("STORE_REF", 1);
            emit("DE_REF", 1);
            if (!pre) emit(inc ? "DEC" : "INC");  // valor anterior à operação
            break;
        }
        case F_MINUS:
            expect(A(1), A(1).type, &int_, "'-' unario exige integer [ERR_INVALID_TYPE]");
            lhs.type = &int_;
            emit("NEG");
            break;
        case F_NOT:
            expect(A(1), A(1).type, &bool_, "'!' exige boolean [ERR_INVALID_TYPE]");
            lhs.type = &bool_;
            emit("NOT");
            break;
        case F_TRUE: lhs.type = &bool_; emit("LOAD_TRUE"); break;    // figura 7.7
        case F_FALSE: lhs.type = &bool_; emit("LOAD_FALSE"); break;
        case F_CHR: lhs.type = &char_; emit("LOAD_CONST", A(0).name); break;
        case F_STR: lhs.type = &string_; emit("LOAD_CONST", A(0).name); break;
        case F_NUM: lhs.type = &int_; emit("LOAD_CONST", A(0).name); break;
        case CHR_C: case STR_S: case NUM_N: lhs.name = A(0).name; break;

        // ----- Chamada de função (figuras 6.23 e 7.28) -----
        case MC_EMPTY: {  // IDU . MC
            Object* f = left(0).obj;
            lhs.obj = f;
            if (f->kind == FUNCTION_) {
                lhs.type = f->type;
            } else {
                if (f->kind != UNIVERSAL_) {
                    error(left(0), "de tipo", idName(f->name) + " nao e funcao [ERR_KIND_NOT_FUNC]");
                }
                lhs.type = &universal_;
                lhs.err = true;
            }
            break;
        }
        case LE_E: {  // IDU MC ( . E
            const Attr& mc = left(1);
            lhs.obj = mc.obj;
            lhs.err = mc.err;
            checkArgument(lhs, A(0));
            break;
        }
        case LE_LIST:  // LE , E
            lhs.obj = A(0).obj;
            lhs.err = A(0).err;
            lhs.count = A(0).count;
            checkArgument(lhs, A(2));
            break;
        case F_CALL: {  // IDU MC ( LE )
            lhs.type = A(1).type;
            if (!A(3).err && static_cast<std::size_t>(A(3).count) < A(3).obj->list.size()) {
                error(A(4), "de tipo", "argumentos de menos na chamada de " +
                                           idName(A(1).obj->name) + " [ERR_TOO_FEW_ARGS]");
            }
            emit("CALL", A(1).obj->index);
            break;
        }

        // ----- Left-values (figura 7.27) -----
        case LV_IDU: {
            const Object* p = A(0).obj;
            if (p->kind == VAR_ || p->kind == PARAM_) {
                lhs.type = p->type;
                emit("LOAD_REF", p->index);
            } else {
                if (p->kind != UNIVERSAL_) {
                    error(A(0), "de tipo", idName(p->name) + " nao e variavel [ERR_KIND_NOT_VAR]");
                }
                lhs.type = &universal_;
            }
            break;
        }
        case LV_FIELD: {  // LV . ID
            const Object* t = A(0).type;
            lhs.type = &universal_;
            if (t == nullptr || t->kind != STRUCT_TYPE_) {
                if (t != &universal_) {
                    error(A(1), "de tipo", "'.' aplicado a " + typeName(t) +
                                               ", que nao e struct [ERR_KIND_NOT_STRUCT]");
                }
                break;
            }
            for (const Object* field : t->list) {
                if (field->name == A(2).name) {
                    lhs.type = field->type;
                    emit("ADD", field->index);
                    break;
                }
            }
            if (lhs.type == &universal_) {
                error(A(2), "de tipo", "campo " + idName(A(2).name) + " nao existe em " +
                                           typeName(t) + " [ERR_FIELD_NOT_DECL]");
            }
            break;
        }
        case LV_INDEX: {  // LV [ E ]
            const Object* t = A(0).type;
            expect(A(2), A(2).type, &int_, "indice deve ser integer [ERR_INVALID_INDEX_TYPE]");
            if (t == &string_) {  // caractere: a referência e o índice ficam na pilha
                lhs.type = &char_;
                lhs.strElem = true;
            } else if (t != nullptr && t->kind == ARRAY_TYPE_) {
                lhs.type = t->type;
                emit("MUL", t->type ? t->type->size : 0);
                emit("ADD");
            } else {
                if (t != &universal_) {
                    error(A(1), "de tipo", "'[]' aplicado a " + typeName(t) +
                                               ", que nao e array nem string [ERR_KIND_NOT_ARRAY]");
                }
                lhs.type = &universal_;
            }
            break;
        }

        default:  // regras sem ação: P, LDE, DE, B, LDV, LS, TRUE, FALSE e a aumentada
            break;
    }
}

}  // namespace

const LalrTable& sslParseTable() {
    static const LalrTable table = [] {
        if (RULES.size() != NUM_RULES) {
            throw std::logic_error("RULES e RuleId fora de sincronia");
        }
        return buildLalrTable(RULES, NUM_TERMINALS, NUM_SYMBOLS,
                              static_cast<int>(TokenKind::END_OF_FILE));
    }();
    return table;
}

CompileResult compile(const std::vector<Token>& tokens, const Lexer& lexer) {
    return Compiler(lexer).run(tokens);
}
