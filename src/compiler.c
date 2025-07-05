#include "compiler.h"

#define CREATED_FLAG ((size_t)1 << (sizeof(size_t)*8 - 1))

scopes pyc_out = vec();
variables pyc_all_variables = vec();
variable_type_holders pyc_variable_types = vec();
static const variable_type type_undefined = {.type = VARIABLE_CLASS_UNDEFINED};
static const variable_type type_int = {.type = VARIABLE_CLASS_INT};
static const variable_type type_float = {.type = VARIABLE_CLASS_FLOAT};
static const variable_type type_bool = {.type = VARIABLE_CLASS_BOOLEAN};
static const variable_type type_none = {.type = VARIABLE_CLASS_NONE};
static size_t var_index = 0;

void scope_append_l(scope *sc, char *b, size_t b_len) {
    if (sc == NULL || sc->content == NULL) {
        fprintf(stderr, "Error: scope or content is NULL\n");
        fail();
    }

    size_t len = sc->content->size;
    sstream_append_l(sc->content, NULL, sc->indent * 4 + b_len);
    for (size_t i = 0; i < sc->indent * 4; i++) {
        sc->content->data[len++] = ' ';
    }

    if (b != NULL) memcpy(sc->content->data + len, b, b_len);
};

void scope_appendf(scope *sc, const char *fmt, ...) {
    if (sc == NULL || sc->content == NULL) {
        fprintf(stderr, "Error: scope or content is NULL\n");
        fail();
    }

    size_t len = sc->content->size;
    sstream_append_l(sc->content, NULL, sc->indent * 4);
    for (size_t i = 0; i < sc->indent * 4; i++) {
        sc->content->data[len++] = ' ';
    }

    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (needed < 0) {
        va_end(args);
        fprintf(stderr, "Error formatting string in scope_appendf\n");
        fail();
    }
    char *buf = (char *) malloc(needed + 1);
    if (buf == NULL) {
        va_end(args);
        fprintf(stderr, "Error allocating temporary buffer in scope_appendf\n");
        fail();
    }
    vsnprintf(buf, needed + 1, fmt, args);
    va_end(args);
    sstream_append_l(sc->content, buf, (size_t) needed);
};

bool variable_types_equals(variable_types a, variable_types b) { // NOLINT(*-no-recursion)
    if (a.size != b.size) return false;
    for (size_t i = 0; i < a.size; i++) {
        if (!variable_type_equals(a.data[i], b.data[i])) return false;
    }

    return true;
}

bool variable_type_equals(variable_type *a, variable_type *b) { // NOLINT(*-no-recursion)
    if (a == b) return true;
    if (a == NULL || b == NULL || a->type != b->type) return false;

    switch (a->type) {
        case VARIABLE_CLASS_UNDEFINED:
        case VARIABLE_CLASS_NONE:
        case VARIABLE_CLASS_INT:
        case VARIABLE_CLASS_FLOAT:
        case VARIABLE_CLASS_STRING:
        case VARIABLE_CLASS_BOOLEAN:
            return true;
        case VARIABLE_CLASS_LIST:
            if (a->list.fixed_size != b->list.fixed_size) return false;
            if (!variable_type_equals(a->list.value, b->list.value)) return false;
            return variable_types_equals(a->list.values, b->list.values);
        case VARIABLE_CLASS_SET:
            if (a->set.fixed_size != b->set.fixed_size) return false;
            if (!variable_type_equals(a->set.value, b->set.value)) return false;
            return variable_types_equals(a->set.values, b->set.values);
        case VARIABLE_CLASS_TUPLE:
            if (a->tuple.fixed_size != b->tuple.fixed_size) return false;
            if (!variable_type_equals(a->tuple.value, b->tuple.value)) return false;
            return variable_types_equals(a->tuple.values, b->tuple.values);
        case VARIABLE_CLASS_DICT:
            if (a->dict.fixed_size != b->dict.fixed_size) return false;
            if (!variable_type_equals(a->dict.value, b->dict.value)) return false;
            if (!variable_types_equals(a->dict.keys, b->dict.keys)) return false;
            return variable_types_equals(a->dict.values, b->dict.values);
        case VARIABLE_CLASS_FUNCTION:
            return a->function == b->function;
        case VARIABLE_CLASS_CLASS:
            return a->class == b->class;
        case VARIABLE_CLASS_AOT_INT:
            return mpz_cmp(a->aot_int, b->aot_int) == 0;
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
        case VARIABLE_CLASS_GROUP:
            return variable_types_equals(a->group, b->group);
    }
}

