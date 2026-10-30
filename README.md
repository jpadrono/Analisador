# Compilador da Simple Script Language (VF)

Trabalho final: o compilador completo da SSL em C++11. Cada entrega está marcada por uma tag deste repositório:

| Tag | Entrega | Conteúdo |
|---|---|---|
| `VC` | VC | análise léxica |
| `VE` | VE | + análise sintática, de escopo e de tipos |
| `VF` | VF (esta) | + geração de código para a máquina virtual do livro |

Em relação à VE, esta entrega acrescenta a geração de código (§6) e os marcadores que ela exige (`NF`, `MT`, `ME`, `MD`, §2). Sem nenhum erro, o compilador agora escreve o assembly na saída padrão, em vez da mensagem "Programa valido".

Fontes: a gramática fornecida pelo professor (`Gramática.pdf`) e os capítulos 3 a 7 do livro *Compiladores* v1.2.3 (Prof. Luiz Guedes).

## 1. Entendimento

O compilador faz uma única passada dirigida pela sintaxe, como no livro:

```
fonte ─► Lexer (VC) ─► tokens ─► parser la-LR(1) ──redução da regra r──► Semantics(r)
                                   (fig. 4.6)                             ├─ escopo  (cap. 5)
                                                                          ├─ tipos   (cap. 6)
                                                                          └─ código  (cap. 7)
```

- **Sintático (cap. 3.8 e 4.1):** autômato de pilha ascendente la-LR(1) dirigido pela tabela `Action[estado][símbolo]`. A cada redução, chama `Semantics(regra)`, que lê os atributos do lado direito na pilha semântica e empilha o atributo do lado esquerdo.
- **Escopo (cap. 5):** tabela de símbolos como pilha de blocos. Os não terminais `IDD` e `IDU` distinguem declaração e uso.
- **Tipos (cap. 6):** cada identificador tem um *kind* e um objeto de tipo. As expressões sintetizam `type`, e cada regra verifica os seus operandos.
- **Geração de código (cap. 7):** assembly textual da máquina de pilha, na versão com *left-values* (`LOAD_REF`/`DE_REF`/`STORE_REF`), porque a linguagem tem arrays e structs.

| Arquivo | Conteúdo |
|---|---|
| `src/lexer.*` | lexer da VC, mais a palavra reservada `return` |
| `src/lalr.*` | construção da tabela la-LR(1) a partir de qualquer gramática |
| `src/compiler.*` | gramática da SSL, driver da fig. 4.6, `Semantics()` com escopo, tipos e código |
| `src/main.cpp` | linha de comando |
| `tests/lexer_tests.cpp` | testes da VC |
| `tests/compiler_tests.cpp` | tabela, código gerado esperado e um caso para cada erro |

## 2. Gramática efetivamente usada

É a de `Gramática.pdf`, acrescida dos marcadores semânticos (regras ε) que o livro introduz nos caps. 5 a 7 para executar ações no meio de uma regra:

```
START → P                         T  → 'integer' | 'char' | 'boolean' | 'string' | IDU
P   → LDE                         DT → 'type' IDD '=' 'array' '[' NUM ']' 'of' T
LDE → LDE DE | DE                    | 'type' IDD '=' 'struct' NB '{' DC '}'
DE  → DF | DT                        | 'type' IDD '=' T
DC  → DC ';' LI ':' T | LI ':' T
DF  → 'function' IDD NF '(' LP ')' ':' T MF B
LP  → LP ',' IDD ':' T | IDD ':' T
B   → '{' LDV LS '}'              LDV → LDV DV | DV            LS → LS S | S
DV  → 'var' LI ':' T ';'          LI  → LI ',' IDD | IDD
S   → 'if' '(' E ')' MT S
    | 'if' '(' E ')' MT S 'else' ME S
    | 'while' MW '(' E ')' MT S
    | 'do' MW S 'while' MD '(' E ')' ';'
    | NB B
    | LV '=' E ';'
    | 'break' ';' | 'continue' ';'
    | 'return' E ';'                                   ← extensão (ver §7)
E → E '&&' L | E '||' L | L
L → L '<' R | L '>' R | L '<=' R | L '>=' R | L '==' R | L '!=' R | R
R → R '+' Y | R '-' Y | Y         Y → Y '*' F | Y '/' F | F
F → LV | '++' LV | '--' LV | LV '++' | LV '--' | '(' E ')' | IDU MC '(' LE ')'
  | '-' F | '!' F | TRUE | FALSE | CHR | STR | NUM
LE → LE ',' E | E
LV → LV '.' ID | LV '[' E ']' | IDU                     ← ID no campo (cap. 5)
IDD → Id   IDU → Id   ID → Id   TRUE → 'true'   FALSE → 'false'   CHR → c   STR → s   NUM → n
NB → ε  NF → ε  MF → ε  MT → ε  ME → ε  MW → ε  MD → ε  MC → ε
```

