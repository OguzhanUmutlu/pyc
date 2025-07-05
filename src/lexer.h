#ifndef PYC_LEXER_H
#define PYC_LEXER_H

#include "utils.h"

#define TOKEN_KEYWORD 0x0
#define TOKEN_KEYWORD_IF 0x010
#define TOKEN_KEYWORD_ELSE 0x020
#define TOKEN_KEYWORD_ELIF 0x030
#define TOKEN_KEYWORD_FOR 0x040
#define TOKEN_KEYWORD_WHILE 0x050
#define TOKEN_KEYWORD_DEF 0x060
#define TOKEN_KEYWORD_RETURN 0x070
#define TOKEN_KEYWORD_CLASS 0x080
#define TOKEN_KEYWORD_IMPORT 0x090
#define TOKEN_KEYWORD_FROM 0x0A0
#define TOKEN_KEYWORD_AS 0x0B0
#define TOKEN_KEYWORD_WITH 0x0C0
#define TOKEN_KEYWORD_TRY 0x0D0
#define TOKEN_KEYWORD_EXCEPT 0x0E0
#define TOKEN_KEYWORD_FINALLY 0x0F0
#define TOKEN_KEYWORD_RAISE 0x100
#define TOKEN_KEYWORD_PASS 0x110
#define TOKEN_KEYWORD_BREAK 0x120
#define TOKEN_KEYWORD_CONTINUE 0x130
#define TOKEN_KEYWORD_LAMBDA 0x140
#define TOKEN_KEYWORD_YIELD 0x150
#define TOKEN_KEYWORD_GLOBAL 0x160
#define TOKEN_KEYWORD_NONLOCAL 0x170
#define TOKEN_KEYWORD_ASSERT 0x180
#define TOKEN_KEYWORD_DEL 0x190
#define TOKEN_KEYWORD_ASYNC 0x1A0
#define TOKEN_KEYWORD_AWAIT 0x1B0
#define TOKEN_KEYWORD_MATCH 0x1C0
#define TOKEN_KEYWORD_CASE 0x1D0

#define TOKEN_BOOLEAN 0x1
#define TOKEN_BOOLEAN_FALSE 0x01
#define TOKEN_BOOLEAN_TRUE 0x11

#define TOKEN_OPERATOR 0x2
#define TOKEN_OPERATOR_EQ 0x002
#define TOKEN_OPERATOR_POW 0x012
#define TOKEN_OPERATOR_ADD 0x022
#define TOKEN_OPERATOR_SUB 0x032
#define TOKEN_OPERATOR_MUL 0x042
#define TOKEN_OPERATOR_DIV 0x052
#define TOKEN_OPERATOR_FDIV 0x062
#define TOKEN_OPERATOR_MOD 0x072
#define TOKEN_OPERATOR_BIT_AND 0x082
#define TOKEN_OPERATOR_BIT_OR 0x092
#define TOKEN_OPERATOR_BIT_XOR 0x0A2
#define TOKEN_OPERATOR_BIT_NOT 0x0B2
#define TOKEN_OPERATOR_LSH 0x0C2
#define TOKEN_OPERATOR_RSH 0x0D2
#define TOKEN_OPERATOR_MAT_MUL 0x0E2
#define TOKEN_OPERATOR_NEQ 0x0F2
#define TOKEN_OPERATOR_GTE 0x102
#define TOKEN_OPERATOR_LTE 0x112
#define TOKEN_OPERATOR_GT 0x122
#define TOKEN_OPERATOR_LT 0x132
#define TOKEN_OPERATOR_IN 0x142
#define TOKEN_OPERATOR_IS 0x152
#define TOKEN_OPERATOR_NOT 0x162
#define TOKEN_OPERATOR_AND 0x172
#define TOKEN_OPERATOR_OR 0x182
#define TOKEN_OPERATOR_WALRUS 0x192
#define TOKEN_OPERATOR_NOT_IN 0x1A2
#define TOKEN_OPERATOR_IS_NOT 0x1B2

