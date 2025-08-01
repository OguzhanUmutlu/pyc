#ifndef PYC_COMPILER_H
#define PYC_COMPILER_H

extern "C" {
#include "utils.h"
#include "parser.h"
}

typedef enum {
    VARIABLE_CLASS_UNDEFINED,
    VARIABLE_CLASS_U8,
    VARIABLE_CLASS_U16,
    VARIABLE_CLASS_U32,
    VARIABLE_CLASS_U64,
    VARIABLE_CLASS_I8,
    VARIABLE_CLASS_I16,
    VARIABLE_CLASS_I32,
    VARIABLE_CLASS_I64,
    VARIABLE_CLASS_INT, // word size
    VARIABLE_CLASS_FLOAT,
    VARIABLE_CLASS_DOUBLE,
    VARIABLE_CLASS_BOOLEAN,
    VARIABLE_CLASS_LIST,
    VARIABLE_CLASS_SET,
    VARIABLE_CLASS_TUPLE,
    VARIABLE_CLASS_DICT,
    VARIABLE_CLASS_FUNCTION,
    VARIABLE_CLASS_CLASS,
    VARIABLE_CLASS_AOT_INT,
    VARIABLE_CLASS_AOT_FLOAT,
    VARIABLE_CLASS_AOT_STRING,
    VARIABLE_CLASS_AOT_BOOLEAN
} variable_class;

static char* variable_class_to_str[] = {
    "uint8", "uint16", "uint32", "uint64", "int8", "int16", "int32", "int64",
    "int", "float", "str", "bool", "list", "set", "tuple", "dict",
    "function", "type", "Int", "Float", "String", "Bool"
};

typedef struct variable_type variable_type;

vec_define_pyc(variable_type, variable_types, variable_type*)

struct variable_type {
    variable_class type;

    union {
        long aot_int;
        double aot_float;
        code_substr aot_string;
        bool aot_bool;

        struct {
            variable_type* values;
            size_t fixed_size; // set to -1 if not fixed
        } list;

        struct {
            variable_type* values;
            size_t fixed_size; // set to -1 if not fixed
        } set;

        struct {
            variable_types values;
            size_t fixed_size;
        } tuple;

        struct {
            variable_type* key;
            variable_type* value;
            size_t fixed_size; // set to -1 if not fixed
        } dict;

        node* function;
        node* class_;
    };
};

typedef struct {
    variable_type* type;
    sstream def;
} variable_type_holder;

vec_define_pyc(variable_type_holder, variable_type_holders, variable_type_holder)

bool variable_type_equals(variable_type* a, variable_type* b);

bool variable_types_equals(variable_types a, variable_types b);

typedef struct scope scope;

typedef struct {
    scope* scope;
    token* tok;
    size_t index;
    size_t name;
    variable_type* type;
} variable;

vec_define_pyc(variable, variables, variable)

vec_define_pyc(variable_p, variables_p, variable*)

variable_type* variable_type_copy(variable_type* var);

void variable_types_copy(variable_types var, variable_types* out);

static inline variable_type* variable_type_create(variable_classclass) {
    variable_type* var = (variable_type*)malloc(sizeof(variable_type));
    var->type =
    class;
    return var;
}

struct scope {
    scope* parent;
    sstream* content;
    size_t name;
    size_t indent;
    variables vars;
};

#define scope_append(a, b) scope_append_l(a, b, strlen(b))

void scope_append_l(scope* sc, char* b, size_t b_len);

void scope_appendf(scope* s, char* fmt, ...);

vec_define_pyc(scope, scopes, scope)

variable* get_variable(scope* n, size_t v);

size_t set_variable(scope* n, size_t v, variable_type* type);

void pyc_compile(char* in_file, char* out_file);

#endif //PYC_COMPILER_H