| Marcador | Origem | Ação |
|---|---|---|
| `NB` | fig. 5.9, 6.14 | `NewBlock()` antes dos campos da struct e no bloco-comando |
| `NF` | fig. 7.18 | define a função (kind, índice no pool) e abre o bloco dos parâmetros |
| `MF` | fig. 6.13, 7.18 | preenche retorno e parâmetros antes do corpo, o que permite recursão; marca onde entra `BEGIN_FUNC` |
| `MT` | fig. 7.21 | `TJMP_FW` depois da condição |
| `ME` | fig. 7.21 | `JMP_FW` sobre o `else` e o rótulo do `if` |
| `MW` | fig. 7.22 | rótulo de início do laço; empilha os destinos de `break`/`continue` |
| `MD` | novo | rótulo antes da condição do `do-while`, destino do `continue` |
| `MC` | fig. 6.23 | confere se o identificador chamado é função e guarda os seus parâmetros |

## 3. Análise sintática

- A tabela não foi montada à mão: `buildLalrTable()` a calcula a partir de `RULES` ao iniciar o programa. O procedimento calcula FIRST e anuláveis, constrói os estados LR(1) (closure/goto) e funde, durante a construção, os estados de mesmo núcleo, propagando os lookaheads novos (seção 3.8). Não se usa bison, yacc nem outro gerador.
- Resultado: **174 estados, 88 símbolos (50 terminais e 38 não terminais), 1 conflito empilha/reduz e 0 conflitos reduz/reduz.** O único conflito é o do *else pendente* (`if (E) S . else`), resolvido por empilhar: o `else` se liga ao `if` mais interno, como em C e Pascal. O teste `testTable` garante esses números, e `testControlFlow` confere o código gerado para `if (a) if (b) ... else ...`.
- O driver é o algoritmo da fig. 4.6. Como diferença, os terminais também são empilhados na pilha semântica. Assim, o atributo do i-ésimo símbolo do lado direito está sempre em `A(i)`, e o token secundário de `Id`/literais não depende de uma variável global, que no LR(1) já conteria o lookahead.
- O primeiro erro sintático interrompe a compilação (o livro não define recuperação). A mensagem traz a posição, o token encontrado e os tokens que a tabela aceitaria naquele estado.

## 4. Análise de escopo

