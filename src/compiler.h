#ifndef PYC_COMPILER_H
#define PYC_COMPILER_H

#include "utils.h"
#include "parser.h"

typedef enum {
    VARIABLE_CLASS_UNDEFINED,
    VARIABLE_CLASS_INT,
    VARIABLE_CLASS_FLOAT,
    VARIABLE_CLASS_STRING,
    VARIABLE_CLASS_BOOLEAN,
    VARIABLE_CLASS_NONE,
    VARIABLE_CLASS_LIST,
    VARIABLE_CLASS_SET,
    VARIABLE_CLASS_TUPLE,
    VARIABLE_CLASS_DICT,
    VARIABLE_CLASS_FUNCTION,
    VARIABLE_CLASS_CLASS,
    VARIABLE_CLASS_AOT_INT,
    VARIABLE_CLASS_AOT_FLOAT,
    VARIABLE_CLASS_AOT_STRING,
    VARIABLE_CLASS_AOT_BOOLEAN,
    VARIABLE_CLASS_GROUP
} variable_class;

static const char *variable_class_to_str[] = {
        "int", "float", "string", "bool", "none", "list", "set", "tuple", "dict",
        "function", "type", "aot_int", "aot_float", "aot_string", "aot_bool", "group"
};

typedef struct variable_type variable_type;

vec_define_pyc(variable_type, variable_types, variable_type*)

struct variable_type {
    variable_class type;
    union {
        mpz_t aot_int;
        double aot_float;
        code_substr aot_string;
        bool aot_bool;
        variable_types group;
        struct {
            variable_type *value;
            variable_types values;
            size_t fixed_size; // set to -1 if not fixed
        } list;
        struct {
            void *value;
            variable_types values;
            size_t fixed_size; // set to -1 if not fixed
        } set;
        struct {
            variable_type *value;
            variable_types values;
            size_t fixed_size; // set to -1 if not fixed
        } tuple;
        struct {
            variable_type *key;
            variable_type *value;
            variable_types keys;
            variable_types values;
            size_t fixed_size; // set to -1 if not fixed
        } dict;
        node *function;
        node *class;
    };
};

typedef struct {
    variable_type *type;
    sstream def;
} variable_type_holder;

vec_define_pyc(variable_type_holder, variable_type_holders, variable_type_holder)

bool variable_type_equals(const variable_type *a, const variable_type *b);

bool variable_types_equals(variable_types a, variable_types b);

typedef struct scope scope;

typedef struct {
    scope *scope;
    token *tok;
    size_t index;
    size_t name;
    variable_type *type;
} variable;

vec_define_pyc(variable, variables, variable)

vec_define_pyc(variable_p, variables_p, variable*)

variable_type *variable_type_copy(const variable_type *var);

void variable_types_copy(variable_types var, variable_types *out);

static inline variable_type *variable_type_create(variable_class class) {
    variable_type *var = (variable_type *) malloc(sizeof(variable_type));
    var->type = class;
    return var;
}

struct scope {
    scope *parent;
    sstream *content;
    size_t name;
    size_t indent;
    variables vars;
};

#define scope_append(a, b) scope_append_l(a, b, strlen(b))

void scope_append_l(const scope *sc, const char *b, size_t b_len);

void scope_appendf(const scope *s, const char *fmt, ...);

vec_define_pyc(scope, scopes, scope)

variable *get_variable(const scope *n, size_t v);

size_t set_variable(scope *n, size_t v, variable_type *type);

void pyc_compile(char *in_file, const char *out_file);

#endif //PYC_COMPILER_H
