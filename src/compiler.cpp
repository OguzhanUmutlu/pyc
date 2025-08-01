#include "compiler.h"
#include <string>

// ReSharper disable CppParameterNamesMismatch

#define CREATED_FLAG ((size_t)1 << (sizeof(size_t)*8 - 1))

scopes pyc_out = vec();
variables pyc_all_variables = vec();
variable_type_holders pyc_variable_types = vec();
static variable_type type_undefined = {.type = VARIABLE_CLASS_UNDEFINED};
static variable_type type_int = {.type = VARIABLE_CLASS_INT};
static variable_type type_float = {.type = VARIABLE_CLASS_FLOAT};
static variable_type type_bool = {.type = VARIABLE_CLASS_BOOLEAN};
static size_t var_index = 0;

bool variable_types_equals(variable_types a, variable_types b) { // NOLINT(*-no-recursion)
    if (a.size != b.size) return false;
    for (size_t i = 0; i < a.size; i++) { if (!variable_type_equals(a.data[i], b.data[i])) return false; }

    return true;
}

bool variable_type_equals(variable_type* a, variable_type* b) { // NOLINT(*-no-recursion)
    if (a == b) return true;
    if (a == nullptr || b == nullptr || a->type != b->type) return false;

    switch (a->type) {
    case VARIABLE_CLASS_UNDEFINED:
    case VARIABLE_CLASS_INT:
    case VARIABLE_CLASS_FLOAT:
    case VARIABLE_CLASS_BOOLEAN:
        return true;
    case VARIABLE_CLASS_LIST:
        if (a->list.fixed_size != b->list.fixed_size) return false;
        return variable_type_equals(a->list.values, b->list.values);
    case VARIABLE_CLASS_SET:
        if (a->set.fixed_size != b->set.fixed_size) return false;
        return variable_type_equals(a->set.values, b->set.values);
    case VARIABLE_CLASS_TUPLE:
        if (a->tuple.fixed_size != b->tuple.fixed_size) return false;
        return variable_types_equals(a->tuple.values, b->tuple.values);
    case VARIABLE_CLASS_DICT:
        if (a->dict.fixed_size != b->dict.fixed_size) return false;
        return variable_type_equals(a->dict.value, b->dict.value);
    case VARIABLE_CLASS_FUNCTION:
        return a->function == b->function;
    case VARIABLE_CLASS_CLASS:
        return a->class_ == b->class_;
    case VARIABLE_CLASS_AOT_INT:
        return a->aot_int == b->aot_int;
    case VARIABLE_CLASS_AOT_FLOAT:
        return a->aot_float == b->aot_float;
    case VARIABLE_CLASS_AOT_STRING: {
        code_substr as = a->aot_string;
        code_substr bs = b->aot_string;
        return as.end - as.start == bs.end - bs.start &&
            strncmp(as.code + as.start, bs.code + bs.start, as.end - as.start) == 0;
    }
    case VARIABLE_CLASS_AOT_BOOLEAN:
        return a->aot_bool == b->aot_bool;
    default:
        return true;
    }
}

variable* get_variable(scope* out, size_t name) {
    while (out != nullptr) {
        for (size_t i = 0; i < out->vars.size; i++) {
            variable* var = &out->vars.data[i];
            if (var->name == name) return var;
        }
        out = out->parent;
    }
    return NULL;
}

size_t set_variable(scope* out, size_t name, variable_type* type) {
    if (out == NULL) {
        fprintf(stderr, "Error: scope is NULL\n");
        fail();
    }

    variable* var = get_variable(out, name);
    if (var != NULL) {
        var->type = type;
        return var->index;
    }

    variable new_var = {.index = ++var_index, .name = name, .type = type};
    variables_push(&out->vars, new_var);

    return var_index | CREATED_FLAG;
}

void variable_types_copy(variable_types var, variable_types* out) {
    out->size = var.size;
    out->capacity = var.capacity;
    out->data = new variable_type*[var.size];
    for (size_t i = 0; i < var.size; i++) { out->data[i] = variable_type_copy(var.data[i]); }
}

variable_type* variable_type_copy(variable_type* var) { // NOLINT(*-no-recursion)
    if (var == NULL) {
        fprintf(stderr, "Error: variable type is NULL\n");
        fail();
    }

    auto new_var = new variable_type;

    *new_var = *var;

    switch (var->type) {
    case VARIABLE_CLASS_UNDEFINED:
        break;
    case VARIABLE_CLASS_AOT_INT:
        new_var->aot_int = var->aot_int;
        break;
    case VARIABLE_CLASS_AOT_FLOAT:
        new_var->aot_float = var->aot_float;
        break;
    case VARIABLE_CLASS_AOT_STRING:
        new_var->aot_string = var->aot_string;
    case VARIABLE_CLASS_BOOLEAN:
    case VARIABLE_CLASS_AOT_BOOLEAN:
        new_var->aot_bool = var->aot_bool;
        break;
    case VARIABLE_CLASS_LIST:
        variable_types_copy(var->list.values, &new_var->list.values);
        break;
    case VARIABLE_CLASS_SET:
        variable_types_copy(var->set.values, &new_var->set.values);
        break;
    case VARIABLE_CLASS_TUPLE:
        new_var->tuple.value = variable_type_copy(var->tuple.value);
        variable_types_copy(var->tuple.values, &new_var->tuple.values);
        break;
    case VARIABLE_CLASS_DICT:
        new_var->dict.key = variable_type_copy(var->dict.key);
        new_var->dict.value = variable_type_copy(var->dict.value);
        variable_types_copy(var->dict.keys, &new_var->dict.keys);
        variable_types_copy(var->dict.values, &new_var->dict.values);
        break;
    case VARIABLE_CLASS_FUNCTION:
        new_var->function = var->function;
        break;
    case VARIABLE_CLASS_CLASS:
        new_var->class_ = var->class_;
        break;
    default:
        break;
    }

    return new_var;
}