- `levels_` é a pilha de blocos (fig. 5.1). `Define`, `Search` (só no nível corrente) e `Find` (do nível corrente para fora) seguem as figs. 5.5 a 5.7.
- **Nível 0:** tipos e funções. **Função:** o nome fica no nível externo, e os parâmetros e as variáveis locais ficam no mesmo nível novo (fig. 5.2). Um bloco-comando `{ ... }` abre outro nível, que pode redeclarar nomes externos.
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
| `LV = E` | tipos iguais (arrays e structs são copiados) | `ERR_TYPE_MISMATCH` |
| `LV → IDU` | variável ou parâmetro | `ERR_KIND_NOT_VAR` |
| `LV . ID` | LV struct, e o campo deve existir | `ERR_KIND_NOT_STRUCT`, `ERR_FIELD_NOT_DECL` |
| `LV [ E ]` | LV array (→ elemento) ou string (→ char); índice integer | `ERR_KIND_NOT_ARRAY`, `ERR_INVALID_INDEX_TYPE` |
| `f(...)` | f função; quantidade e tipo dos argumentos | `ERR_KIND_NOT_FUNC`, `ERR_TOO_MANY_ARGS`, `ERR_TOO_FEW_ARGS`, `ERR_PARAM_TYPE` |
| `return E` | E compatível com o retorno da função | `ERR_TYPE_MISMATCH` |
| `break`/`continue` | dentro de um laço | erro semântico |
| `array [n]` | 1 ≤ n ≤ 999999 | tamanho de array inválido |

## 6. Geração de código

**Endereçamento (fig. 7.11 a 7.19).** Os tamanhos são medidos em valores escalares: escalar = 1, array = n × elemento, struct = soma dos campos, alias = tamanho da base. Os parâmetros são numerados a partir de 0 e as variáveis locais continuam a numeração, inclusive nas declaradas em blocos internos. Cada campo guarda o seu deslocamento dentro da struct. `BEGIN_FUNC n, p, v` informa o índice da função no pool, o tamanho dos parâmetros e o tamanho total de parâmetros + variáveis.

| Construção | Código gerado |
|---|---|
| função | `BEGIN_FUNC n, p, v` ‖B‖ `END_FUNC` |
| `if (E) S` | ‖E‖ `TJMP_FW L1` ‖S‖ `L1:` |
| `if (E) S1 else S2` | ‖E‖ `TJMP_FW L1` ‖S1‖ `JMP_FW L2` `L1:` ‖S2‖ `L2:` |
| `while (E) S` | `L1:` ‖E‖ `TJMP_FW L2` ‖S‖ `JMP_BW L1` `L2:` |
| `do S while (E);` | `L1:` ‖S‖ `L3:` ‖E‖ `NOT` `TJMP_BW L1` `L2:` |
| `break` / `continue` | `JMP_FW L2` / `JMP_BW L1` (while) ou `JMP_FW L3` (do) |
| `LV = E;` | ‖LV‖ ‖E‖ `STORE_REF tam` |
| `return E;` | ‖E‖ `RET` |
| `E1 op E2` | ‖E1‖ ‖E2‖ `ADD SUB MUL DIV AND OR LT GT LE GE EQ NEQ` |
| `-F`, `!F` | ‖F‖ `NEG` / `NOT` |
| literais | `LOAD_CONST k` (k = token secundário), `LOAD_TRUE`, `LOAD_FALSE` |
| `LV → IDU` | `LOAD_REF endereço` |
| `LV . campo` | ‖LV‖ `ADD deslocamento` |
| `LV [E]` (array) | ‖LV‖ ‖E‖ `MUL tam_elem` `ADD` |
| `F → LV` | ‖LV‖ `DE_REF tam` |
| `++LV` / `LV++` | ‖LV‖ `DUP DUP DE_REF 1 INC STORE_REF 1 DE_REF 1` (+ `DEC` no pós) |
| `f(E1,...,En)` | ‖E1‖...‖En‖ `CALL n` |
| `s[E]` lido / atribuído | ‖s‖ ‖E‖ `CALL_LIB 0, 0` / ‖s‖ ‖E‖ ‖valor‖ `CALL_LIB 0, 1` `POP` |

O código é emitido em ordem, durante as reduções. O cabeçalho `BEGIN_FUNC` é inserido na posição marcada por `MF` quando a função termina, porque só então o total de variáveis é conhecido; o livro faz o mesmo com `fseek`. Se houver qualquer erro, nenhum código é produzido (o `ERROR_FLAG` do livro).

## 7. Decisões e ambiguidades

