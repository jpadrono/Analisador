# Analisador léxico da Simple Script Language

Este diretório contém a análise pedida, a implementação funcional em C++11, exemplos e testes. As fontes primárias consultadas foram `../Gramática.pdf` (uma página) e `../Compiladoresv1.2.1.pdf` (105 páginas). O nome do segundo arquivo diverge do nome `Compiladoresv1.2.3_analise-lexica` citado no enunciado; como não há arquivo com esse nome na pasta, o PDF disponível foi adotado como a fonte teórica correspondente.

## 1. Entendimento da linguagem

O capítulo 2 do livro define análise léxica como a fase que lê caracteres, reconhece lexemas válidos e associa cada lexema a um token. Ele também distingue o token principal (a categoria sintática) do token secundário usado para diferenciar identificadores e constantes da mesma categoria. As páginas 5 a 8 apresentam a Simple Script Language; as páginas 8 a 12 fornecem as expressões regulares, a enumeração de tokens e um esboço do autômato do lexer.

O arquivo `Gramática.pdf` fornece a gramática usada no trabalho. Seus terminais literais determinam as palavras reservadas, os operadores e os delimitadores. Os símbolos `Id`, `n`, `c` e `s` são as quatro famílias regulares que o lexer deve reconhecer. Os demais símbolos, como `P`, `LDE`, `S`, `E`, `LV`, `IDD` e `IDU`, são não terminais e pertencem ao analisador sintático, não constituindo novos tokens léxicos.

O programa implementado percorre diretamente os caracteres da entrada. Ele não usa gerador de lexer, expressão regular pronta nem biblioteca que realize a tokenização.

## 2. Tokens

| Token | Lexema ou padrão | Categoria | Observação |
|---|---|---|---|
| `ARRAY` | `array` | palavra reservada | declaração de array |
| `BOOLEAN` | `boolean` | palavra reservada | tipo escalar |
| `BREAK` | `break` | palavra reservada | controle de repetição |
| `CHAR` | `char` | palavra reservada | tipo escalar |
| `CONTINUE` | `continue` | palavra reservada | controle de repetição |
| `DO` | `do` | palavra reservada | repetição pós-testada |
| `ELSE` | `else` | palavra reservada | alternativa de seleção |
| `FALSE` | `false` | palavra reservada/literal booleano | terminal de `FALSE` |
| `FUNCTION` | `function` | palavra reservada | declaração de função |
| `IF` | `if` | palavra reservada | seleção |
| `INTEGER` | `integer` | palavra reservada | tipo escalar |
| `OF` | `of` | palavra reservada | tipo do elemento do array |
| `STRING` | `string` | palavra reservada | tipo escalar |
| `STRUCT` | `struct` | palavra reservada | declaração de estrutura |
| `TRUE` | `true` | palavra reservada/literal booleano | terminal de `TRUE` |
| `TYPE` | `type` | palavra reservada | declaração de tipo |
| `VAR` | `var` | palavra reservada | declaração de variável |
| `WHILE` | `while` | palavra reservada | repetição |
| `ID` | `[A-Za-z][A-Za-z0-9]*` | identificador | recebe índice na tabela de identificadores |
| `NUMERAL` | `[0-9]+` | literal inteiro | recebe índice na tabela de constantes |
| `CHARACTER` | `'` + um caractere + `'` | literal caractere | exatamente um caractere, sem regra de escape |
| `STRINGVAL` | `"` + zero ou mais caracteres + `"` | literal string | termina na próxima aspa dupla; pode ser vazia |
| `EQUALS` | `=` | operador | atribuição |
| `AND` | `&&` | operador lógico | `&` isolado é erro |
| `OR` | `\|\|` | operador lógico | `\|` isolado é erro |
| `LESS_THAN` | `<` | operador relacional | — |
| `GREATER_THAN` | `>` | operador relacional | — |
| `LESS_OR_EQUAL` | `<=` | operador relacional | reconhecido antes de `<` por maximal munch |
| `GREATER_OR_EQUAL` | `>=` | operador relacional | reconhecido antes de `>` por maximal munch |
| `EQUAL_EQUAL` | `==` | operador relacional | reconhecido antes de `=` por maximal munch |
| `NOT_EQUAL` | `!=` | operador relacional | reconhecido antes de `!` por maximal munch |
| `PLUS` | `+` | operador aritmético | — |
| `PLUS_PLUS` | `++` | operador de incremento | prefixo ou sufixo na gramática |
| `MINUS` | `-` | operador aritmético/unário | o sinal não integra `NUMERAL` |
| `MINUS_MINUS` | `--` | operador de decremento | prefixo ou sufixo na gramática |
| `TIMES` | `*` | operador aritmético | — |
| `DIVIDE` | `/` | operador aritmético | não inicia comentário, pois comentários não foram definidos |
| `NOT` | `!` | operador lógico unário | — |
| `COLON` | `:` | delimitador | — |
| `SEMI_COLON` | `;` | delimitador | — |
| `COMMA` | `,` | delimitador | — |
| `LEFT_SQUARE` | `[` | delimitador | — |
| `RIGHT_SQUARE` | `]` | delimitador | — |
| `LEFT_BRACES` | `{` | delimitador | — |
| `RIGHT_BRACES` | `}` | delimitador | — |
| `LEFT_PARENTHESIS` | `(` | delimitador | — |
| `RIGHT_PARENTHESIS` | `)` | delimitador | — |
| `DOT` | `.` | delimitador/acesso | acesso a campo de estrutura |
| `EOF` | fim físico do arquivo | controle do lexer | não é um lexema escrito nem o caractere `$` |