variable *get_variable(scope *out, size_t name) {
    while (out != NULL) {
        for (size_t i = 0; i < out->vars.size; i++) {
            variable *var = &out->vars.data[i];
            if (var->name == name) return var;
        }
        out = out->parent;
    }
    return NULL;
}

size_t set_variable(scope *out, size_t name, variable_type *type) {
    if (out == NULL) {
        fprintf(stderr, "Error: scope is NULL\n");
        fail();
    }

    variable *var = get_variable(out, name);
    if (var != NULL) {
        var->type = type;
        return var->index;
    }

    variable new_var = {.index = ++var_index, .name = name, .type = type};
    variables_push(&out->vars, new_var);

    return var_index | CREATED_FLAG;
}

void write_variable_type(variable_type *var, scope *out) {
    if (var == NULL) {
        fprintf(stderr, "Error: variable type is NULL\n");
        fail();
    }

    switch (var->type) {
        case VARIABLE_CLASS_UNDEFINED:
            break;
        case VARIABLE_CLASS_INT:
        case VARIABLE_CLASS_AOT_INT:
            sstream_append(out->content, "mpz_t");
            break;
        case VARIABLE_CLASS_FLOAT:
        case VARIABLE_CLASS_AOT_FLOAT:
            sstream_append(out->content, "double");
            break;
        case VARIABLE_CLASS_STRING:
        case VARIABLE_CLASS_AOT_STRING:
            sstream_append(out->content, "string");
            break;
        case VARIABLE_CLASS_BOOLEAN:
        case VARIABLE_CLASS_AOT_BOOLEAN:
            sstream_append(out->content, "_Bool");
            break;
        case VARIABLE_CLASS_NONE:
            break;
        case VARIABLE_CLASS_LIST:
            break;
        case VARIABLE_CLASS_SET:
            break;
        case VARIABLE_CLASS_TUPLE:
            break;
        case VARIABLE_CLASS_DICT:
            break;
        case VARIABLE_CLASS_FUNCTION:
            break;
        case VARIABLE_CLASS_CLASS:
            break;
        case VARIABLE_CLASS_GROUP:
            break;
    }
}

void variable_types_copy(variable_types var, variable_types *out) {
    out->size = var.size;
    out->capacity = var.capacity;
    out->data = malloc(var.size * sizeof(variable_type *));
    if (out->data == NULL) {
        perror("malloc failed");
        fail();
    }
    for (size_t i = 0; i < var.size; i++) {
        out->data[i] = variable_type_copy(var.data[i]);
    }
}


variable_type *variable_type_copy(const variable_type *var) { // NOLINT(*-no-recursion)
    if (var == NULL) {
        fprintf(stderr, "Error: variable type is NULL\n");
        fail();
    }

    variable_type *new_var = malloc(sizeof(variable_type));
    if (new_var == NULL) {
        fprintf(stderr, "Error allocating memory for variable type copy\n");
        fail();
    }
    *new_var = *var;

    switch (var->type) {
        case VARIABLE_CLASS_UNDEFINED:
        case VARIABLE_CLASS_NONE:
            break;
        case VARIABLE_CLASS_INT:
        case VARIABLE_CLASS_AOT_INT:
            mpz_init(new_var->aot_int);
            break;
        case VARIABLE_CLASS_FLOAT:
        case VARIABLE_CLASS_AOT_FLOAT:
            new_var->aot_float = var->aot_float;
            break;
        case VARIABLE_CLASS_STRING:
        case VARIABLE_CLASS_AOT_STRING:
            new_var->aot_string = var->aot_string;
        case VARIABLE_CLASS_BOOLEAN:
        case VARIABLE_CLASS_AOT_BOOLEAN:
            new_var->aot_bool = var->aot_bool;
            break;
        case VARIABLE_CLASS_LIST:
            new_var->list.value = variable_type_copy(var->list.value);
            variable_types_copy(var->list.values, &new_var->list.values);
            break;
        case VARIABLE_CLASS_SET:
            new_var->set.value = variable_type_copy(var->set.value);
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
            new_var->class = var->class;
            break;
        case VARIABLE_CLASS_GROUP:
            break;
    }

    return new_var;
}

