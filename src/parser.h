#ifndef PYC_PARSER_H
#define PYC_PARSER_H

#include "lexer.h"
#include "utils.h"

typedef struct node node;

vec_define_pyc(node, nodes, node *)

typedef struct comprehension {
    int is_async;
    node *target;
    node *iter;
    nodes ifs;
} comprehension;

vec_define_pyc(comprehension, comprehensions, comprehension)

typedef struct keyword {
    token *name;
    node *value;
} keyword;

vec_define_pyc(keyword, keywords, keyword)

typedef struct import_lib {
    tokens_p tok;
    token *as;
} import_lib;

vec_define_pyc(import_lib, import_libs, import_lib)

typedef struct import_alias {
    token *tok;
    token *as;
} import_alias;

vec_define_pyc(import_alias, import_aliases, import_alias)

typedef struct except_handler {
    node *class;
    token *name;
    node *body;
} except_handler;

vec_define_pyc(except_handler, except_handlers, except_handler)

typedef struct match_case {
    node *pattern;
    token *as;
    node *condition;
    node *body;
} match_case;

vec_define_pyc(match_case, match_cases, match_case)

typedef enum {
    EXPR_CTX_LOAD,
    EXPR_CTX_STORE,
    EXPR_CTX_DEL
} expr_context;

typedef enum {
    NODE_MODULE,
    NODE_GROUP, // ( EXPR ) | STMT...

    EXPR_IDENTIFIER,    // IDENTIFIER
    EXPR_CONSTANT,      // INT | FLOAT | STRING | BOOL | NONE
    EXPR_FORMAT_STRING, // f"text {some content} text etc."
    EXPR_OPERATOR,      // SYMBOL(filter=operator)
    EXPR_BINARY_OPERATION, // EXPR BINARY_OPERATOR EXPR
    EXPR_UNARY_OPERATION, // UNARY_OPERATOR EXPR
    EXPR_CMP_OPERATION, // EXPR (CMP_OPERATOR EXPR)...
    EXPR_WALRUS_OPERATION, // EXPR := EXPR

    EXPR_LIST_COMP, // [EXPR_GENERATOR]
    EXPR_LIST,      // [EXPR]
    EXPR_DICT_COMP, // {EXPR : EXPR_GENERATOR}
    EXPR_DICT,      // {EXPR : EXPR ( , EXPR : EXPR )... }
    EXPR_SET_COMP,  // {EXPR_GENERATOR}
    EXPR_SET,       // {EXPR}
    EXPR_TUPLE,     // EXPR ( , EXPR )...

    EXPR_LAMBDA, // lambda IDENTIFIER ( , IDENTIFIER )... : EXPR
    EXPR_IF_EXP, // EXPR if EXPR else EXPR

    EXPR_GENERATOR, // EXPR for EXPR in EXPR (if EXPR | for EXPR in EXPR)...

    EXPR_AWAIT, // await EXPR

    EXPR_YIELD,      // yield EXPR
    EXPR_YIELD_FROM, // yield from EXPR

    EXPR_CALL, // EXPR ( EXPR )

    EXPR_ATTRIBUTE, // EXPR . IDENTIFIER
    EXPR_INDEX, // EXPR [ EXPR ] | EXPR [ EXPR : EXPR ]   |   EXPR [ : EXPR ]  | EXPR [ EXPR : ] etc.

    STMT_FUNCTION_DEF, // (async)? def IDENTIFIER ( IDENTIFIER ( : EXPR )? ( , IDENTIFIER ( : EXPR )? )... ) ( - > EXPR )? : BODY
    STMT_CLASS_DEF, // type IDENTIFIER ( ( ( ( IDENTIFIER | EXPR ) ( , IDENTIFIER | EXPR )... ) )? : CLASS_BODY
    STMT_RETURN,    // return EXPR
    STMT_DELETE,    // del EXPR
    STMT_ASSIGN,    // EXPR SET_OPERATOR EXPR
    STMT_ASSIGN_MULT,    // (EXPR =)... EXPR
    STMT_FOR,       // for EXPR in EXPR : BODY
    STMT_WHILE,     // while EXPR : BODY
    STMT_IF,        // if EXPR : BODY ( elif EXPR : BODY )... ( else : BODY )?
    STMT_WITH,      // with EXPR ( as IDENTIFIER )? : BODY
    STMT_MATCH,     // match EXPR : MATCH_BODY
    STMT_RAISE,     // raise ( EXPR | from EXPR )?
    STMT_TRY_CATCH, // try : BODY ( except (*)? ( EXPR ( as IDENTIFIER )? )? : BODY )... ( finally : BODY )? ( else : BODY )?
    STMT_ASSERT,      // assert EXPR
    STMT_IMPORT,      // import ( IDENTIFIER ( as IDENTIFIER ) ),...
    STMT_IMPORT_FROM, // from IDENTIFIER ( import | from ) ( IDENTIFIER ( as  IDENTIFIER ) ),...
    STMT_GLOBAL,   // global IDENTIFIER ( , IDENTIFIER )...
    STMT_NONLOCAL, // nonlocal IDENTIFIER ( , IDENTIFIER )...
    STMT_EXPR,     // EXPR
    STMT_PASS,     // pass
    STMT_BREAK,    // break
    STMT_CONTINUE // continue
} node_type;