1. **`return`, a única extensão da linguagem.** A gramática tem o tipo de retorno, mas não tem como devolver um valor. Como os parâmetros são passados por valor e não existem variáveis globais nem E/S, nenhum programa teria efeito observável. Todos os exemplos do cap. 7 usam `return E;` → ‖E‖ `RET`. Por isso `return` virou palavra reservada (na VC era `ID`) e `S → 'return' E ';'` foi acrescentada.
2. **Blocos exigem declaração e comando.** `B → '{' LDV LS '}'` com `LDV` e `LS` não vazios foi mantido como na gramática. Por isso `while (c) { x = 1; }` é erro sintático; use `while (c) x = 1;` ou declare uma variável. Os exemplos do livro violam essa regra.
3. **Não existe comando-expressão.** `i++;` ou `f(x);` sozinhos não são comandos na gramática; escreva `i = i + 1;`.
4. **`'T'` em `DC`** é o não terminal `T` (já decidido na VC).
5. **`LV '.' ID`:** a gramática diz `IDU`, mas o cap. 5 usa `ID` para não procurar o nome do campo na tabela de símbolos (fig. 5.8, regra `ID → Id`).
6. **`NewBlock()` em `NF`:** a fig. 7.18 o coloca em `MF`, mas o texto diz que `NF` substitui `NB`. Os parâmetros precisam do bloco novo antes de `LP`.
7. **`LOAD_REF` na fig. 7.27** usa o tamanho do tipo como operando. O correto, e o que os exemplos mostram, é o endereço da variável.
8. **Tipo universal com kind `UNIVERSAL_`:** na fig. 6.3 ele tem kind escalar, o que dispararia `ERR_KIND_NOT_STRUCT` e outros erros em cascata.
9. **Listas por vetor:** nas figs. 6.6 a 6.13, a lista de parâmetros é encadeada por `pNext` no mesmo nível das variáveis locais, então a checagem de argumentos continuaria pelas variáveis locais. Os parâmetros e os campos ficam num `std::vector` do objeto.
10. **Compatibilidade por nome com alias sinônimo:** a versão estrutural da fig. 6.17 tem laço infinito nos campos, e a versão por nome tornaria um alias incompatível com a própria base.
11. **`v` em `BEGIN_FUNC`** inclui os parâmetros, como dizem o texto e a fig. 7.18. Os exemplos 7.4 mostram só os locais e são inconsistentes entre si (o 7.4.3 tem 3 locais e informa 1).
12. **`NEQ`:** o conjunto de instruções (fig. 7.2) e os exemplos usam `NEQ`; as figs. 7.6 e 7.24 escrevem `NE`.
13. **`true`/`false` → `LOAD_TRUE`/`LOAD_FALSE`** (fig. 7.7). A fig. 7.26 usa `LOAD_CONST tokenSecundario`, mas essas palavras reservadas não têm token secundário.
14. **Caracteres de string via `CALL_LIB`:** a geração com left-values (fig. 7.27) só trata arrays, e o livro recomenda funções de biblioteca para strings em linguagens com tipos estruturados. A biblioteca 0 ficou com `0` = `charAt(ref, i)` e `1` = `setChar(ref, i, c)`. Como `CALL_LIB` sempre empilha um resultado, a atribuição gera também um `POP`.
15. **`continue` no `do-while`** vai para a condição (`JMP_FW`, fig. 7.5). Isso exigiu o marcador `MD`. `break`/`continue` não tinham regras de atributos no livro.
16. **Redeclaração:** o `IDD` repetido recebe um objeto fora da tabela, para que o restante da declaração seja analisado sem sobrescrever o original.
17. **Erros:** os erros léxicos impedem a análise sintática; o primeiro erro sintático encerra a compilação; os erros de escopo e de tipo são acumulados e aparecem na ordem das reduções.
18. **Fora do escopo do enunciado:** montador e interpretador da máquina virtual. O compilador entrega o assembly; o pool de constantes sai com `--tables`.

## 8. Como compilar, executar e testar

No PowerShell, a partir deste diretório:

```powershell
mingw32-make                 # gera compilador.exe
mingw32-make test            # testes léxicos + testes do compilador + exemplo

.\compilador.exe examples\fatorial.ssl              # assembly na saída padrão
.\compilador.exe examples\fatorial.ssl > fatorial.asm
.\compilador.exe --tables examples\strings.ssl      # + tabelas de identificadores e constantes
.\compilador.exe --tokens examples\valido.ssl       # só a análise léxica (saída da VC)
Get-Content examples\minimo.ssl -Raw | .\compilador.exe -
```

Sem `make`: `g++ -std=c++11 -Wall -Wextra -Wpedantic -O2 src/main.cpp src/lexer.cpp src/lalr.cpp src/compiler.cpp -o compilador.exe`.

Códigos de saída: `0` = compilou, `1` = erro léxico, sintático ou semântico, `2` = uso incorreto ou arquivo inexistente.

## 9. Exemplos

`examples/`: `fatorial.ssl` (7.4.1), `strings.ssl` (variações de 7.4.2/7.4.3), `ordenacao.ssl` (7.4.4, com struct e array de struct), `valido.ssl` e `minimo.ssl` (da VC), `erros_semanticos.ssl`, `erro_sintatico.ssl` e `erro.ssl` (léxico, da VC).

**Entrada** (`fatorial.ssl`, primeira função):

```
function fatorial(n: integer): integer {
    var i, f: integer;
    i = 2;
    f = 1;
    while (i <= n) f = f * i++;
    return f;
}
```

**Saída** (`n` = 0, `i` = 1, `f` = 2; constantes `2` = 0 e `1` = 1):

```
BEGIN_FUNC 0, 1, 3
	LOAD_REF 1
	LOAD_CONST 0
	STORE_REF 1
	LOAD_REF 2
	LOAD_CONST 1
	STORE_REF 1
L0:
	LOAD_REF 1
	DE_REF 1
	LOAD_REF 0
	DE_REF 1
	LE
	TJMP_FW L1
	LOAD_REF 2
	LOAD_REF 2
	DE_REF 1
	LOAD_REF 1
	DUP
	DUP
	DE_REF 1
	INC
	STORE_REF 1
	DE_REF 1
	DEC
	MUL
	STORE_REF 1
	JMP_BW L0
L1:
	LOAD_REF 2
	DE_REF 1
	RET
END_FUNC
```

**Erros semânticos** (`erros_semanticos.ssl`):

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
3:13: erro sintatico: token inesperado ';' (SEMI_COLON); esperado: FALSE TRUE ID NUMERAL CHARACTER STRINGVAL PLUS_PLUS MINUS MINUS_MINUS NOT LEFT_PARENTHESIS
```

## 10. Revisão de cobertura

- Das 87 regras de `RULES`, 74 têm `case` em `Semantics()`. As outras 13 (`START`, `P`, `LDE`, `DE`, `B`, `LDV`, `LS`, `TRUE`, `FALSE`) só agrupam símbolos e caem no `default`, como no livro.
- Regras de atributos implementadas: figs. 5.4–5.10 (escopo); 6.4–6.23 (tipos de declarações, comandos, expressões, left-values e chamadas); 7.4–7.10 e 7.12–7.28 (código). A fig. 7.29 (versão sem left-values: `LOAD_VAR`, `STORE_VAR`, `CHAR_AT`, `SET_CHAR`) não foi usada porque a SSL tem tipos estruturados (§7, item 14).
- `tests/compiler_tests.cpp` verifica: a tabela (1 S/R, 0 R/R); o código completo esperado de quatro programas (fatorial + chamada, controle de fluxo com `break`/`continue`/else pendente, struct/array/alias/string e cópia de struct); os 15 códigos de erro do livro, mais caractere de string, retorno, `break`/`continue` fora de laço, campo fora de escopo, bloco sem declaração e erro sintático; e as regras de escopo (redeclaração em bloco interno, parâmetro × local, variável de bloco interno fora dele, recursão).