variable_type *pyc_analyze_expr(node *expr, scope *out) {
    switch (expr->type) {
        case EXPR_IDENTIFIER: {
            size_t name = expr->identifier->type >> 4;
            variable *var = get_variable(out, name);
            if (var == NULL) {
                printf("NameError: name '");
                code_substr name_str = pyc_temp_identifiers.data[name];
                print_readable(name_str.code, name_str.start, name_str.end);
                printf("' is not defined\n");
                fail();
            }
            return var->type;
        }
        case EXPR_CONSTANT:
            switch (expr->constant->type & 0xf) {
                case TOKEN_FLOAT: {
                    size_t double_index = expr->constant->type >> 4;
                    variable_type *var = variable_type_create(VARIABLE_CLASS_AOT_FLOAT);
                    var->aot_float = pyc_temp_doubles.data[double_index];
                    return var;
                }
                case TOKEN_INTEGER: {
                    size_t int_index = expr->constant->type >> 4;
                    variable_type *var = variable_type_create(VARIABLE_CLASS_AOT_INT);
                    mpz_init_set(var->aot_int, pyc_temp_bigints.data[int_index]);
                    return var;
                }
            }
            break;
        case EXPR_FORMAT_STRING:
            break;
        case EXPR_OPERATOR:
            break;
        case EXPR_BINARY_OPERATION:
            break;
        case EXPR_UNARY_OPERATION:
            break;
        case EXPR_CMP_OPERATION:
            break;
        case EXPR_WALRUS_OPERATION:
            break;
        case EXPR_LIST_COMP:
            break;
        case EXPR_LIST:
            break;
        case EXPR_DICT_COMP:
            break;
        case EXPR_DICT:
            break;
        case EXPR_SET_COMP:
            break;
        case EXPR_SET:
            break;
        case EXPR_TUPLE:
            break;
        case EXPR_LAMBDA:
            break;
        case EXPR_IF_EXP:
            break;
        case EXPR_GENERATOR:
            break;
        case EXPR_AWAIT:
            break;
        case EXPR_YIELD:
            break;
        case EXPR_YIELD_FROM:
            break;
        case EXPR_CALL:
            break;
        case EXPR_ATTRIBUTE:
            break;
        case EXPR_INDEX:
            break;
        default:
            break;
    }

    return NULL;
}

