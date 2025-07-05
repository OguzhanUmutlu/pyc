#include "lexer.h"

char *pyc_code;
size_t pyc_ci;
size_t pyc_code_len;
tokens pyc_tokens;
size_t pyc_tok_count;
size_t pyc_ti;
code_substrs pyc_temp_identifiers = {.data = NULL, .size = 0, .capacity = 0};
doubles pyc_temp_doubles = {.data = NULL, .size = 0, .capacity = 0};
code_substrs pyc_temp_strings = {.data = NULL, .size = 0, .capacity = 0};
bigints pyc_temp_bigints = {.data = NULL, .size = 0, .capacity = 0};

void pyc_lexer_init(char *code) {
    pyc_code = code;
    pyc_ci = 0;
    pyc_code_len = strlen(code);
    pyc_ti = 0;
    tokens_init(&pyc_tokens);
}

size_t temp_identifiers_introduce(size_t start, size_t end) {
    size_t len = end - start;
    for (size_t i = 0; i < pyc_temp_identifiers.size; i++) {
        code_substr ex = pyc_temp_identifiers.data[i];
        if (ex.start == start) return i;
        if (end - start == ex.end - ex.start) {
            char *p1 = pyc_code + start;
            char *p2 = ex.code + ex.start;
            for (size_t j = 0; j < len; j++) {
                if (*p1++ != *p2++) break;
                if (j == len - 1) return i;
            }
        }
    }

    code_substrs_push(&pyc_temp_identifiers, (code_substr) {.code=pyc_code, .start=start, .end=end});
    return pyc_temp_identifiers.size - 1;
}


size_t temp_bigints_introduce(mpz_t x) {
    for (size_t i = 0; i < pyc_temp_bigints.size; i++) {
        if (mpz_cmp(pyc_temp_bigints.data[i], x) == 0) return i;
    }

    bigints_reserve_push(&pyc_temp_bigints);
    mpz_init_set(pyc_temp_bigints.data[pyc_temp_bigints.size++], x);
    return pyc_temp_bigints.size - 1;
}

size_t temp_doubles_introduce(double x) {
    for (size_t i = 0; i < pyc_temp_doubles.size; i++) {
        if (pyc_temp_doubles.data[i] == x) return i;
    }

    doubles_push(&pyc_temp_doubles, x);
    return pyc_temp_doubles.size - 1;
}

void pyc_load(char *code, tokens tokens) {
    pyc_code = code;
    pyc_code_len = strlen(code);
    pyc_ti = 0;
    pyc_tokens = tokens;
}

bool str_ind_eq(size_t start, size_t end, const char *target) {
    size_t len = end - start;
    if (len != strlen(target)) return 0;
    return strncmp(pyc_code + start, target, len) == 0;
}

char *str_ind_dup(size_t start, size_t end) {
    return strndup(pyc_code + start, end - start);
}

char *str_tok_dup(token t) {
    return strndup(pyc_code + t.start, t.end - t.start);
}

size_t match_symbol(size_t i, size_t len, char **matched) {
    for (size_t s = 0; s < symbols_count; s++) {
        size_t sLen = strlen(symbols[s]);
        if (i + sLen <= len && str_ind_eq(i, i + sLen, symbols[s])) {
            *matched = (char *) symbols[s];
            return s;
        }
    }
    return -1;
}

typedef struct {
    bool is_triple;
    char quote;
    token tok;
    int open;
} fstring;

vec_define_pyc(fstring, fstrings, fstring)

inline void fstring_free(fstring t) {}

void pyc_feed() { // TODO: re-implement the tokenizer to allow live-streaming data
}

#include <gmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static inline bool extract_number(size_t start, size_t end, bool is_float, double *out_float, mpz_t out_int) {
    size_t len = end - start;
    char *buf = (char *) malloc(len + 1);
    if (!buf) return false;
    memcpy(buf, pyc_code + start, len);
    buf[len] = '\0';

    size_t offset = 0;
    for (size_t i = 0; i < len; i++) {
        if (buf[i] == '_') {
            offset++;
            continue;
        }

        buf[i - offset] = buf[i];
    }

    buf[len - offset] = '\0';

    int base = 10;
    if (len > 2 && buf[0] == '0') {
        if (buf[1] == 'x' || buf[1] == 'X') base = 16;
        else if (buf[1] == 'o' || buf[1] == 'O') base = 8;
        else if (buf[1] == 'b' || buf[1] == 'B') base = 2;
    }

    if (is_float) {
        if (base != 10) {
            raise_error_i(start, "SyntaxError: invalid float literal with base other than 10", false);
        }
        char *end_ptr;
        *out_float = strtod(buf, &end_ptr);
        bool success = (*end_ptr == '\0');
        free(buf);
        return success;
    }

    int err = mpz_set_str(out_int, base == 10 ? buf : buf + 2, base);
    free(buf);
    return err == 0;
}