Espaço, tabulação vertical ou horizontal, avanço de página e quebras `LF`, `CR` ou `CRLF` são separadores ignorados. Eles apenas atualizam linha e coluna.

## 3. Regras léxicas

1. O lexer ignora separadores e observa o primeiro caractere ainda não consumido.
2. Uma letra ASCII inicia `ID`; letras e dígitos seguintes são consumidos. O lexema completo é então procurado na tabela de palavras reservadas. A busca só ocorre depois da leitura completa, portanto `integer2` é `ID`, não `INTEGER NUMERAL`.
3. Um dígito inicia `NUMERAL`, e todos os dígitos consecutivos são consumidos. Não se impõe limite numérico que a gramática não tenha especificado; o valor é preservado como texto na tabela de constantes.
4. Aspa simples inicia `CHARACTER`. Deve haver exatamente um caractere antes da aspa final.
5. Aspa dupla inicia `STRINGVAL`. O lexer consome até a próxima aspa dupla, inclusive através de quebra de linha, porque `any` foi definido como qualquer caractere lido do arquivo.
6. Operadores com dois caracteres são reconhecidos pelo princípio do maior prefixo válido: `++`, `--`, `&&`, `||`, `<=`, `>=`, `==` e `!=` têm precedência léxica sobre suas formas de um caractere.
7. Um caractere que não inicia token válido gera erro com linha, coluna e lexema. A análise continua quando existe um ponto seguro de recuperação. String não terminada consome até o EOF.
8. Identificadores repetidos reutilizam o mesmo token secundário. Cada ocorrência de literal recebe uma entrada na tabela de constantes, como sugerido pela implementação teórica.

Palavras reservadas são sensíveis a maiúsculas e minúsculas: `while` é reservada, mas `While` é um `ID`.

## 4. Decisões e ambiguidades

- **Nome da fonte teórica:** o arquivo citado no pedido não existe. Foi usado integralmente `Compiladoresv1.2.1.pdf`, o único PDF teórico disponível.
- **`IDD` e `IDU`:** distinguem declaração e uso apenas nas fases sintática/semântica. Ambos derivam `Id`; o lexer emite somente `ID`.
- **`'T'` em `DC`:** a produção dos campos escreve `LI ':' 'T'`, mas `T` é um não terminal definido, e os capítulos de atributos consultam `T.type`. Isso foi tratado como erro tipográfico. Não foi criada palavra reservada `T`.
- **Terminais sem aspas:** `while` em `do S while (...)` e `=` em `LV = E` aparecem sem aspas, embora sejam inequivocamente a palavra reservada e o operador usados nas outras produções e na enumeração do livro. Eles geram `WHILE` e `EQUALS`.
- **Numeral:** a gramática isolada mostra `n = digito * digito`, uma notação ambígua. O livro escreve explicitamente `n = digito . digito*`; logo, foi adotado um ou mais dígitos (`[0-9]+`).
- **Sublinhado:** a expressão formal `Id = letra.(letra+digito)*` exclui `_`, embora o pseudocódigo posterior do livro teste `_`. A regra formal e a gramática prevaleceram; `_` é erro léxico.
- **Literais e escapes:** não existe definição de escape. Embora capítulos posteriores mostrem `\0` em exemplos de caractere, aceitá-lo exigiria acrescentar uma regra ausente. A implementação é estrita: `'A'` é válido e `'\0'` contém dois caracteres, portanto é erro. Strings terminam na próxima `"`; `\"` não é escape.
- **`any`:** uma aspa igual ao delimitador não pode simultaneamente ser conteúdo sem uma regra de escape. Para string, vale a primeira aspa dupla seguinte. Para caractere, vale exatamente um caractere seguido de aspa simples.
- **Comentários:** nenhum dos PDFs define comentário. Assim, não há comentário a ignorar; `//x` produz `DIVIDE DIVIDE ID` e `/*x*/` produz operadores e identificador.
- **Fim de arquivo:** `EOF` é um marcador de controle necessário para encerrar o consumidor de tokens. O `$` citado no capítulo de parsing pertence à gramática aumentada e não foi inventado como lexema de entrada.
- **Construções ausentes:** `return` aparece em exemplos posteriores de geração de código, mas não na gramática fornecida nem na enumeração léxica. Portanto, é reconhecido como `ID`, não como palavra reservada.
- **Questões puramente sintáticas:** obrigatoriedade de listas não vazias, associação do `else`, precedência de operadores, validade de um bloco e ordem de declarações não são verificadas pelo lexer.

