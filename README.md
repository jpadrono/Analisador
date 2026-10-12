# Compilador da Simple Script Language: VE

Continuação da VC: a versão entregue na VC (só o analisador léxico) está na tag `VC` deste repositório. Esta entrega (VE) acrescenta as fases de análise do compilador da SSL em C++11: **análise sintática, análise de escopo e análise de tipos**. A geração de código fica para a VF.

Fontes: a gramática fornecida pelo professor (`Gramática.pdf`) e os capítulos 3 a 6 do livro *Compiladores* v1.2.3 (Prof. Luiz Guedes).

## 1. Entendimento

O analisador faz uma única passada dirigida pela sintaxe, como no livro:

```
fonte ─► Lexer (VC) ─► tokens ─► parser la-LR(1) ──redução da regra r──► Semantics(r)
                                   (fig. 4.6)                             ├─ escopo (cap. 5)
                                                                          └─ tipos  (cap. 6)
```

- **Sintático (cap. 3.8 e 4.1):** autômato de pilha ascendente la-LR(1) dirigido pela tabela `Action[estado][símbolo]`. A cada redução, chama `Semantics(regra)`, que lê os atributos do lado direito na pilha semântica e empilha o atributo do lado esquerdo.
- **Escopo (cap. 5):** tabela de símbolos como pilha de blocos. Os não terminais `IDD` e `IDU` distinguem declaração e uso.
- **Tipos (cap. 6):** cada identificador tem um *kind* e um objeto de tipo. As expressões sintetizam `type`, e cada regra verifica os seus operandos.

| Arquivo | Conteúdo |
|---|---|
| `src/lexer.*` | lexer da VC, mais a palavra reservada `return` |
| `src/lalr.*` | construção da tabela la-LR(1) a partir de qualquer gramática |
| `src/compiler.*` | gramática da SSL, driver da fig. 4.6 e `Semantics()` com escopo e tipos |
| `src/main.cpp` | linha de comando |
| `tests/lexer_tests.cpp` | testes da VC |
| `tests/compiler_tests.cpp` | tabela, programas válidos, um caso para cada erro e regras de escopo |

## 2. Gramática efetivamente usada

É a de `Gramática.pdf`, acrescida dos marcadores semânticos (regras ε) que o livro introduz nos caps. 5 e 6 para executar ações no meio de uma regra:

```
START → P                         T  → 'integer' | 'char' | 'boolean' | 'string' | IDU
P   → LDE                         DT → 'type' IDD '=' 'array' '[' NUM ']' 'of' T
LDE → LDE DE | DE                    | 'type' IDD '=' 'struct' NB '{' DC '}'
DE  → DF | DT                        | 'type' IDD '=' T
DC  → DC ';' LI ':' T | LI ':' T
DF  → 'function' IDD NB '(' LP ')' ':' T MF B
LP  → LP ',' IDD ':' T | IDD ':' T
B   → '{' LDV LS '}'              LDV → LDV DV | DV            LS → LS S | S
DV  → 'var' LI ':' T ';'          LI  → LI ',' IDD | IDD
S   → 'if' '(' E ')' S
    | 'if' '(' E ')' S 'else' S
    | 'while' MW '(' E ')' S
    | 'do' MW S 'while' '(' E ')' ';'
    | NB B
    | LV '=' E ';'
    | 'break' ';' | 'continue' ';'
    | 'return' E ';'                                   ← extensão (ver §6)
E → E '&&' L | E '||' L | L
L → L '<' R | L '>' R | L '<=' R | L '>=' R | L '==' R | L '!=' R | R
R → R '+' Y | R '-' Y | Y         Y → Y '*' F | Y '/' F | F
F → LV | '++' LV | '--' LV | LV '++' | LV '--' | '(' E ')' | IDU MC '(' LE ')'
  | '-' F | '!' F | TRUE | FALSE | CHR | STR | NUM
LE → LE ',' E | E
LV → LV '.' ID | LV '[' E ']' | IDU                     ← ID no campo (cap. 5)
IDD → Id   IDU → Id   ID → Id   TRUE → 'true'   FALSE → 'false'   CHR → c   STR → s   NUM → n
NB → ε   MF → ε   MW → ε   MC → ε
```