void pyc_tokenize() {
    fstrings fstrings;
    fstrings_init(&fstrings);

    while (pyc_ci < pyc_code_len) {
        char c = pyc_code[pyc_ci];

        if (c == '\r') {
            pyc_ci++;
            continue;
        }

        if (c == '#') {
            while (pyc_ci < pyc_code_len && pyc_code[pyc_ci] != '\n') {
                pyc_ci++;
            }

            continue;
        }

        if (c == '\\') {
            pyc_ci++;
            char c2;
            while (pyc_ci < pyc_code_len && (c2 = pyc_code[pyc_ci]) == '\r') pyc_ci++;
            if (pyc_ci >= pyc_code_len || c2 != '\n') {
                raise_error_i(pyc_ci, "SyntaxError: invalid syntax", false);
            }

            pyc_ci++;
            continue;
        }

        if (c == '\n' || c == ';') {
            if (pyc_tokens.size == 0) {
                pyc_ci++;
                continue;
            }
            token tok;
            tok.start = pyc_ci;
            tok.end = pyc_ci + 1;
            tok.type = c == '\n' ? TOKEN_LINE_BREAK_NEWLINE : TOKEN_LINE_BREAK_SEMICOLON;
            tokens_push(&pyc_tokens, tok);
            pyc_ci++;
            continue;
        }

        if (c == ' ' || c == '\t') {
            pyc_ci++;
            continue;
        }

        if (fstrings.size > 0) {
            if (c == '{') fstrings_back(fstrings)->open++;

            if (c == '}') {
                fstring *opt = fstrings_back(fstrings);
                if (--opt->open == 0) {
                    size_t start = pyc_ci;
                    char quote = opt->quote;
                    bool has_format = false;
                    bool is_triple = opt->is_triple;
                    bool slash = false;

                    while (pyc_ci < pyc_code_len) {
                        c = pyc_code[pyc_ci];
                        if (c == '{') {
                            if (pyc_ci + 1 < pyc_code_len && pyc_code[pyc_ci + 1] == '{') {
                                pyc_ci += 2;
                                continue;
                            } else {
                                opt->open++;
                                has_format = true;
                                pyc_ci++;
                                break;
                            }
                        }

                        if (c == '\r') {
                            pyc_ci++;
                            continue;
                        }

                        if (!slash && !is_triple && c == '\n') {
                            raise_error_i(pyc_ci, "SyntaxError: invalid syntax", false);
                        }

                        if (c == '\\') slash = !slash;
                        else if (!slash && c == quote) {
                            if (is_triple) {
                                if (pyc_ci + 2 < pyc_code_len && pyc_code[pyc_ci + 1] == quote &&
                                    pyc_code[pyc_ci + 2] == quote) {
                                    pyc_ci += 3;
                                    break;
                                }
                            } else {
                                pyc_ci++;
                                break;
                            }
                        } else slash = false;

                        pyc_ci++;
                    }

                    token tok;
                    tok.start = start;
                    tok.end = pyc_ci - (has_format ? 1 : is_triple ? 3 : 1);
                    tok.type = has_format ? TOKEN_FSTRING_MIDDLE : TOKEN_FSTRING_END;
                    if (!has_format) fstrings_pop(&fstrings);
                    tokens_push(&pyc_tokens, tok);
                    continue;
                }
            }
        }

        char *matched = NULL;
        size_t symbol_ind = match_symbol(pyc_ci, pyc_code_len, &matched);
        if (symbol_ind != -1 && (c != '.' || pyc_ci + 1 >= pyc_code_len || !isdigit(pyc_code[pyc_ci + 1]))) {
            size_t match_len = strlen(matched);

            token tok;
            tok.start = pyc_ci;
            tok.end = pyc_ci += match_len;
            tok.type = symbol_ind == 0
                       ? TOKEN_OPERATOR_EQ
                       : (symbol_ind <= SYMBOLS_SET_OPERATOR
                          ? (TOKEN_SET_OPERATOR | ((symbol_ind - 1) << 4))
                          : (symbol_ind <= SYMBOLS_OPERATOR
                             ? (TOKEN_OPERATOR | ((symbol_ind - SYMBOLS_SET_OPERATOR) << 4))
                             : (TOKEN_SYMBOL | ((symbol_ind - SYMBOLS_OPERATOR - 1) << 4))));
            tokens_push(&pyc_tokens, tok);
            continue;
        }

        if (isalpha(c) || c == '_') {
            token tok;
            tok.start = pyc_ci;

            while (pyc_ci < pyc_code_len && (isalnum(pyc_code[pyc_ci]) || pyc_code[pyc_ci] == '_')) {
                pyc_ci++;
            }
            size_t len = pyc_ci - tok.start;

            char *word = strndup(pyc_code + tok.start, len);

            tok.end = pyc_ci;

            bool is_true = strcmp(word, "True") == 0;
            if (is_true || strcmp(word, "False") == 0) {
                tok.type = is_true ? TOKEN_BOOLEAN_TRUE : TOKEN_BOOLEAN_FALSE;
            } else if (strcmp(word, "None") == 0) {
                tok.type = TOKEN_NONE;
            } else {
                int j = 0;
                for (; j < keywords_count; j++) {
                    if (strcmp(word, keyword_names[j]) == 0) break;
                }

                if (j != keywords_count) tok.type = TOKEN_KEYWORD | ((j + 1) << 4);
                else {
                    j = 0;
                    for (; j < word_operators_count; j++) {
                        if (strcmp(word, word_operators[j]) == 0) break;
                    }

                    if (j != word_operators_count) {
                        if ((j == WORD_OPERATOR_IN_INDEX || j == WORD_OPERATOR_NOT_INDEX) && pyc_tokens.size > 0) {
                            token *prev = tokens_back(pyc_tokens);

                            if (j == WORD_OPERATOR_IN_INDEX && p_tok_val_eq(prev, "not")) {
                                prev->type = TOKEN_OPERATOR_NOT_IN;
                                prev->end = pyc_ci;
                                free(word);
                                continue;
                            } else if (j == WORD_OPERATOR_NOT_INDEX && p_tok_val_eq(prev, "is")) {
                                prev->type = TOKEN_OPERATOR_IS_NOT;
                                prev->end = pyc_ci;
                                free(word);
                                continue;
                            }
                        }

                        tok.type = TOKEN_OPERATOR | ((j + 0x14) << 4);
                    } else {
                        tok.type = TOKEN_IDENTIFIER | (temp_identifiers_introduce(tok.start, tok.end) << 4);
                    }
                }
            }

            free(word);

            tokens_push(&pyc_tokens, tok);
            continue;
        }

        if (c == '.' || isdigit(c)) {
            token tok;
            tok.start = pyc_ci;
            bool has_dot = c == '.';
            bool has_exponent = false;
            char cn;

            if (has_dot) pyc_ci++;
            else if (c == '0') {
                if (pyc_ci + 1 < pyc_code_len && (
                        pyc_code[pyc_ci + 1] == 'x'
                        || pyc_code[pyc_ci + 1] == 'X'
                        || pyc_code[pyc_ci + 1] == 'o'
                        || pyc_code[pyc_ci + 1] == 'O'
                        || pyc_code[pyc_ci + 1] == 'b'
                        || pyc_code[pyc_ci + 1] == 'B'
                )) {
                    pyc_ci += 2;
                } else {
                    raise_error_i(pyc_ci, "SyntaxError: leading zeros in decimal integer literals "
                                          "are not permitted; use an 0o prefix for octal integers", false);
                }
            }

            while (pyc_ci < pyc_code_len && (isdigit(cn = pyc_code[pyc_ci]) || cn == '.' || cn == '_')) {
                if (cn == '_' && pyc_code[pyc_ci - 1] == '_') {
                    raise_error_i(pyc_ci, "invalid decimal literal", false);
                }

                if (cn == '.') {
                    if (has_dot) break;
                    if (pyc_code[pyc_ci - 1] == '_' || (pyc_ci + 1 < pyc_code_len && pyc_code[pyc_ci + 1] == '_')) {
                        raise_error_i(pyc_ci, "invalid decimal literal", false);
                    }
                    has_dot = true;
                }

                pyc_ci++;
            }

            if (pyc_ci < pyc_code_len && ((cn = pyc_code[pyc_ci]) == 'e' || cn == 'E')) {
                has_exponent = true;
                pyc_ci++;
                cn = pyc_code[pyc_ci];
                if (pyc_ci < pyc_code_len && (cn == '+' || cn == '-')) {
                    pyc_ci++;
                }

                if (pyc_ci >= pyc_code_len || !isdigit(pyc_code[pyc_ci])) {
                    raise_error_i(pyc_ci, "invalid decimal literal",
                                  false); // todo: python throws the slices into weird places
                }

                int exponent = 0;
                while (pyc_ci < pyc_code_len && (isdigit(cn = pyc_code[pyc_ci]) || cn == '_')) {
                    if (cn == '_' && pyc_code[pyc_ci - 1] == '_') {
                        raise_error_i(pyc_ci, "invalid decimal literal", false);
                    }

                    if (cn != '_') exponent = exponent * 10 + (cn - '0');
                    pyc_ci++;
                }
            }

            tok.end = pyc_ci;
            mpz_t mpz_val;
            mpz_init(mpz_val);
            double float_val;
            bool is_float = has_dot || has_exponent;

            if (!extract_number(tok.start, tok.end, is_float, &float_val, mpz_val)) {
                raise_error_i(tok.start, "invalid number literal", false);
            }

            if (pyc_code[pyc_ci - 1] == '_') {
                raise_error_i(pyc_ci - 1, "invalid number literal", false);
            }

            if (is_float) {
                tok.type = TOKEN_FLOAT | (temp_doubles_introduce(float_val) << 4);
                mpz_clear(mpz_val);
            } else {
                tok.type = TOKEN_INTEGER | (temp_bigints_introduce(mpz_val) << 4);
            }

            tokens_push(&pyc_tokens, tok);
            continue;
        }

        if (c == '\'' || c == '"') {
            char quote = c;

            token tok;
            size_t start = tok.start = pyc_ci;

            bool is_triple = (pyc_ci + 2 < pyc_code_len && pyc_code[pyc_ci + 1] == quote &&
                              pyc_code[pyc_ci + 2] == quote);
            pyc_ci += is_triple ? 3 : 1;

            bool is_fstring = false;
            bool is_raw = false;
            bool is_binary = false;
            bool slash = false;
            bool has_format = false;
            int flag = 0;

            token back;
            if (pyc_tokens.size > 0 && (back = *tokens_back(pyc_tokens)).type == TOKEN_IDENTIFIER &&
                back.end == start) {
                if (tok_val_eq(back, "f")) {
                    flag = 0x10;
                    is_binary = true;
                    is_raw = true;
                } else if (tok_val_eq(back, "r")) {
                    flag = 0x50;
                    is_raw = true;
                } else if (tok_val_eq(back, "b")) {
                    flag = 0x50;
                    is_binary = true;
                } else if (tok_val_eq(back, "rf") || tok_val_eq(back, "fr")) {
                    flag = 0x50;
                    is_fstring = true;
                    is_raw = true;
                }

                if (is_fstring || is_raw || is_binary) {
                    tok.start = back.start;
                    tokens_pop(&pyc_tokens);
                }
            }

            while (pyc_ci < pyc_code_len) {
                c = pyc_code[pyc_ci];
                if (is_fstring && c == '{') {
                    if (pyc_ci + 1 < pyc_code_len && pyc_code[pyc_ci + 1] == '{') {
                        pyc_ci += 2;
                        continue;
                    } else {
                        has_format = true;
                        pyc_ci++;
                        break;
                    }
                }

                if (c == '\r') {
                    pyc_ci++;
                    continue;
                }

                if (!slash && !is_triple && c == '\n') {
                    raise_error_i(pyc_ci, "SyntaxError: invalid syntax", true);
                }

                if (c == '\\') slash = !slash;
                else if (!slash && c == quote) {
                    if (is_triple) {
                        if (pyc_ci + 2 < pyc_code_len && pyc_code[pyc_ci + 1] == quote &&
                            pyc_code[pyc_ci + 2] == quote) {
                            pyc_ci += 3;
                            break;
                        }
                    } else {
                        pyc_ci++;
                        break;
                    }
                } else slash = false;

                pyc_ci++;
            }

            tok.end = pyc_ci - 1;
            if (has_format) {
                tok.type = TOKEN_FSTRING_START | flag;
                fstring opt = {.is_triple = is_triple, .quote = quote, .tok = tok, .open = 1};
                fstrings_push(&fstrings, opt);
            } else {
                tok.type = TOKEN_STRING | flag;
            }

            tokens_push(&pyc_tokens, tok);
            continue;
        }

        raise_error_i(pyc_ci, "invalid syntax", false);
    }

    if (fstrings.size > 0) {
        fstring optBack = fstrings_pop(&fstrings);
        raise_error_t(optBack.tok, "SyntaxError: unterminated string literal");
    }

    fstrings_clear(&fstrings);

    tokens_shrink(&pyc_tokens);
    pyc_tok_count = pyc_tokens.size;
}