struct node {
    node *parent;
    node_type type;
    union {
        struct {
            char *filename;
            char *code;
            tokens tokens;
            size_t code_len;
        } module;
        token *identifier;
        token *constant;
        struct {
            token *start_string;
            nodes values;
            nodes extras;
            tokens_p strings;
        } fstring;
        token *operator;
        struct {
            node *left;
            token *op;
            node *right;
        } bin_op;
        struct {
            token *op;
            node *expr;
        } unary_op;
        struct {
            nodes ex;
            tokens_p op;
        } cmp_op;
        struct {
            node *left;
            node *right;
        } walrus_op;
        struct {
            token *tok;
            nodes v;
        } group;
        struct {
            token *tok;
            nodes v;
        } tuple;
        struct {
            token *tok;
            nodes v;
        } set;
        struct {
            token *tok;
            nodes v;
        } list;
        struct {
            token *tok;
            nodes keys;
            nodes values;
        } dict;
        struct {
            token *tok;
            node *value;
            comprehensions comp;
        } list_comp;
        struct {
            token *tok;
            node *value;
            comprehensions comp;
        } set_comp;
        struct {
            token *tok;
            node *key;
            node *value;
            comprehensions comp;
        } dict_comp;
        struct {
            token *tok;
            tokens_p args;
            node *body;
        } lambda;
        struct {
            node *if_expr;
            node *else_expr;
            node *condition;
        } if_else_expr;
        struct {
            node *value;
            comprehensions comp;
        } generator;
        node *await;
        node *yield;
        node *yield_from;
        struct {
            node *base;
            nodes args;
            keywords kws;
        } call;
        struct {
            node *base;
            token *key;
        } attribute;
        struct {
            node *base;
            node *slices[3];
        } index;
        node *stmt_expr;
        struct {
            bool is_async;
            token *name;

            tokens_p args;
            nodes args_defaults;
            token *ls_args;
            token *kw_args;

            node *body;
            nodes decorators;
        } function_def;
        struct {
            token *tok;
            node *v;
        } ret;
        node *del;
        struct {
            node *var;
            token *set_op;
            node *val;
        } assign;
        struct {
            nodes targets;
            node *val;
        } assign_mult;
        struct {
            token *name;
            tokens_p extends;
            nodes methods;
            nodes properties;
            nodes decorators;
        } class_def;
        struct {
            token *tok;
            bool is_async;
            node *value;
            node *iter;
            node *body;
            node *else_body;
        } for_loop;
        struct {
            token *tok;
            node *cond;
            node *body;
        } while_loop;
        struct {
            token *tok;
            nodes contexts;
            tokens_p vars;
            node *body;
            bool is_async;
        } with;
        struct {
            token *tok;
            nodes conditions;
            nodes bodies;
        } if_stmt;
        struct {
            token *tok;
            import_libs v;
        } imports;
        struct {
            token *tok;
            tokens_p lib;
            import_aliases imports;
            int level;
        } import_from;
        tokens_p global;
        tokens_p nonlocal;
        struct {
            token *tok;
            node *condition;
            node *message;
        } assert;
        struct {
            token *tok;
            node *exception;
            node *cause;
        } raise;
        node *decorator;
        struct {
            token *tok;
            node *subject;
            match_cases cases;
        } match;
        struct {
            token *tok;
            bool is_star;
            node *try_body;
            except_handlers handlers;
            node *else_body;
            node *finally_body;
        } try_catch;
        token *pass;
        token *break_st;
        token *continue_st;
    };
};

static inline node *ast_node_create(node *parent) {
    node *ex = (node *) malloc(sizeof(node));
    if (ex == NULL) {
        perror("malloc failed");
        fail();
    }

    ex->parent = parent;
    return ex;
}

#define token_peek(i) (pyc_tokens.data[pyc_ti + i])
#define char_peek(i) (pyc_code[token_peek(i).start])
#define str_peek(i) (pyc_code + token_peek(i).start)
#define tokens_over() (pyc_ti >= pyc_tok_count)
#define token_type_peek_t(i) (pyc_tokens.data[pyc_ti + i].type & 0xf)
#define assert_token(ty)                                                       \
    do {                                                                       \
        if (tokens_over() || token_peek(0).type != ty) {                       \
            raise_error("expected ");                                          \
        }                                                                      \
    } while (0)
#define assert_tokenP(ty)                                                      \
    do {                                                                       \
        if (tokens_over() || token_peek(0).type != ty) {                       \
            raise_error("expected ");                                          \
        }                                                                      \
        pyc_ti++;                                                              \
    } while (0)
#define assert_token_type_t(type_)                                             \
    do {                                                                       \
        if (tokens_over() || token_type_peek_t(0) != type_) {                  \
            raise_error("invalid syntax");                                     \
        }                                                                      \
    } while (0)

node *parse_file(node *parent, char *filename, char *code);

size_t count_indent(size_t index);

void parse_expression_group(node *ex);

node *parse_expression_group_child(node *parent);

void parse_expression_next(node *node);

void parse_expression_tuple(node *ex);

void parse_expression_list(node *ex);

void parse_expression_dict(node *node);

void parse_expression_lambda(node *ex);

void parse_expression_await(node *ex);

void parse_expression_yield(node *ex);

void parse_expression(node *ex);

void parse_comprehensions_post(node *parent, comprehensions *list);

void parse_statements(node *st);

void parse_statement_next(node *st, int spaces);

void parse_statement_group(node *st, int spaces_parent);

token *get_node_token(const node *node);

#endif // PYC_PARSER_H