void pyc_compile(char* in_file, char* out_file) {
}

void scope_free(scope s) { // NOLINT(*-no-recursion)
    sstream_free(s.content);

    variables_clear(&s.vars);

    if (s.parent != NULL) {
        scope_free(*s.parent);
        free(s.parent);
    }
}

void variable_free(variable obj) { variable_type_free(obj.type); }

void variable_type_free(variable_type* var) { // NOLINT(*-no-recursion)
    if (var == NULL) return;

    switch (var->type) {
    case VARIABLE_CLASS_UNDEFINED:
    case VARIABLE_CLASS_INT:
    case VARIABLE_CLASS_FLOAT:
    case VARIABLE_CLASS_BOOLEAN:
        break;
    case VARIABLE_CLASS_LIST:
        variable_types_clear(&var->list.values);
        break;
    case VARIABLE_CLASS_SET:
        variable_types_clear(&var->set.values);
        break;
    case VARIABLE_CLASS_TUPLE:
        variable_type_free(var->tuple.value);
        variable_types_clear(&var->tuple.values);
        break;
    case VARIABLE_CLASS_DICT:
        variable_type_free(var->dict.value);
        variable_types_clear(&var->dict.keys);
        variable_types_clear(&var->dict.values);
        break;
    case VARIABLE_CLASS_FUNCTION:
        node_free(var->function);
        break;
    case VARIABLE_CLASS_CLASS:
        node_free(var->class_);
        break;
    case VARIABLE_CLASS_AOT_STRING:
        free(var->aot_string.code);
        break;
    default:
        break;
    }

    free(var);
}

#ifdef NEED_AST_PRINT

void variable_print(variable var, int indent) {
    printf("variable(name=");
    token_p_print(var.tok, indent + 1);
    printf(", type=");
    variable_type_print(var.type, indent + 1);
    printf(")");
}

void variable_p_print(variable* var, int indent) { variable_print(*var, indent); }

void variable_type_print(variable_type* var, int indent) { // NOLINT(*-no-recursion)
    if (var == NULL) {
        printf("NULL");
        return;
    }

    printf("(name=%s, ", variable_class_to_str[var->type]);
    switch (var->type) {
    case VARIABLE_CLASS_LIST:
        printf("values=");
        variable_types_print_indent(var->list.values, indent + 1);
        printf(", size=%zu", var->list.fixed_size);
        break;
    case VARIABLE_CLASS_SET:
        printf("value=");
        variable_type_print(var->set.value, indent + 1);
        printf(", values=");
        variable_types_print_indent(var->set.values, indent + 1);
        printf(", size=%zu", var->set.fixed_size);
        break;
    case VARIABLE_CLASS_TUPLE:
        printf("value=");
        variable_type_print(var->tuple.value, indent + 1);
        printf(", values=");
        variable_types_print_indent(var->tuple.values, indent + 1);
        printf(", size=%zu", var->tuple.fixed_size);
        break;
    case VARIABLE_CLASS_DICT:
        printf("value=");
        variable_type_print(var->dict.value, indent + 1);
        printf(", keys=");
        variable_types_print_indent(var->dict.keys, indent + 1);
        printf(", values=");
        variable_types_print_indent(var->dict.values, indent + 1);
        printf(", size=%zu", var->dict.fixed_size);
        break;
    case VARIABLE_CLASS_FUNCTION:
        node_print(var->function, indent + 1);
        break;
    case VARIABLE_CLASS_CLASS:
        node_print(var->class, indent + 1);
        break;
    case VARIABLE_CLASS_AOT_INT:
        printf("value=%" PRIu64, var->aot_int);
        break;
    case VARIABLE_CLASS_AOT_FLOAT:
        printf("value=%f", var->aot_float);
        break;
    case VARIABLE_CLASS_AOT_STRING:
        printf("value=");
        print_readable(pyc_code, var->aot_string.start, var->aot_string.end);
        break;
    case VARIABLE_CLASS_AOT_BOOLEAN:
        printf("value=%s", var->aot_bool ? "true" : "false");
        break;
    default:
        break;
    }

    printf(")");
}

// ReSharper restore CppParameterNamesMismatch

#endif