void get_index_pos(size_t index, size_t *l, size_t *c) {
    size_t line = 0;
    size_t column = 0;
    for (size_t i = 0; i < index; i++) {
        char ch = pyc_code[i];
        if (ch == '\n') {
            line++;
            column = 0;
        } else if (ch == '\r') {
            column = 0;
        } else column++;
    }

    *l = line;
    *c = column;
}

void get_line_bound(size_t index, size_t *s, size_t *e) {
    size_t start = 0;
    size_t st_index = index == 0 ? 0 : index - 1;
    if (pyc_code[index] == '\n' || pyc_code[index] == '\r') {
        char c2;
        while (st_index > 0 && ((c2 = pyc_code[st_index]) == '\n' || c2 == '\r')) st_index--;
    }

    for (size_t i = st_index; i-- > 0;) {
        char c = pyc_code[i];
        if (c == '\n' || c == '\r') {
            start = i + 1;
            *s = start;
            break;
        }
    }

    if (start == 0) *s = 0;

    for (size_t i = index; i < pyc_code_len; i++) {
        char c = pyc_code[i];
        if (c == '\n' || c == '\r') {
            *e = i;
            return;
        }
    }

    *e = pyc_code_len;
}

inline void token_free(token _) {}

inline void token_p_free(token *_) {}