| Marcador | Origem | Ação |
|---|---|---|
| `NB` | figs. 5.9, 5.10, 6.11, 6.14 | `NewBlock()` antes dos parâmetros da função, dos campos da struct e do bloco-comando |
| `MF` | fig. 6.13 | preenche kind, retorno e parâmetros da função antes do corpo, o que permite recursão |
| `MW` | fig. 7.22 (adaptado) | conta os laços abertos, para acusar `break`/`continue` fora de laço |
| `MC` | fig. 6.23 | confere se o identificador chamado é função e guarda os seus parâmetros |

## 3. Análise sintática

- A tabela não foi montada à mão: `buildLalrTable()` a calcula a partir de `RULES` ao iniciar o programa. O procedimento calcula FIRST e anuláveis, constrói os estados LR(1) (closure/goto) e funde, durante a construção, os estados de mesmo núcleo, propagando os lookaheads novos (seção 3.8). Não se usa bison, yacc nem outro gerador.
- Resultado: **170 estados, 84 símbolos (50 terminais e 34 não terminais), 1 conflito empilha/reduz e 0 conflitos reduz/reduz.** O único conflito é o do *else pendente* (`if (E) S . else`), resolvido por empilhar: o `else` se liga ao `if` mais interno, como em C e Pascal. O teste `testTable` garante esses números.
- O driver é o algoritmo da fig. 4.6. Como diferença, os terminais também são empilhados na pilha semântica. Assim, o atributo do i-ésimo símbolo do lado direito está sempre em `A(i)`, e o token secundário de `Id`/literais não depende de uma variável global, que no LR(1) já conteria o lookahead.
- O primeiro erro sintático interrompe a análise (o livro não define recuperação). A mensagem traz a posição, o token encontrado e os tokens que a tabela aceitaria naquele estado.

## 4. Análise de escopo

- `levels_` é a pilha de blocos (fig. 5.1). `Define`, `Search` (só no nível corrente) e `Find` (do nível corrente para fora) seguem as figs. 5.5 a 5.7.
- **Nível 0:** tipos e funções. **Função:** o nome fica no nível externo, e os parâmetros e as variáveis locais ficam no mesmo nível novo (fig. 5.2). Um bloco-comando `{ ... }` abre outro nível, que pode redeclarar nomes externos e cujas declarações deixam de valer no `}`.
- **Campos:** são declarados num nível próprio da struct (`NB`) e deixam de ser visíveis ao fim dela. O acesso `LV . ID` é resolvido pela lista de campos do tipo, e não pela tabela de símbolos (cap. 5).
- `IDD` repetido no nível → `ERR_REDECL`. `IDU` não encontrado → `ERR_NOT_DECL`; o nome é então definido com kind `UNIVERSAL_` para não gerar erros em cascata.

## 5. Análise de tipos

- Kinds: `VAR_`, `PARAM_`, `FUNCTION_`, `FIELD_`, `ARRAY_TYPE_`, `STRUCT_TYPE_`, `ALIAS_TYPE_`, `SCALAR_TYPE_` e `UNIVERSAL_` (fig. 6.1). `integer`, `char`, `boolean` e `string` são objetos únicos, e `universal` é o tipo de qualquer expressão com erro.
- Compatibilidade **por nome** (fig. 6.16): dois tipos são compatíveis se forem o mesmo objeto ou se um deles for o universal. Um alias é **sinônimo**: `T → IDU` devolve o tipo base, então `type Inteiro = integer` é compatível com `integer`.