void pyc_compile_expr(bool new, size_t load, node *expr, scope *out) {
    switch (expr->type) {
        case EXPR_IDENTIFIER: {
            size_t name = expr->identifier->type >> 4;
            variable *var = get_variable(out, name);
            if (var == NULL) {
                printf("NameError: name '");
                code_substr name_str = pyc_temp_identifiers.data[name];
                print_readable(name_str.code, name_str.start, name_str.end);
                printf("' is not defined\n");
                fail();
            }

            break;
        }
        case EXPR_CONSTANT:
            switch (expr->constant->type & 0xf) {
                case TOKEN_FLOAT: {
                    size_t double_index = expr->constant->type >> 4;
                    double d = pyc_temp_doubles.data[double_index];
                    scope_appendf(out, "var_%zu = %f;", load, d);
                    break;
                }
                case TOKEN_INTEGER: {
                    if (new) scope_appendf(out, "mpz_init(var_%zu);\n", load);
                    size_t count;
                    unsigned char *bytes = mpz_export(NULL, &count, 1, 1, 1, 0,
                                                      pyc_temp_bigints.data[expr->constant->type >> 4]);
                    scope_appendf(out, "mpz_import(var_%zu, %zu, 1, 1, 1, 0, (const unsigned char[]) {",
                                  load, count);

                    for (size_t i = 0; i < count; i++) {
                        sstream_appendf(out->content, "0x%02x", bytes[i]);
                        if (i < count - 1)
                            sstream_append(out->content, ", ");
                    }
                    sstream_append(out->content, "});\n");
                    free(bytes);
                    break;
                }
            }
            break;
        case EXPR_FORMAT_STRING:
            break;
        case EXPR_OPERATOR:
            break;
        case EXPR_BINARY_OPERATION:
            break;
        case EXPR_UNARY_OPERATION:
            break;
        case EXPR_CMP_OPERATION:
            break;
        case EXPR_WALRUS_OPERATION:
            break;
        case EXPR_LIST_COMP:
            break;
        case EXPR_LIST:
            break;
        case EXPR_DICT_COMP:
            break;
        case EXPR_DICT:
            break;
        case EXPR_SET_COMP:
            break;
        case EXPR_SET:
            break;
        case EXPR_TUPLE:
            break;
        case EXPR_LAMBDA:
            break;
        case EXPR_IF_EXP:
            break;
        case EXPR_GENERATOR:
            break;
        case EXPR_AWAIT:
            break;
        case EXPR_YIELD:
            break;
        case EXPR_YIELD_FROM:
            break;
        case EXPR_CALL:
            break;
        case EXPR_ATTRIBUTE:
            break;
        case EXPR_INDEX:
            break;
        default:
            break;
    }
}

void pyc_compile_stmt(node *stmt, scope *out) { // NOLINT(*-no-recursion)
    switch (stmt->type) {
        case NODE_GROUP:
            for (size_t i = 0; i < stmt->group.v.size; i++) {
                node *child = stmt->group.v.data[i];
                pyc_compile_stmt(child, out);
            }
            break;
        case STMT_ASSIGN: {
            node *var = stmt->assign.var;
            node *val = stmt->assign.val;
            if (var->type == EXPR_IDENTIFIER) {
                // simple
                variable_type *type = pyc_analyze_expr(val, out);
                size_t name = var->identifier->type >> 4;
                size_t ret = set_variable(out, name, type);
                bool created = ret & CREATED_FLAG;
                ret &= ~CREATED_FLAG;
                if (created) {
                    scope_append_l(out, NULL, 0);
                    write_variable_type(type, out);
                    sstream_appendf(out->content, " var_%zu;\n", ret);
                }

                // todo: instead of passing in what's created, we should pass in the old type, because that's what actually matters
                pyc_compile_expr(created, ret, val, out);
            }
            break;
        }
        case STMT_ASSIGN_MULT:
            break;
        case STMT_FUNCTION_DEF:
            break;
        case STMT_CLASS_DEF:
            break;
        case STMT_RETURN:
            break;
        case STMT_DELETE:
            break;
        case STMT_FOR:
            break;
        case STMT_WHILE:
            break;
        case STMT_IF:
            break;
        case STMT_WITH:
            break;
        case STMT_MATCH:
            break;
        case STMT_RAISE:
            break;
        case STMT_TRY_CATCH:
            break;
        case STMT_ASSERT:
            break;
        case STMT_IMPORT:
            break;
        case STMT_IMPORT_FROM:
            break;
        case STMT_GLOBAL:
            break;
        case STMT_NONLOCAL:
            break;
        case STMT_EXPR:
            break;
        case STMT_PASS:
            break;
        case STMT_BREAK:
            break;
        case STMT_CONTINUE:
            break;
        default:
            break;
    }
}