## 5. Implementação

A implementação completa está dividida em:

- `src/lexer.hpp`: categorias, registros de token/erro e interface do lexer;
- `src/lexer.cpp`: reconhecimento caractere a caractere, tabelas secundárias, erros e nomes dos tokens;
- `src/main.cpp`: leitura de arquivo ou entrada padrão, interface de linha de comando e impressão;
- `tests/lexer_tests.cpp`: testes automatizados de todas as categorias e dos principais erros.

O fluxo principal chama `scanAll()`, que repete o estado inicial do autômato até emitir `EOF`. Os tokens contêm categoria, lexema, linha, coluna e, quando aplicável, token secundário. Erros ficam em uma coleção separada, são enviados para `stderr` e fazem o programa terminar com código `1`. Falha de uso ou de abertura do arquivo termina com código `2`.

## 6. Como compilar e executar

No PowerShell, a partir deste diretório:

```powershell
g++ -std=c++11 -Wall -Wextra -Wpedantic -O2 src/main.cpp src/lexer.cpp -o analisador_lexico.exe
.\analisador_lexico.exe examples\valido.ssl
.\analisador_lexico.exe --tables examples\valido.ssl
```

Com o `mingw32-make`:

```powershell
mingw32-make
mingw32-make test
```

Para ler da entrada padrão, use `-` como nome do arquivo:

```powershell
Get-Content examples\minimo.ssl -Raw | .\analisador_lexico.exe -
```

Em Linux/macOS, troque o nome final por `analisador_lexico` e execute `./analisador_lexico`.

## 7. Exemplos de entrada e saída

Entrada válida em `examples/minimo.ssl`:

```text
function demo(x: integer): integer {
    var s: string;
    s = "ok";
    x = x + 10;
}
```

Saída produzida (sem o cabeçalho tabular):

```text
1:1   FUNCTION           -  function
1:10  ID                 0  demo
1:14  LEFT_PARENTHESIS   -  (
1:15  ID                 1  x
1:16  COLON              -  :
1:18  INTEGER            -  integer
1:25  RIGHT_PARENTHESIS  -  )
1:26  COLON              -  :
1:28  INTEGER            -  integer
1:36  LEFT_BRACES        -  {
2:5   VAR                -  var
2:9   ID                 2  s
2:10  COLON              -  :
2:12  STRING             -  string
2:18  SEMI_COLON         -  ;
3:5   ID                 2  s
3:7   EQUALS             -  =
3:9   STRINGVAL          0  "ok"
3:13  SEMI_COLON         -  ;
4:5   ID                 1  x
4:7   EQUALS             -  =
4:9   ID                 1  x
4:11  PLUS               -  +
4:13  NUMERAL            1  10
4:15  SEMI_COLON         -  ;
5:1   RIGHT_BRACES       -  }
6:1   EOF                -  <fim do arquivo>
```

`examples/valido.ssl` é um exemplo maior, com tipos `array` e `struct`, duas funções, booleanos, caractere, string, indexação, seleção, repetição e operadores compostos.

Entrada inválida em `examples/erro.ssl` contém `_`, `&` isolado e string não terminada. O programa preserva os tokens válidos e reporta:

```text
Foram encontrados 3 erro(s) lexico(s):
2:13: erro lexico: caractere nao pertence ao alfabeto lexico da linguagem [lexema: _]
3:19: erro lexico: '&' isolado nao e token valido; use '&&' [lexema: &]
4:12: erro lexico: literal de string nao terminado [lexema: "string sem fechamento;\n}\n]
```

## 8. Relação com a gramática

Da gramática foram usados na fase léxica:

- todos os terminais literais entre aspas, que viraram palavras reservadas, operadores ou delimitadores;
- `Id`, `n`, `c` e `s`, cujas definições regulares viraram `ID`, `NUMERAL`, `CHARACTER` e `STRINGVAL`;
- a ocorrência contextual de `while` e `=`, que confirma os dois terminais impressos sem aspas;
- `IDD -> Id`, `IDU -> Id`, `TRUE -> 'true'`, `FALSE -> 'false'`, `CHR -> c`, `STR -> s` e `NUM -> n`, que mostram o mapeamento entre os tokens do lexer e os símbolos consumidos pelo parser.

Não foram implementadas as produções de `P`, declarações, comandos ou expressões. Elas definem como os tokens podem ser combinados e pertencem à análise sintática. Da mesma forma, distinguir declaração de uso de identificador, verificar escopo, tipos ou validade de operandos são responsabilidades de fases posteriores.

### Revisão de cobertura

O teste `tests/lexer_tests.cpp` enumera e verifica as 18 palavras reservadas, as quatro categorias regulares, todos os 16 operadores, os dez delimitadores e `EOF`. Também verifica tokens secundários, CRLF, linha/coluna, `_`, `&`/`|` isolados, caractere vazio/longo/não terminado, string não terminada, caractere desconhecido e a decisão explícita de não tratar `//` como comentário. `mingw32-make test` foi executado com sucesso.