#define TOKEN_SET_OPERATOR 0x3
#define TOKEN_SET_OPERATOR_EQ 0x03
#define TOKEN_SET_OPERATOR_POW 0x13
#define TOKEN_SET_OPERATOR_ADD 0x23
#define TOKEN_SET_OPERATOR_SUB 0x33
#define TOKEN_SET_OPERATOR_MUL 0x43
#define TOKEN_SET_OPERATOR_DIV 0x53
#define TOKEN_SET_OPERATOR_FDIV 0x63
#define TOKEN_SET_OPERATOR_MOD 0x73
#define TOKEN_SET_OPERATOR_BIT_AND 0x83
#define TOKEN_SET_OPERATOR_BIT_OR 0x93
#define TOKEN_SET_OPERATOR_XOR 0xA3
#define TOKEN_SET_OPERATOR_LSH 0xB3
#define TOKEN_SET_OPERATOR_RSH 0xC3
#define TOKEN_SET_OPERATOR_MAT_MUL 0xD3

#define TOKEN_SYMBOL 0x4
#define TOKEN_SYMBOL_LPAREN 0x04
#define TOKEN_SYMBOL_RPAREN 0x14
#define TOKEN_SYMBOL_LBRACKET 0x24
#define TOKEN_SYMBOL_RBRACKET 0x34
#define TOKEN_SYMBOL_LBRACE 0x44
#define TOKEN_SYMBOL_RBRACE 0x54
#define TOKEN_SYMBOL_COLON 0x64
#define TOKEN_SYMBOL_COMMA 0x74
#define TOKEN_SYMBOL_DOT 0x84
#define TOKEN_SYMBOL_EXC 0x94

#define TOKEN_LINE_BREAK 0x5
#define TOKEN_LINE_BREAK_NEWLINE 0x05
#define TOKEN_LINE_BREAK_SEMICOLON 0x15

// ((x >> 4) & 0b111)   =>    0: '', 1: 'f', 2: 'r', 3: 'b', 4: 'rb', 5: 'rf'
// (x >> 7)             =>    57 bits free for the string value index
#define TOKEN_STRING 0x6
#define TOKEN_STRING_F  0x16
#define TOKEN_STRING_R  0x26
#define TOKEN_STRING_B  0x36
#define TOKEN_STRING_RB 0x46
#define TOKEN_STRING_RF 0x56

// ((x >> 4) & 1)  =>  either has 'r' or not
// x >> 5          =>  59 bits free for the string value index
#define TOKEN_FSTRING_START 0x7
#define TOKEN_FSTRING_START_R  0x17

// x >> 4   =>  60 bits free for the string value index
#define TOKEN_FSTRING_MIDDLE 0x8
#define TOKEN_FSTRING_END 0x9

// x >> 4   =>  60 bits free for the content substr index
#define TOKEN_IDENTIFIER 0xA

// x >> 4   =>  60 bits free for the integer/float value index
#define TOKEN_INTEGER 0xB
#define TOKEN_FLOAT 0xC

#define TOKEN_NONE 0xD

static const char *keyword_names[] = {
        "if", "else", "elif", "for", "while", "def", "return", "type", "import", "from", "as",
        "with", "try", "except", "finally", "raise", "pass", "break", "continue", "lambda",
        "yield", "global", "nonlocal", "assert", "del", "async", "await", "match", "case"
};
static const size_t keywords_count = sizeof(keyword_names) / sizeof(keyword_names[0]);

static const char *symbols[] = {
        "==",

        "=", "**=", "+=", "-=", "*=", "/=", "//=", "%=", "&=", "|=",
        "^=", "~=", "<<=", ">>=", "@=",

        "**", "+", "-", "*", "/", "//", "%", "&", "|", "^",
        "~", "<<", ">>", "@", "!=", ">=", "<=", ">", "<", ":=",

        "(", ")", "[", "]", "{", "}", ":", ",", ".", "!"
};