void pyc_compile(char *in_file, const char *out_file) {
    FILE *file = fopen(out_file, "wb");
    if (file == NULL) {
        perror("Error opening file");
        fail();
    }

    const char *header =
            "#include <stdio.h>\n"
            "#include <stdlib.h>\n"
            "#include <string.h>\n"
            "#include <gmp.h>\n"
            "\n";
    fwrite(header, 1, strlen(header), file);

    char *code = read_file(in_file);
    if (!code)
        return;

    pyc_lexer_init(code);
    pyc_tokenize(); // pls do this after processing: tokens_clear(&pyc_tokens);

    node *prog = parse_file(NULL, in_file, code);

    sstream main_content = {.data = NULL, .size = 0};
    scope main_scope = {.parent = NULL, .indent = 1, .name = (size_t) -1, .content = &main_content};

    pyc_compile_stmt(prog, &main_scope);

    for (size_t i = 0; i < pyc_out.size; i++) {
        scope ex = pyc_out.data[i];
        fprintf(file, "void fn_%zu();\n", ex.name);
    }

    for (size_t i = 0; i < pyc_out.size; i++) {
        scope ex = pyc_out.data[i];
        fprintf(file, "void fn_%zu() {\n%.*s\n}\n\n", ex.name, (int) ex.content->size, ensure_str(ex.content->data));
    }

    fprintf(file, "int main() {\n%.*s\n    return 0;\n}", (int) main_content.size, ensure_str(main_content.data));

    fclose(file);

    scope_free(main_scope);
}

void scope_free(scope s) { // NOLINT(*-no-recursion)
    sstream_free(s.content);

    variables_clear(&s.vars);

    if (s.parent != NULL) {
        scope_free(*s.parent);
        free(s.parent);
    }
}

void variable_free(variable obj) {
    variable_type_free(obj.type);
}

void variable_type_free(variable_type *var) { // NOLINT(*-no-recursion)
    if (var == NULL) return;

    switch (var->type) {
        case VARIABLE_CLASS_UNDEFINED:
        case VARIABLE_CLASS_NONE:
        case VARIABLE_CLASS_INT:
        case VARIABLE_CLASS_FLOAT:
        case VARIABLE_CLASS_STRING:
        case VARIABLE_CLASS_BOOLEAN:
            break;
        case VARIABLE_CLASS_LIST:
            variable_type_free(var->list.value);
            variable_types_clear(&var->list.values);
            break;
        case VARIABLE_CLASS_SET:
            variable_type_free(var->set.value);
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
            node_free(var->class);
            break;
        case VARIABLE_CLASS_AOT_INT:
            mpz_clear(var->aot_int);
            break;
        case VARIABLE_CLASS_AOT_FLOAT:
            break;
        case VARIABLE_CLASS_AOT_STRING:
            free(var->aot_string.code);
            break;
        case VARIABLE_CLASS_AOT_BOOLEAN:
            break;
        case VARIABLE_CLASS_GROUP:
            variable_types_clear(&var->group);
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

void variable_p_print(variable *var, int indent) {
    variable_print(*var, indent);
}

void variable_type_print(variable_type *var, int indent) { // NOLINT(*-no-recursion)
    if (var == NULL) {
        printf("NULL");
        return;
    }

    printf("(name=%s, ", variable_class_to_str[var->type]);
    switch (var->type) {
        case VARIABLE_CLASS_UNDEFINED:
        case VARIABLE_CLASS_NONE:
        case VARIABLE_CLASS_INT:
        case VARIABLE_CLASS_FLOAT:
        case VARIABLE_CLASS_STRING:
        case VARIABLE_CLASS_BOOLEAN:
            break;
        case VARIABLE_CLASS_LIST:
            printf("value=");
            variable_type_print(var->list.value, indent + 1);
            printf(", values=");
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
            gmp_printf("value=%Zu", var->aot_int);
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
        case VARIABLE_CLASS_GROUP:
            break;
    }

    printf(")");
}

#endif