#define TOKEN_ONLY_TEXT
//#define TOKEN_PRINTS_POSITION

#ifdef NEED_AST_PRINT

void print_readable(const char *code, size_t start, size_t end) {
    for (size_t i = start; i < end; i++) {
        char c = code[i];
        if (isprint(c)) {
            putchar(c);
        } else if (c == '\n') {
            printf("\\n");
        } else {
            printf("\\x%02x", c);
        }
    }
}

void token_print(token tok, int indent) {
    size_t line, column;
    get_index_pos(tok.start, &line, &column);
#ifdef TOKEN_ONLY_TEXT
    putchar('"');
    print_readable(pyc_code, tok.start, tok.end);
    putchar('"');
#else
#ifdef TOKEN_PRINTS_POSITION
    printf("token(text=\"");
    print_readable(pyc_code, tok.start, tok.end);
    printf("\", type=%s, id=%d, line=%d, column=%d)", token_type_str[tok.type & 0xf], tok.type, line + 1, column + 1);
#else
    printf("token(text=\"%s\", type=%s)", val, token_type_str[tok.type & 0xf]);
#endif
#endif
}

void token_p_print(token *tok, int indent) {
    if (tok == NULL) {
        printf("NULL");
        return;
    }

    token_print(*tok, indent);
}

#endif

__THROWNL __attribute__((noreturn)) void raise_error_i(size_t i, const char *err, bool has_line) {
    if (i >= pyc_code_len) i = pyc_code_len == 0 ? 0 : pyc_code_len - 1;
    size_t line, column;
    get_index_pos(i, &line, &column);
    size_t start, end;
    get_line_bound(i, &start, &end);
    printf("%.*s", (int) (end - start), pyc_code + start);
    putchar('\n');

    print_spaces(column);
    printf("^\n");

    if (has_line) {
        printf("%s on line %ld\n", err, line + 1);
    } else {
        printf("%s\n", err);
    }

    fail();
}