static const char *word_operators[] = {
        "in", "is", "not", "and", "or"
};
static const size_t word_operators_count = sizeof(word_operators) / sizeof(word_operators[0]);
#define WORD_OPERATOR_IN_INDEX 0
#define WORD_OPERATOR_NOT_INDEX 2

#define SYMBOLS_SET_OPERATOR 15
#define SYMBOLS_OPERATOR 35

static const size_t symbols_count = sizeof(symbols) / sizeof(symbols[0]);

static const char *token_type_str[] = {
        "keyword", "boolean", "operator", "set_operator", "symbol", "line_break",
        "identifier", "integer", "float", "string", "fstring_start", "fstring_middle",
        "fstring_end", "none"
};

typedef struct token {
    size_t type;
    size_t start;
    size_t end;
} token;

vec_define_pyc(token, tokens, token)

void token_p_free(token *obj);

vec_define(token*, tokens_p);

void token_p_print(token *obj, int indent);

vec_define_print(token*, tokens_p, token_p_print(a, indent));

typedef struct {
    char *code;
    size_t start;
    size_t end;
} code_substr;
vec_define(code_substr, code_substrs);

extern char *pyc_code;
extern size_t pyc_ci;
extern size_t pyc_code_len;
extern tokens pyc_tokens;
extern size_t pyc_tok_count;
extern size_t pyc_ti;
extern tokens pyc_temp_tokens;
extern code_substrs pyc_temp_identifiers;
extern code_substrs pyc_temp_strings;
extern doubles pyc_temp_doubles;
extern bigints pyc_temp_bigints;

void print_readable(const char *code, size_t start, size_t end);

void get_index_pos(size_t index, size_t *line, size_t *column);

void get_line_bound(size_t index, size_t *start, size_t *end);

void pyc_tokenize();

void pyc_lexer_init(char *code);

void pyc_load(char *code, tokens tokens);

#define p_tok_val_eq(t, target) str_ind_eq(t->start, t->end, target)
#define tok_val_eq(t, target) str_ind_eq(t.start, t.end, target)
#define tok_peek_eq(peek, target) tok_val_eq(token_peek(peek), target)
#define tok_peek_teq(peek, target) (token_peek(peek).type == target)
#define tok_now_teq(target) tok_peek_teq(0, target)

bool str_ind_eq(size_t start, size_t end, const char *target);

char *str_ind_dup(size_t start, size_t end);

char *str_tok_dup(token t);

#define syntax_error() raise_error("SyntaxError: invalid syntax")
#define syntax_error_t(t) raise_error_t((t), "SyntaxError: invalid syntax")
#define tab_error()                                                            \
    raise_error("TabError: inconsistent use of tabs and spaces in "            \
                "indentation")
#define indentation_error()                                                    \
    raise_error("IndentationError: expected an indented block after function " \
                "definition")
#define not_implemented() raise_error("NotImplementedError: not implemented")
#define raise_error_t(t, err) raise_error_t_ln(t, err, false)
#define raise_error_t_ln(t, err, ln) raise_error_i((t).start, err, ln)
#define raise_error(err) raise_error_ln(err, false)
#define raise_error_ln(err, ln)      raise_error_i(tokens_over() ? pyc_code_len - 1 : token_peek(0).start,   err, ln)
#define raise_error_n(n, err) raise_error_t(*get_node_token(n), err)

static inline void print_line(size_t i) {
    if (i >= pyc_code_len) i = pyc_code_len - 1;
    size_t line, column;
    get_index_pos(i, &line, &column);
    size_t start, end;
    get_line_bound(i, &start, &end);
    printf("%.*s", (int) (end - start), pyc_code + start);
    putchar('\n');
}

__THROWNL __attribute__((noreturn)) void raise_error_i(size_t i, const char *err, bool has_line);

#endif // PYC_LEXER_H