| Construção | Exigência | Erro |
|---|---|---|
| `T → IDU` | o nome deve ser um tipo | `ERR_TYPE_EXPECTED` |
| `if`/`while`/`do` | condição boolean | `ERR_BOOL_TYPE_EXPECTED` |
| `&&`, `\|\|` | operandos boolean; resultado boolean | `ERR_BOOL_TYPE_EXPECTED` |
| `< > <= >= == !=` | operandos de tipos iguais; resultado boolean | `ERR_TYPE_MISMATCH` |
| `+` | operandos iguais, integer ou string (concatenação) | `ERR_TYPE_MISMATCH`, `ERR_INVALID_TYPE` |
| `- * /` | operandos iguais e integer | idem |
| `-F`, `!F`, `++`, `--` | integer, boolean, variável integer | `ERR_INVALID_TYPE` |
| `LV = E` | tipos iguais | `ERR_TYPE_MISMATCH` |
| `LV → IDU` | variável ou parâmetro | `ERR_KIND_NOT_VAR` |
| `LV . ID` | LV struct, e o campo deve existir | `ERR_KIND_NOT_STRUCT`, `ERR_FIELD_NOT_DECL` |
| `LV [ E ]` | LV array (→ elemento) ou string (→ char); índice integer | `ERR_KIND_NOT_ARRAY`, `ERR_INVALID_INDEX_TYPE` |
| `f(...)` | f função; quantidade e tipo dos argumentos | `ERR_KIND_NOT_FUNC`, `ERR_TOO_MANY_ARGS`, `ERR_TOO_FEW_ARGS`, `ERR_PARAM_TYPE` |
| `return E` | E compatível com o retorno da função | `ERR_TYPE_MISMATCH` |
| `break`/`continue` | dentro de um laço | erro semântico |
| `array [n]` | 1 ≤ n ≤ 999999 | tamanho de array inválido |

## 6. Decisões e ambiguidades

1. **`return`, a única extensão da linguagem.** Toda função declara um tipo de retorno, mas a gramática não tem comando para devolver o valor: o tipo de retorno não teria o que verificar e uma chamada `f(...)` não teria valor definido. Os exemplos do livro usam `return E;`. Por isso `return` virou palavra reservada (na VC era `ID`) e `S → 'return' E ';'` foi acrescentada; a análise de tipos confere `E` com o retorno da função.
2. **Blocos exigem declaração e comando.** `B → '{' LDV LS '}'` com `LDV` e `LS` não vazios foi mantido como na gramática. Por isso `while (c) { x = 1; }` é erro sintático; use `while (c) x = 1;` ou declare uma variável. Os exemplos do livro violam essa regra.
3. **Não existe comando-expressão.** `i++;` ou `f(x);` sozinhos não são comandos na gramática; escreva `i = i + 1;`.
4. **`'T'` em `DC`** é o não terminal `T` (já decidido na VC).
5. **`LV '.' ID`:** a gramática diz `IDU`, mas o cap. 5 usa `ID` para não procurar o nome do campo na tabela de símbolos (fig. 5.8, regra `ID → Id`).
6. **Tipo universal com kind `UNIVERSAL_`:** na fig. 6.3 ele tem kind escalar, o que dispararia `ERR_KIND_NOT_STRUCT` e outros erros em cascata.
7. **Listas por vetor:** nas figs. 6.6 a 6.13, a lista de parâmetros é encadeada por `pNext` no mesmo nível das variáveis locais, então a checagem de argumentos continuaria pelas variáveis locais. Os parâmetros e os campos ficam num `std::vector` do objeto.
8. **Compatibilidade por nome com alias sinônimo:** a versão estrutural da fig. 6.17 tem laço infinito nos campos, e a versão por nome tornaria um alias incompatível com a própria base.
9. **`break`/`continue` fora de laço:** o livro não dá regras de atributos para eles. O marcador `MW` (que no cap. 7 gera o rótulo do laço) aqui só conta os laços abertos.
10. **Redeclaração:** o `IDD` repetido recebe um objeto fora da tabela, para que o restante da declaração seja analisado sem sobrescrever o original.
11. **Erros:** os erros léxicos impedem a análise sintática; o primeiro erro sintático encerra a análise; os erros de escopo e de tipo são acumulados e aparecem na ordem das reduções.
12. **Fora desta entrega:** a geração de código (cap. 7) fica para a VF.

## 7. Como compilar, executar e testar

No PowerShell, a partir deste diretório:

```powershell
mingw32-make                 # gera compilador.exe
mingw32-make test            # testes léxicos + testes do compilador + exemplo

.\compilador.exe examples\fatorial.ssl              # analisa o programa
.\compilador.exe --tables examples\strings.ssl      # + tabelas de identificadores e constantes
.\compilador.exe --tokens examples\valido.ssl       # só a análise léxica (saída da VC)
Get-Content examples\minimo.ssl -Raw | .\compilador.exe -
```

Sem `make`: `g++ -std=c++11 -Wall -Wextra -Wpedantic -O2 src/main.cpp src/lexer.cpp src/lalr.cpp src/compiler.cpp -o compilador.exe`.

Códigos de saída: `0` = programa válido, `1` = erro léxico, sintático, de escopo ou de tipo, `2` = uso incorreto ou arquivo inexistente.

## 8. Exemplos

`examples/`: `fatorial.ssl` (7.4.1 do livro), `strings.ssl` (variações de 7.4.2/7.4.3), `ordenacao.ssl` (7.4.4, com struct e array de struct), `valido.ssl` e `minimo.ssl` (da VC), `erros_semanticos.ssl`, `erro_sintatico.ssl` e `erro.ssl` (léxico, da VC).

**Programa válido** (`fatorial.ssl`):

```
function fatorial(n: integer): integer {
    var i, f: integer;
    i = 2;
    f = 1;
    while (i <= n) f = f * i++;
    return f;
}
```

```
Programa valido: nenhum erro lexico, sintatico, de escopo ou de tipo.
```

**Erros de escopo e de tipo** (`erros_semanticos.ssl`):

```
type Ponto = struct { x, y: integer }

function f(a: integer, a: char): integer {
    var p: Ponto;
    var b: boolean;
    b = a + 1;
    p.z = 2;
    if (a) continue;
    a = g(1);
    a = f(1, 'c', 3);
    return "texto";
}
```

```
Foram encontrados 8 erro(s):
3:24: erro de escopo: identificador 'a' ja declarado neste escopo [ERR_REDECL]
6:9: erro de tipo: tipos incompativeis (boolean e integer) [ERR_TYPE_MISMATCH]
7:7: erro de tipo: campo 'z' nao existe em Ponto [ERR_FIELD_NOT_DECL]
8:12: erro semantico: continue fora de comando de repeticao
8:9: erro de tipo: condicao deve ser boolean [ERR_BOOL_TYPE_EXPECTED]
9:9: erro de escopo: identificador 'g' nao declarado [ERR_NOT_DECL]
10:19: erro de tipo: argumentos demais na chamada de 'f' [ERR_TOO_MANY_ARGS]
11:12: erro de tipo: retorno de 'f': tipos incompativeis (integer e string) [ERR_TYPE_MISMATCH]
```

**Erro sintático** (`erro_sintatico.ssl`, `y = x + ;`):

```
Foram encontrados 1 erro(s):
3:13: erro sintatico: token inesperado ';' (SEMI_COLON); esperado: FALSE TRUE ID NUMERAL CHARACTER STRINGVAL PLUS_PLUS MINUS MINUS_MINUS NOT LEFT_PARENTHESIS
```

## 9. Revisão de cobertura

- Das 83 regras de `RULES`, 68 têm `case` em `Semantics()`. As outras 15 (`START`, `P`, `LDE`, `DE`, `B`, `LDV`, `LS`, `TRUE`, `FALSE`, `CHR`, `STR`) só agrupam símbolos e caem no `default`; o tipo dos literais é atribuído nas regras de `F`.
- Regras de atributos implementadas: figs. 5.4–5.10 (escopo) e 6.4–6.23 (tipos de declarações, comandos, expressões, left-values e chamadas).
- `tests/compiler_tests.cpp` verifica: a tabela (1 S/R, 0 R/R); três programas válidos (fatorial com chamada de função, controle de fluxo com `break`/`continue`/else pendente, struct/array/alias/string); os 15 códigos de erro do livro, mais caractere de string, retorno, `break`/`continue` fora de laço, campo fora de escopo, bloco sem declaração e erro sintático; e as regras de escopo (redeclaração em bloco interno, parâmetro × local, variável de bloco interno fora dele, recursão).
