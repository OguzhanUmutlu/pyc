#include "parser.h"
#include "lexer.h"

#pragma clang diagnostic push
#pragma ide diagnostic ignored "misc-no-recursion"
static bool paren = false;

size_t count_indent(size_t index) {
    size_t spaces = 0;

    for (size_t i = index - 1; i-- > 0;) {
        char ch = pyc_code[i];
        if (ch == ' ' || ch == '\t') spaces++;
        else break;
    }

    return spaces;
}

node *parse_file(node *parent, char *filename, char *code) {
    node *mod = ast_node_create(parent);
    mod->type = NODE_MODULE;
    mod->module.filename = filename;
    mod->module.code = code;
    mod->module.tokens = pyc_tokens;
    mod->module.code_len = pyc_code_len;
    node *parsed = ast_node_create(mod);
    parse_statements(parsed); // pls do this after processing: node_clear(&parsed);
    return parsed;
}

size_t get_node_tok_index(size_t expect, bool care_colon, bool care_comma,
                          bool anyways) {
    int p1 = 0, p2 = 0, p3 = 0;

    for (size_t i = pyc_ti; i < pyc_tok_count; i++) {
        token tok = pyc_tokens.data[i];
        size_t tok_type = tok.type;
        size_t tok_type_t = tok_type & 0xf;
        if (p1 == 0 && p2 == 0 && p3 == 0 && tok.type == expect) {
            return i;
        }

        if (tok_type_t == TOKEN_KEYWORD || tok_type_t == TOKEN_SET_OPERATOR ||
            tok_type_t == TOKEN_FSTRING_MIDDLE ||
            tok_type_t == TOKEN_FSTRING_END ||
            tok_type == TOKEN_LINE_BREAK_SEMICOLON ||
            (!paren && tok_type == TOKEN_LINE_BREAK_NEWLINE) ||
            (care_colon && tok_type == TOKEN_SYMBOL_COLON) ||
            (care_comma && tok_type == TOKEN_SYMBOL_COMMA) ||
            tok_type == TOKEN_SYMBOL_EXC) {
            return anyways ? i : -1;
        }

        switch (tok_type) {
            case TOKEN_SYMBOL_LPAREN:
                p1++;
                break;
            case TOKEN_SYMBOL_LBRACKET:
                p2++;
                break;
            case TOKEN_SYMBOL_LBRACE:
                p3++;
                break;
            case TOKEN_SYMBOL_RPAREN:
                if (p1 == 0) {
                    if (anyways)
                        continue;
                    return -1;
                }
                p1--;
                break;
            case TOKEN_SYMBOL_RBRACKET:
                if (p2 == 0) {
                    if (anyways)
                        continue;
                    return -1;
                }
                p2--;
                break;
            case TOKEN_SYMBOL_RBRACE:
                if (p3 == 0) {
                    if (anyways)
                        continue;
                    return -1;
                }
                p3--;
                break;
            default:
                break;
        }
    }

    return anyways ? pyc_tok_count : -1;
}

node *simplify_group(node *ex) {
    nodes grp;
    while (ex->type == NODE_GROUP && (grp = ex->group.v).size == 1) {
        ex = grp.data[0];
        free(grp.data);
    }
    return ex;
}

// If group has 1 element, it replaces the group with the value in it
bool trim_group(node *ex) {
    if (ex->group.v.size == 1) {
        node *el = ex->group.v.data[0];
        free(ex->group.v.data);
        *ex = *el;
        return true;
    }

    return false;
}

#define expression_ends(tok, tok_type)                                         \
    (tok_type_t == TOKEN_LINE_BREAK || tok_type_t == TOKEN_KEYWORD ||          \
     tok_type_t == TOKEN_SET_OPERATOR || tok_type_t == TOKEN_FSTRING_MIDDLE || \
     tok_type_t == TOKEN_FSTRING_END || tok_type == TOKEN_SYMBOL_COLON ||      \
     tok_type == TOKEN_SYMBOL_COMMA || tok_type == TOKEN_SYMBOL_RPAREN ||      \
     tok_type == TOKEN_SYMBOL_RBRACKET || tok_type == TOKEN_SYMBOL_RBRACE ||   \
     tok_type == TOKEN_SYMBOL_EXC)

void parser_ignore_type() {
    size_t end_ind = get_node_tok_index(TOKEN_SET_OPERATOR_EQ, true, false, true);
    if (end_ind != -1)
        pyc_ti = end_ind;
}

nodes pratt_tokens;
size_t pratt_tok_count;
size_t pratt_pos;
bool pratt_free; // basically frees operators if true

#define pratt_tokens_over() (pratt_pos >= pratt_tok_count)

static inline size_t pratt_lbp(size_t tok_type) {
    switch (tok_type) {
        case TOKEN_OPERATOR_WALRUS:
            return 1;
        case TOKEN_OPERATOR_OR:
            return 2;
        case TOKEN_OPERATOR_AND:
            return 3;
        case TOKEN_OPERATOR_NOT:
            return 4;
        case TOKEN_OPERATOR_IN:
        case TOKEN_OPERATOR_IS:
        case TOKEN_OPERATOR_NOT_IN:
        case TOKEN_OPERATOR_IS_NOT:
        case TOKEN_OPERATOR_EQ:
        case TOKEN_OPERATOR_NEQ:
        case TOKEN_OPERATOR_GT:
        case TOKEN_OPERATOR_LT:
        case TOKEN_OPERATOR_GTE:
        case TOKEN_OPERATOR_LTE:
            return 5;
        case TOKEN_OPERATOR_BIT_OR:
            return 6;
        case TOKEN_OPERATOR_BIT_XOR:
            return 7;
        case TOKEN_OPERATOR_BIT_AND:
            return 8;
        case TOKEN_OPERATOR_LSH:
        case TOKEN_OPERATOR_RSH:
            return 9;
        case TOKEN_OPERATOR_ADD:
        case TOKEN_OPERATOR_SUB:
            return 10;
        case TOKEN_OPERATOR_MUL:
        case TOKEN_OPERATOR_DIV:
        case TOKEN_OPERATOR_FDIV:
        case TOKEN_OPERATOR_MOD:
        case TOKEN_OPERATOR_MAT_MUL:
            return 11;
        case TOKEN_OPERATOR_POW:
            return 12;
        default:
            return 0;
    }
}

node *parse_pratt_iter(size_t rbp) {
    node *nd = pratt_tokens.data[pratt_pos++];
    node *left = nd;
    if (nd->type == EXPR_OPERATOR) {
        token *tok = nd->operator;
        size_t tok_type = tok->type;
        if (tok_type == TOKEN_OPERATOR_ADD || tok_type == TOKEN_OPERATOR_SUB || tok_type == TOKEN_OPERATOR_BIT_NOT ||
            tok_type == TOKEN_OPERATOR_NOT) {
            left = ast_node_create(nd->parent);
            left->type = EXPR_UNARY_OPERATION;
            left->unary_op.op = tok;
            left->unary_op.expr = parse_pratt_iter(100);
            if (pratt_free) node_free(nd);
        }
    }

    while (!pratt_tokens_over()) {
        node *now = pratt_tokens.data[pratt_pos];
        if (now->type != EXPR_OPERATOR) break;

        token *tok = now->operator;
        size_t tok_type = tok->type;
        if (pratt_lbp(tok->type) <= rbp) break;
        pratt_pos++;

        node *new_left = ast_node_create(now->parent);
        switch (tok_type) {
            case TOKEN_OPERATOR_WALRUS:
                new_left->type = EXPR_BINARY_OPERATION;
                new_left->bin_op.left = left;
                new_left->bin_op.right = parse_pratt_iter(pratt_lbp(tok_type));
                break;
            case TOKEN_OPERATOR_LT:
            case TOKEN_OPERATOR_GT:
            case TOKEN_OPERATOR_LTE:
            case TOKEN_OPERATOR_GTE:
            case TOKEN_OPERATOR_EQ:
            case TOKEN_OPERATOR_NEQ:
            case TOKEN_OPERATOR_IN:
            case TOKEN_OPERATOR_NOT:
            case TOKEN_OPERATOR_IS:
            case TOKEN_OPERATOR_IS_NOT:
            case TOKEN_OPERATOR_NOT_IN:
                new_left->type = EXPR_CMP_OPERATION;
                tokens_p *cmp_ops = &new_left->cmp_op.op;
                nodes *cmp_ex = &new_left->cmp_op.ex;
                tokens_p_init(cmp_ops);
                nodes_init(cmp_ex);
                cmp_ex->data[0] = left;
                cmp_ex->size = 1;
                nodes_push(cmp_ex, parse_pratt_iter(5));

                while (!pratt_tokens_over()) {
                    node *new_nd = pratt_tokens.data[pratt_pos];
                    if (new_nd->type != EXPR_OPERATOR) {
                        nodes_print(pratt_tokens);
                        raise_error("SyntaxError: expected an operator");
                    }

                    token *new_tok = new_nd->operator;
                    size_t new_tok_type = new_tok->type;
                    if (new_tok_type == TOKEN_OPERATOR_LT || new_tok_type == TOKEN_OPERATOR_GT ||
                        new_tok_type == TOKEN_OPERATOR_LTE || new_tok_type == TOKEN_OPERATOR_GTE ||
                        new_tok_type == TOKEN_OPERATOR_EQ || new_tok_type == TOKEN_OPERATOR_NEQ ||
                        new_tok_type == TOKEN_OPERATOR_IN || new_tok_type == TOKEN_OPERATOR_NOT ||
                        new_tok_type == TOKEN_OPERATOR_IS || new_tok_type == TOKEN_OPERATOR_IS_NOT ||
                        new_tok_type == TOKEN_OPERATOR_NOT_IN) {
                        tokens_p_push(cmp_ops, new_tok);
                        if (pratt_free) node_free(new_nd);
                        pratt_pos++;
                        nodes_push(cmp_ex, parse_pratt_iter(5));
                    } else break;
                }
                break;
            default:
                new_left->type = EXPR_BINARY_OPERATION;
                new_left->bin_op.left = left;
                new_left->bin_op.op = tok;
                new_left->bin_op.right = parse_pratt_iter(
                        pratt_lbp(tok_type) - (tok_type == TOKEN_OPERATOR_POW ? 1 : 0));
                break;
        }

        left = new_left;
    }

    return left;
}

node *parse_pratt(nodes t) {
    pratt_tokens = t;
    pratt_tok_count = t.size;
    pratt_pos = 0;
    pratt_free = true;

    if (pratt_tokens.size == 0) {
        raise_error("SyntaxError: expected an expression");
    }

    return parse_pratt_iter(0);
}

void parse_expression_group(node *ex) {
    ex->type = NODE_GROUP;
    nodes *group = &ex->group.v;
    nodes_init(group);
    size_t start = pyc_ti;
    bool trim = ex->parent == NULL || ex->parent->type != STMT_FUNCTION_DEF;

    while (!tokens_over()) {
        token tok = token_peek(0);
        size_t tok_type = tok.type;
        size_t tok_type_t = tok_type & 0xf;

        if (tok_type == TOKEN_LINE_BREAK_NEWLINE && paren) {
            pyc_ti++;
            continue;
        }

        if (tok_type != TOKEN_KEYWORD_LAMBDA &&
            expression_ends(tok, tok_type)) {
            if (start == pyc_ti) {
                syntax_error();
            }

            break;
        }

        if (start != pyc_ti &&
            (tok_type == TOKEN_KEYWORD_FOR || tok_type == TOKEN_KEYWORD_ASYNC ||
             tok_type == TOKEN_KEYWORD_IF))
            break;

        node *child = ast_node_create(ex);
        parse_expression_next(child);
        child = simplify_group(child);
        nodes_push(group, child);
    }

    if (ex->group.v.size == 0) {
        syntax_error();
    }

    if (!tokens_over()) {
        if (tok_now_teq(TOKEN_KEYWORD_IF)) {
            node *ch = ast_node_create(ex);
            ch->type = EXPR_IF_EXP;
            node *if_expr = ast_node_create(ex);
            if_expr->group = ex->group;
            if (!trim_group(if_expr))
                if_expr->type = NODE_GROUP;
            ch->if_else_expr.if_expr = if_expr;

            pyc_ti++;
            ch->if_else_expr.condition = parse_expression_group_child(ch);

            assert_tokenP(TOKEN_KEYWORD_ELSE);

            ch->if_else_expr.else_expr = parse_expression_group_child(ch);

            nodes_init(&ex->group.v); // re-initialize it because we're using it
            // in the if_expr
            ex->group.v.data[0] = ch;
            ex->group.v.size = 1;
        } else if (tok_now_teq(TOKEN_KEYWORD_FOR) ||
                   tok_now_teq(TOKEN_KEYWORD_ASYNC)) {
            node *ch = ast_node_create(ex);
            ch->type = EXPR_GENERATOR;
            node *val_expr = ast_node_create(ex);
            val_expr->type = NODE_GROUP;
            val_expr->group = ex->group;
            ch->generator.value = val_expr;
            comprehensions_init(&ch->generator.comp);

            parse_comprehensions_post(ex, &ch->generator.comp);

            nodes_init(&ex->group.v); // re-initialize it because we're using it
            // in the if_expr
            ex->group.v.data[0] = ch;
            ex->group.v.size = 1;
        }
    }

    if (ex->group.v.size > 1) {
        node *res = parse_pratt(ex->group.v);
        if (trim) {
            free(ex->group.v.data);
            *ex = *res;
            free(res);
        } else {
            ex->group.v.data[0] = res;
            ex->group.v.size = 1;
            nodes_shrink(&ex->group.v);
        }
    } else if (!trim || !trim_group(ex)) {
        nodes_shrink(group);
    }
}

void parse_expression(node *ex) {
    parse_expression_tuple(ex);

    if (ex->tuple.v.size == 1 && char_peek(-1) != ',') {
        _Static_assert(sizeof(ex->group) == sizeof(ex->tuple) &&
                       sizeof(ex->group) == 32,
                       "Group and tuple do not match in size (and equal to 32 or `nodes+token*`).");
        if (!trim_group(ex))
            ex->type = NODE_GROUP;
    } else if (!paren && ex->tuple.v.size == 0) {
        raise_error("SyntaxError: expected an expression");
    }
}

node *parse_expression_group_child(node *parent) {
    node *expr = ast_node_create(parent);
    parse_expression_group(expr);
    return simplify_group(expr);
}

node *parse_expression_tuple_child(node *parent) {
    node *expr = ast_node_create(parent);
    parse_expression_tuple(expr);
    return simplify_group(expr);
}

node *parse_expression_child(node *parent) {
    node *expr = ast_node_create(parent);
    parse_expression(expr);
    return expr;
}

void parse_expression_lambda(node *ex) {
    ex->lambda.tok = &token_peek(0);
    pyc_ti++;
    ex->type = EXPR_LAMBDA;
    tokens_p *args = &ex->lambda.args;
    tokens_p_init(args);

    while (!tokens_over()) {
        token *tok_p = &token_peek(0);
        token tok = *tok_p;

        if (tok.type == TOKEN_SYMBOL_COLON) {
            pyc_ti++;
            break;
        }

        if ((tok.type & 0xf) != TOKEN_IDENTIFIER) {
            syntax_error();
        }

        size_t tok_type = tok.type;

        for (size_t i = 0; i < args->size; i++) {
            if (args->data[i]->type == tok_type) {
                raise_error_t(tok, "SyntaxError: duplicate argument 'a' in function definition");
            }
        }

        tokens_p_push(args, tok_p);
        pyc_ti++;

        token t = token_peek(0);
        pyc_ti++;

        if (t.type == TOKEN_SYMBOL_COLON) {
            break;
        }

        if (t.type != TOKEN_SYMBOL_COMMA) {
            syntax_error();
        }
    }

    ex->lambda.body = parse_expression_group_child(ex);
}

void parse_expression_fstring(node *ex) {
    ex->type = EXPR_FORMAT_STRING;
    ex->fstring.start_string = &token_peek(0);
    nodes_init(&ex->fstring.values);
    tokens_p_init(&ex->fstring.strings);
    pyc_ti++;

    while (!tokens_over()) {
        node *childExpr = parse_expression_child(ex);
        nodes_push(&ex->fstring.values, childExpr);

        token tk = token_peek(0);

        if (tk.type == TOKEN_SYMBOL_EXC || tk.type == TOKEN_SYMBOL_COLON) {
            pyc_ti++;
            if (tokens_over())
                syntax_error();
            node *t = ast_node_create(ex);
            parse_expression(t);
            nodes_push(&ex->fstring.extras, t);
        } else
            nodes_push(&ex->fstring.extras, NULL);

        token *tok_p = &token_peek(0);
        token tok = *tok_p;
        size_t tok_type_t = tok.type & 0xf;

        if (tok_type_t != TOKEN_FSTRING_END &&
            tok_type_t != TOKEN_FSTRING_MIDDLE) {
            syntax_error();
        }

        tokens_p_push(&ex->fstring.strings, tok_p);

        pyc_ti++;

        if (tok_type_t == TOKEN_FSTRING_END) {
            break;
        }
    }

    nodes_shrink(&ex->fstring.values);
    tokens_p_shrink(&ex->fstring.strings);
}

void accumulate_tuple_expression(node *parent, nodes *tuple) {
    bool found_named_arg = false;
    bool last_comma = false;

    while (!tokens_over()) {
        token tok = token_peek(0);
        size_t tok_type = tok.type;
        size_t tok_type_t = tok_type & 0xf;

        if (tok_type == TOKEN_LINE_BREAK_NEWLINE && paren) {
            pyc_ti++;
            continue;
        }

        if (tok_type == TOKEN_SYMBOL_COMMA) {
            if (last_comma)
                syntax_error();
            last_comma = true;
            pyc_ti++;
            continue;
        }

        if (tok_type != TOKEN_KEYWORD_LAMBDA && expression_ends(tok, tok_type))
            break;

        last_comma = false;
        nodes_push(tuple, parse_expression_group_child(parent));

        if (tokens_over())
            break;
    }

    nodes_shrink(tuple);
}

void accumulate_call_arguments(node *parent) {
    bool found_named_arg = false;
    bool last_comma = false;
    nodes *args = &parent->call.args;
    keywords *kws = &parent->call.kws;
    nodes_init(args);
    keywords_init(kws);

    while (!tokens_over()) {
        token *tok_p = &token_peek(0);
        token tok = *tok_p;
        size_t tok_type = tok.type;
        size_t tok_type_t = tok_type & 0xf;

        if (tok_type == TOKEN_SYMBOL_RPAREN)
            break;

        if (tok_type == TOKEN_LINE_BREAK_NEWLINE) {
            pyc_ti++;
            continue;
        }

        if (tok_type == TOKEN_SYMBOL_COMMA) {
            if (last_comma)
                syntax_error();
            last_comma = true;
            pyc_ti++;
            continue;
        }

        last_comma = false;

        if (tok_type_t == TOKEN_IDENTIFIER && pyc_ti + 1 < pyc_tok_count &&
            token_peek(1).type == TOKEN_SET_OPERATOR_EQ) {
            found_named_arg = true;
            pyc_ti += 2;
            if (tokens_over())
                syntax_error();
            keyword kw = {.name = tok_p, .value = parse_expression_group_child(parent)};
            keywords_push(kws, kw);
            continue;
        } else if (found_named_arg && !tok_val_eq(tok, "**") &&
                   !tok_val_eq(tok, "*")) {
            syntax_error();
        }

        node *ch = parse_expression_group_child(parent);
        nodes_push(args, ch);

        if (tokens_over())
            break;
    }

    nodes_shrink(args);
}

void parse_expression_tuple(node *ex) {
    ex->type = EXPR_TUPLE;
    nodes_init(&ex->tuple.v);
    accumulate_tuple_expression(ex, &ex->tuple.v);
}

void parse_expression_list(node *ex) {
    ex->type = EXPR_LIST;
    ex->list.tok = &token_peek(-1);
    nodes_init(&ex->list.v);
    accumulate_tuple_expression(ex, &ex->list.v);
}

void parse_expression_set(node *ex) {
    ex->type = EXPR_SET;
    ex->set.tok = &token_peek(-1);
    nodes_init(&ex->set.v);
    accumulate_tuple_expression(ex, &ex->set.v);
}

void parse_expression_dict(node *ex) {
    ex->type = EXPR_DICT;
    nodes_init(&ex->dict.keys);
    nodes_init(&ex->dict.values);
    size_t start = pyc_ti;
    nodes *keys = &ex->dict.keys;
    nodes *values = &ex->dict.values;

    while (!tokens_over()) {
        token *tok_p = &pyc_tokens.data[pyc_ti];
        token tok = *tok_p;
        size_t tok_type = tok.type;
        size_t tok_type_t = tok_type & 0xf;

        if (tok_type == TOKEN_SYMBOL_RBRACE)
            break;

        if (tok_type == TOKEN_LINE_BREAK_NEWLINE) {
            pyc_ti++;
            continue;
        }

        if (tok_type_t == TOKEN_LINE_BREAK || tok_type_t == TOKEN_KEYWORD ||
            tok_type_t == TOKEN_SET_OPERATOR ||
            tok_type_t == TOKEN_FSTRING_MIDDLE ||
            tok_type_t == TOKEN_FSTRING_END) {
            if (start == pyc_ti) {
                syntax_error();
            }

            break;
        }

        nodes_push(keys, parse_expression_group_child(ex));
        assert_tokenP(TOKEN_SYMBOL_COLON);
        nodes_push(values, parse_expression_group_child(ex));

        if (token_peek(0).type == TOKEN_SYMBOL_RBRACE)
            break;

        assert_tokenP(TOKEN_SYMBOL_COMMA);
    }

    nodes_shrink(keys);
    nodes_shrink(values);
}

void parse_comprehensions_post(node *parent, comprehensions *list) {
    while (!tokens_over()) {
        bool is_async = tok_now_teq(TOKEN_KEYWORD_ASYNC);
        if (is_async)
            pyc_ti++;

        if (!tok_now_teq(TOKEN_KEYWORD_FOR))
            break;
        pyc_ti++;

        comprehension comp;
        comp.is_async = is_async;
        nodes_init(&comp.ifs);

        size_t in_token = get_node_tok_index(TOKEN_OPERATOR_IN, true, false, false);
        if (in_token == -1) {
            syntax_error();
        }

        pyc_tok_count = in_token;
        comp.target = parse_expression_child(parent);
        pyc_tok_count = pyc_tokens.size;
        assert_tokenP(TOKEN_OPERATOR_IN);

        size_t if_token = get_node_tok_index(TOKEN_KEYWORD_IF, true, false, false);
        if (if_token != -1)
            pyc_tok_count = if_token;
        comp.iter = parse_expression_child(parent);
        pyc_tok_count = pyc_tokens.size;

        size_t else_token = get_node_tok_index(TOKEN_KEYWORD_ELSE, true, false, false);

        if (else_token != -1) {
            raise_error_t(pyc_tokens.data[else_token],
                          "SyntaxError: invalid syntax");
        }

        while (!tokens_over() && tok_now_teq(TOKEN_KEYWORD_IF)) {
            pyc_ti++;
            nodes_push(&comp.ifs, parse_expression_group_child(parent));
        }

        nodes_shrink(&comp.ifs);

        comprehensions_push(list, comp);
    }

    comprehensions_shrink(list);
}

void parse_comprehensions(node *valueExpr, comprehensions *list) {
    node *parent = valueExpr->parent;
    if (parent == NULL) fail();

    parse_expression(valueExpr);

    parse_comprehensions_post(parent, list);
}

void parse_expression_list_comp(node *ex) {
    ex->type = EXPR_LIST_COMP;

    node *gen = parse_expression_child(ex);
    if (gen->type != EXPR_GENERATOR) {
        syntax_error();
    }

    ex->list_comp.value = gen->generator.value;
    ex->list_comp.comp = gen->generator.comp;

    free(gen);
}

void parse_expression_set_comp(node *ex) {
    ex->type = EXPR_SET_COMP;

    node *gen = parse_expression_child(ex);
    if (gen->type != EXPR_GENERATOR) {
        syntax_error();
    }

    ex->set_comp.value = gen->generator.value;
    ex->set_comp.comp = gen->generator.comp;

    free(gen);
}

void parse_expression_dict_comp(node *ex) {
    ex->type = EXPR_DICT_COMP;
    ex->dict_comp.key = parse_expression_group_child(ex);

    assert_tokenP(TOKEN_SYMBOL_COLON);

    node *valueExpr = ex->dict_comp.value = ast_node_create(ex);
    comprehensions *comp = &ex->dict_comp.comp;
    comprehensions_init(comp);

    parse_comprehensions(valueExpr, comp);
}

void parse_expression_await(node *ex) {
    pyc_ti++;
    ex->type = EXPR_AWAIT;
    ex->await = parse_expression_group_child(ex);
}

void parse_expression_yield(node *ex) {
    pyc_ti++;

    if (tokens_over())
        syntax_error();

    if (tok_peek_teq(1, TOKEN_KEYWORD_FROM)) {
        pyc_ti++;
        ex->type = EXPR_YIELD_FROM;
        ex->yield_from = parse_expression_group_child(ex);
    } else {
        ex->type = EXPR_YIELD;
        ex->yield = parse_expression_group_child(ex);
    }
}

void parse_expression_next(node *ex) {
    token *tok_p = &token_peek(0);
    token tok = *tok_p;
    size_t tok_type = tok.type;
    size_t tok_type_t = tok_type & 0xf;

    if (tok_type == TOKEN_KEYWORD_LAMBDA)
        return parse_expression_lambda(ex);
    else if (tok_type == TOKEN_KEYWORD_AWAIT)
        return parse_expression_await(ex);
    else if (tok_type == TOKEN_KEYWORD_YIELD)
        return parse_expression_yield(ex);

    if (tok_type_t == TOKEN_INTEGER || tok_type_t == TOKEN_FLOAT ||
        tok_type_t == TOKEN_BOOLEAN || tok_type_t == TOKEN_STRING ||
        tok_type_t == TOKEN_NONE) {
        ex->type = EXPR_CONSTANT;
        ex->constant = tok_p;
        pyc_ti++;
        return;
    }

    if (tok_type_t == TOKEN_FSTRING_START) {
        parse_expression_fstring(ex);
        return;
    }

    if (tok_type_t == TOKEN_IDENTIFIER) {
        ex->type = EXPR_IDENTIFIER;
        ex->identifier = tok_p;
        pyc_ti++;
        return;
    }

    if (tok_type_t == TOKEN_OPERATOR) {
        ex->type = EXPR_OPERATOR;
        ex->operator = tok_p;
        pyc_ti++;
        return;
    }

    if (tok_type_t == TOKEN_SYMBOL) {
        node *parent = ex->parent;
        bool p = paren;
        switch (tok_type) {
            case TOKEN_SYMBOL_DOT:
                pyc_ti++;
                token *tok2_p = &token_peek(0);
                token tok2 = *tok2_p;
                if ((tok2.type & 0xf) != TOKEN_IDENTIFIER &&
                    tok2.type != TOKEN_KEYWORD_MATCH) {
                    syntax_error();
                }
                pyc_ti++;

                if (parent == NULL || parent->type != NODE_GROUP ||
                    parent->group.v.size == 0)
                    syntax_error();

                node *base = nodes_pop(&parent->group.v);

                base->parent = ex;
                ex->type = EXPR_ATTRIBUTE;
                ex->attribute.base = base;
                ex->attribute.key = tok2_p;
                break;
            case TOKEN_SYMBOL_LPAREN:
                pyc_ti++;
                paren = true;
                if (parent && parent->type == NODE_GROUP &&
                    parent->group.v.size >= 1) {
                    node *back = *nodes_back(parent->group.v);
                    if (back->type != EXPR_OPERATOR &&
                        back->type != EXPR_CONSTANT) {
                        nodes_pop(&parent->group.v);
                        ex->type = EXPR_CALL;
                        back->parent = ex;
                        ex->call.base = back;
                        if (token_peek(0).type == TOKEN_SYMBOL_RPAREN) {
                            pyc_ti++;
                            nodes_init(&ex->call.args);
                            keywords_init(&ex->call.kws);
                            paren = p;
                            return;
                        } else {
                            accumulate_call_arguments(ex);
                        }

                        assert_tokenP(TOKEN_SYMBOL_RPAREN);
                        paren = p;
                        return;
                    }
                }

                parse_expression(ex);
                if (ex->type == NODE_GROUP) {
                    ex->group.tok = tok_p;
                }

                assert_tokenP(TOKEN_SYMBOL_RPAREN);
                paren = p;
                break;
            case TOKEN_SYMBOL_LBRACKET:
                pyc_ti++;
                paren = true;

                if (parent && parent->type == NODE_GROUP &&
                    parent->group.v.size >= 1) {
                    node *back = nodes_pop(&parent->group.v);
                    ex->type = EXPR_INDEX;
                    back->parent = ex;
                    ex->index.base = back;
                    ex->index.slices[0] = NULL;
                    ex->index.slices[1] = NULL;
                    ex->index.slices[2] = NULL;
                    token t2;
                    for (int i = 0; i < 3; i++) {
                        t2 = token_peek(0);

                        if (t2.type == TOKEN_SYMBOL_RBRACKET)
                            break;

                        if (t2.type == TOKEN_SYMBOL_COLON) {
                            pyc_ti++;
                            continue;
                        } else {
                            node *sl = parse_expression_child(ex);
                            ex->index.slices[i] = sl;
                            if (tokens_over() ||
                                ((t2 = token_peek(0)).type != TOKEN_SYMBOL_COLON &&
                                 t2.type != TOKEN_SYMBOL_RBRACKET))
                                syntax_error();
                            if (i == 2)
                                break;
                            if (t2.type == TOKEN_SYMBOL_COLON) {
                                pyc_ti++;
                                continue;
                            }
                        }
                    }

                    if (t2.type != TOKEN_SYMBOL_RBRACKET)
                        syntax_error();
                    pyc_ti++;
                    paren = p;
                    return;
                } else {
                    size_t for_index = get_node_tok_index(TOKEN_KEYWORD_FOR, true, false, false);

                    if (for_index != -1) {
                        parse_expression_list_comp(ex);
                    } else {
                        parse_expression_list(ex);
                    }
                }

                assert_tokenP(TOKEN_SYMBOL_RBRACKET);
                paren = p;
                break;
            case TOKEN_SYMBOL_LBRACE:
                pyc_ti++;
                paren = true;
                size_t for_ind = get_node_tok_index(TOKEN_KEYWORD_FOR, false, false, false);
                size_t colon_ind = get_node_tok_index(TOKEN_SYMBOL_COLON, true, false, false);

                if (for_ind != -1) {
                    if (colon_ind != -1) {
                        comprehensions_init(&ex->dict_comp.comp);
                        parse_expression_dict_comp(ex);
                    } else {
                        comprehensions_init(&ex->set_comp.comp);
                        parse_expression_set_comp(ex);
                    }
                } else if (colon_ind != -1)
                    parse_expression_dict(ex);
                else
                    parse_expression_set(ex);
                assert_tokenP(TOKEN_SYMBOL_RBRACE);
                paren = p;
                break;
            default:
                break;
        }
        return;
    }

    syntax_error();
}

// does the indentation check etc. used 3 times in the content so might as well...
#define indent_group_macro_thing(tok_type, tok, spaces, spaces_parent, found)  \
    if (tok_type == TOKEN_LINE_BREAK_NEWLINE) {                                \
        pyc_ti++;                                                              \
        while (!tokens_over() && token_type_peek_t(0) == TOKEN_LINE_BREAK)     \
            pyc_ti++;                                                          \
        if (tokens_over())                                                     \
            break;                                                             \
        tok = token_peek(0);                                                   \
        int s = count_indent(tok.start);                                       \
                                                                               \
        if (found) {                                                           \
            if (s < spaces) {                                                  \
                if (pyc_ti != 0)                                               \
                    pyc_ti--;                                                  \
                break;                                                         \
            } else if (s > spaces)                                             \
                tab_error();                                                   \
        } else {                                                               \
            if (spaces_parent != 0 && s <= spaces_parent) {                    \
                if (pyc_ti != 0)                                               \
                    pyc_ti--;                                                  \
                break;                                                         \
            }                                                                  \
                                                                               \
            found = true;                                                      \
            spaces = s;                                                        \
        }                                                                      \
    }

#define equal_indent_group_macro_thing(tok_type, tok, spaces)                  \
    if (tok_type == TOKEN_LINE_BREAK_NEWLINE) {                                \
        pyc_ti++;                                                              \
        while (!tokens_over() && token_type_peek_t(0) == TOKEN_LINE_BREAK)     \
            pyc_ti++;                                                          \
        if (tokens_over())                                                     \
            break;                                                             \
        tok = token_peek(0);                                                   \
        int s = count_indent(tok.start);                                       \
        if (spaces_parent != 0 && s != spaces_parent) {                        \
            if (pyc_ti != 0)                                                   \
                pyc_ti--;                                                      \
            break;                                                             \
        }                                                                      \
        tok_type = tok.type;                                                   \
    }

// def test():
//            ^ this function is run right here. right after the ':'. allowing
//            the use of semicolons
void parse_statement_group(node *st, int spaces_parent) {
    st->type = NODE_GROUP;
    node *parent = st->parent;
    bool class_def = parent != NULL && parent->type == STMT_CLASS_DEF;
    bool match_def = parent != NULL && parent->type == STMT_MATCH;
    if (!class_def && !match_def) nodes_init(&st->group.v);
    else if (st->parent == NULL) fail();
    int spaces = spaces_parent;
    bool found = false;
    bool added_any = false;
    if (tokens_over()) raise_error("expected a statement");
    token start = token_peek(0);

    while (!tokens_over()) {
        token tok = token_peek(0);
        size_t tok_type = tok.type;

        indent_group_macro_thing(tok_type, tok, spaces, spaces_parent, found)

        node *childStmt = ast_node_create(st);
        parse_statement_next(childStmt, spaces);

        added_any = true;

        if (class_def) {
            node_type type = childStmt->type;
            if (type == STMT_FUNCTION_DEF) {
                nodes_push(&parent->class_def.methods, childStmt);
            } else if (type == STMT_ASSIGN || type == STMT_ASSIGN_MULT) {
                nodes_push(&parent->class_def.properties, childStmt);
            } else if (type != STMT_PASS && type != STMT_EXPR) {
                syntax_error_t(tok);
            }
        } else {
            nodes_push(&st->group.v, childStmt);
        }
    }

    if (!added_any)
        raise_error_t(start, "expected a statement");

    if (!class_def)
        nodes_shrink(&st->group.v);
}

node *parse_statement_group_child(node *parent, int spaces_parent) {
    node *st = ast_node_create(parent);
    parse_statement_group(st, spaces_parent);
    return st;
}

void parse_statement_group_match_def(node *parent, int spaces_parent) {
    match_cases_init(&parent->match.cases);
    int spaces = spaces_parent;
    bool found = false;

    while (!tokens_over()) {
        token tok = token_peek(0);
        size_t tok_type = tok.type;

        indent_group_macro_thing(tok_type, tok, spaces, spaces_parent, found)

        if (!tok_now_teq(TOKEN_KEYWORD_CASE)) syntax_error();
        pyc_ti++;

        match_case cas;
        size_t if_index = get_node_tok_index(TOKEN_KEYWORD_IF, true, false, false);
        size_t len = pyc_tok_count;
        if (if_index != -1)
            pyc_tok_count = if_index;
        cas.pattern = parse_expression_child(parent);
        pyc_tok_count = len;
        if (tok_now_teq(TOKEN_KEYWORD_AS)) {
            pyc_ti++;
            token *tok2 = &token_peek(0);
            if ((tok2->type & 0xf) != TOKEN_IDENTIFIER)
                syntax_error();
            cas.as = tok2;
            pyc_ti++;
        } else
            cas.as = NULL;

        if (tok_now_teq(TOKEN_KEYWORD_IF)) {
            pyc_ti++;
            cas.condition = parse_expression_group_child(parent);
        } else
            cas.condition = NULL;

        assert_tokenP(TOKEN_SYMBOL_COLON);

        cas.body = parse_statement_group_child(parent, spaces);
        match_cases_push(&parent->match.cases, cas);
    }

    if (parent->match.cases.size == 0)
        syntax_error();

    match_cases_shrink(&parent->match.cases);
}

void parse_statement_group_try_catch(node *parent, int spaces_parent) {
    assert_tokenP(TOKEN_SYMBOL_COLON);
    except_handlers_init(&parent->try_catch.handlers);
    parent->try_catch.else_body = NULL;
    parent->try_catch.finally_body = NULL;
    parent->try_catch.is_star = false;
    parent->try_catch.try_body =
            parse_statement_group_child(parent, spaces_parent);

    while (!tokens_over()) {
        token tok = token_peek(0);
        size_t tok_type = tok.type;

        equal_indent_group_macro_thing(tok_type, tok, spaces_parent)

        if (tok_type == TOKEN_KEYWORD_EXCEPT) {
            if (parent->try_catch.else_body != NULL ||
                parent->try_catch.finally_body != NULL)
                syntax_error();
            pyc_ti++;
            if (tokens_over())
                syntax_error();
            token t2 = token_peek(0);
            if (t2.type == TOKEN_OPERATOR_MUL) {
                if (!parent->try_catch.is_star) {
                    if (parent->try_catch.handlers.size > 0)
                        raise_error("Syntax Error: try statement cannot "
                                    "contain both except and except*");
                    parent->try_catch.is_star = true;
                }

                pyc_ti++;
            } else {
                if (parent->try_catch.is_star)
                    raise_error("Syntax Error: try statement cannot contain "
                                "both except and except*");
            }

            except_handler handler;
            handler.class = parse_expression_child(parent);
            if (tokens_over())
                syntax_error();

            if (tok_now_teq(TOKEN_KEYWORD_AS)) {
                pyc_ti++;
                token *tok2;
                if (tokens_over() ||
                    ((tok2 = &token_peek(0))->type & 0xf) != TOKEN_IDENTIFIER)
                    syntax_error();
                handler.name = tok2;
                pyc_ti++;
            } else {
                handler.name = NULL;
            }

            assert_tokenP(TOKEN_SYMBOL_COLON);
            handler.body = parse_statement_group_child(parent, spaces_parent);
            except_handlers_push(&parent->try_catch.handlers, handler);
        } else if (tok_type == TOKEN_KEYWORD_ELSE) {
            if (parent->try_catch.else_body != NULL ||
                parent->try_catch.finally_body != NULL)
                syntax_error();
            pyc_ti++;
            assert_tokenP(TOKEN_SYMBOL_COLON);
            parent->try_catch.else_body =
                    parse_statement_group_child(parent, spaces_parent);
        } else if (tok_type == TOKEN_KEYWORD_FINALLY) {
            if (parent->try_catch.finally_body != NULL)
                syntax_error();
            pyc_ti++;
            assert_tokenP(TOKEN_SYMBOL_COLON);
            parent->try_catch.finally_body =
                    parse_statement_group_child(parent, spaces_parent);
        } else break;
    }

    if (parent->try_catch.handlers.size == 0 &&
        parent->try_catch.else_body != NULL &&
        parent->try_catch.finally_body != NULL) {
        raise_error("Expected an except handler");
    }

    except_handlers_shrink(&parent->try_catch.handlers);
}

void parse_statements(node *st) {
    if (pyc_tok_count == 0) {
        st->type = NODE_GROUP;
        nodes_init(&st->group.v);
        return;
    }

    parse_statement_group(st, 0);
}

void parse_statement_for(node *st, int spaces_parent) {
    bool is_async = tok_now_teq(TOKEN_KEYWORD_ASYNC);
    pyc_ti++;
    if (is_async) {
        if (tokens_over() || !tok_now_teq(TOKEN_KEYWORD_FOR))
            syntax_error();
        pyc_ti++;
    } else if (!tok_peek_teq(-1, TOKEN_KEYWORD_FOR))
        syntax_error();

    st->type = STMT_FOR;
    st->for_loop.is_async = is_async;

    size_t in_token = get_node_tok_index(TOKEN_OPERATOR_IN, true, false, false);
    if (in_token == -1) {
        syntax_error();
    }

    pyc_tok_count = in_token;
    st->for_loop.value = parse_expression_child(st);
    pyc_tok_count = pyc_tokens.size;
    assert_tokenP(TOKEN_OPERATOR_IN);
    st->for_loop.iter = parse_expression_child(st);
    assert_tokenP(TOKEN_SYMBOL_COLON);
    st->for_loop.body = parse_statement_group_child(st, spaces_parent);

    token tok = token_peek(0);
    size_t tok_type = tok.type;
    do {
        equal_indent_group_macro_thing(tok_type, tok, spaces_parent)
    } while (0);

    if (tok_type == TOKEN_KEYWORD_ELSE) {
        pyc_ti++;
        assert_tokenP(TOKEN_SYMBOL_COLON);
        st->for_loop.else_body =
                parse_statement_group_child(st, spaces_parent);
    } else {
        st->for_loop.else_body = NULL;
    }
}

void parse_statement_while(node *st, int spaces_parent) {
    st->type = STMT_WHILE;
    pyc_ti++;
    st->while_loop.cond = parse_expression_group_child(st);
    assert_tokenP(TOKEN_SYMBOL_COLON);
    st->while_loop.body = parse_statement_group_child(st, spaces_parent);
}

void parse_statement_with(node *st, int spaces_parent) {
    bool is_async = tok_now_teq(TOKEN_KEYWORD_ASYNC);
    pyc_ti++;
    if (is_async) {
        if (tokens_over() || !tok_now_teq(TOKEN_KEYWORD_WITH))
            syntax_error();
        pyc_ti++;
    } else if (!tok_peek_teq(-1, TOKEN_KEYWORD_WITH))
        syntax_error();

    st->type = STMT_WITH;
    st->with.is_async = is_async;
    tokens_p_init(&st->with.vars);
    nodes_init(&st->with.contexts);

    while (!tokens_over()) {
        node *context = parse_expression_child(st);
        nodes_push(&st->with.contexts, context);
        if (tokens_over())
            syntax_error();
        token t = token_peek(0);
        if (t.type == TOKEN_SYMBOL_COLON)
            break;
        if (t.type == TOKEN_SYMBOL_COMMA) {
            pyc_ti++;
            tokens_p_push(&st->with.vars, NULL);
            continue;
        }

        if (!tok_now_teq(TOKEN_KEYWORD_AS))
            syntax_error();
        pyc_ti++;

        token *tok_p = &token_peek(0);
        token tok = *tok_p;
        if ((tok.type & 0xf) != TOKEN_IDENTIFIER)
            syntax_error();
        tokens_p_push(&st->with.vars, tok_p);
    }

    if (tokens_over() || token_peek(0).type != TOKEN_SYMBOL_COLON ||
        st->with.contexts.size == 0)
        syntax_error();
    pyc_ti++;
    st->with.body = parse_statement_group_child(st, spaces_parent);

    nodes_shrink(&st->with.contexts);
    tokens_p_shrink(&st->with.vars);
}

void parse_statement_if(node *st, int spaces_parent) {
    st->type = STMT_IF;
    pyc_ti++;
    nodes_init(&st->if_stmt.conditions);
    nodes_init(&st->if_stmt.bodies);

    nodes_push(&st->if_stmt.conditions, parse_expression_child(st));
    assert_tokenP(TOKEN_SYMBOL_COLON);
    nodes_push(&st->if_stmt.bodies, parse_statement_group_child(st, spaces_parent));

    while (!tokens_over()) {
        token tok = token_peek(0);
        size_t tok_type = tok.type;

        equal_indent_group_macro_thing(tok_type, tok, spaces_parent)

        if (tok_type == TOKEN_KEYWORD_ELIF) {
            pyc_ti++;
            nodes_push(&st->if_stmt.conditions, parse_expression_child(st));
            assert_tokenP(TOKEN_SYMBOL_COLON);
            nodes_push(&st->if_stmt.bodies,
                       parse_statement_group_child(st, spaces_parent));
            continue;
        }

        if (tok_type == TOKEN_KEYWORD_ELSE) {
            pyc_ti++;
            assert_tokenP(TOKEN_SYMBOL_COLON);
            nodes_push(&st->if_stmt.conditions, NULL);
            nodes_push(&st->if_stmt.bodies, parse_statement_group_child(st, spaces_parent));
            break;
        }

        break;
    }

    nodes_shrink(&st->if_stmt.conditions);
    nodes_shrink(&st->if_stmt.bodies);
}

void parse_statement_function_def(node *st, int spaces_parent) {
    bool is_async = tok_now_teq(TOKEN_KEYWORD_ASYNC);
    pyc_ti++;
    if (is_async) {
        if (tokens_over() || !tok_now_teq(TOKEN_KEYWORD_DEF))
            syntax_error();
        pyc_ti++;
    } else if (!tok_peek_teq(-1, TOKEN_KEYWORD_DEF))
        syntax_error();

    st->type = STMT_FUNCTION_DEF;
    nodes *decorators = &st->function_def.decorators;
    decorators->size = 0;
    decorators->capacity = 0;
    decorators->data = NULL;

    st->function_def.is_async = is_async;
    token *namePtr = &token_peek(0);

    if ((namePtr->type & 0xf) != TOKEN_IDENTIFIER)
        syntax_error();
    st->function_def.name = namePtr;
    pyc_ti++;

    bool p = paren;
    paren = true;

    assert_tokenP(TOKEN_SYMBOL_LPAREN);

    st->function_def.ls_args = NULL;
    st->function_def.kw_args = NULL;

    tokens_p *args = &st->function_def.args;
    nodes *defaults = &st->function_def.args_defaults;
    tokens_p_init(args);
    nodes_init(defaults);
    while (!tokens_over()) {
        token *tok2_p = &token_peek(0);
        token tok2 = *tok2_p;
        size_t tok2_type = tok2.type;

        if (tok2.type == TOKEN_SYMBOL_RPAREN)
            break;

        if (tok2.type == TOKEN_LINE_BREAK_NEWLINE) {
        }

        bool is_ls = tok2_type == TOKEN_OPERATOR_MUL;
        bool is_kw = tok2_type == TOKEN_OPERATOR_POW;

        if (is_ls) {
            pyc_ti++;
            if (tokens_over() || st->function_def.ls_args != NULL)
                syntax_error();
            st->function_def.ls_args = &token_peek(0);
            continue;
        } else if (is_kw) {
            pyc_ti++;
            if (tokens_over() || st->function_def.kw_args != NULL)
                syntax_error();
            st->function_def.kw_args = &token_peek(0);
            continue;
        }

        if (tok2.type == TOKEN_LINE_BREAK_NEWLINE) {
            pyc_ti++;
            continue;
        }

        if ((tok2.type & 0xf) != TOKEN_IDENTIFIER)
            syntax_error();

        for (size_t i = 0; i < args->size; i++) {
            if (args->data[i]->type == tok2_type) {
                raise_error_t(tok2, "SyntaxError: duplicate argument 'a' in function definition");
            }
        }

        tokens_p_push(args, tok2_p);

        pyc_ti++;

        if (tokens_over())
            syntax_error();

        if (token_peek(0).type == TOKEN_SYMBOL_COLON) {
            pyc_ti++;
            parser_ignore_type();
        }

        if (tokens_over())
            syntax_error();

        if (token_peek(0).type == TOKEN_SET_OPERATOR_EQ) {
            pyc_ti++;
            nodes_push(defaults, parse_expression_group_child(st));
        } else
            nodes_push(defaults, NULL);

        token t2;
        if (tokens_over() || ((t2 = token_peek(0)).type != TOKEN_SYMBOL_COMMA &&
                              t2.type != TOKEN_SYMBOL_RPAREN))
            syntax_error();

        if (t2.type == TOKEN_SYMBOL_RPAREN)
            break;
        pyc_ti++;
    }

    assert_tokenP(TOKEN_SYMBOL_RPAREN);

    paren = p;

    token t2 = token_peek(0);
    if (t2.type == TOKEN_OPERATOR_SUB) {
        pyc_ti++;
        if (tokens_over() || token_peek(0).type != TOKEN_OPERATOR_GT)
            syntax_error();
        pyc_ti++;
        node *ex = parse_expression_group_child(st);
        node_free(ex);
        if (tokens_over())
            syntax_error();
        t2 = token_peek(0);
    }

    if (t2.type != TOKEN_SYMBOL_COLON)
        syntax_error();
    pyc_ti++;

    st->function_def.body = parse_statement_group_child(st, spaces_parent);
}

void parse_statement_class(node *st, int spaces) {
    st->type = STMT_CLASS_DEF;

    nodes *dec = &st->class_def.decorators;
    dec->size = 0;
    dec->capacity = 0;
    dec->data = NULL;

    pyc_ti++;
    if (tokens_over())
        syntax_error();
    token *tokName = &token_peek(0);
    if ((tokName->type & 0xf) != TOKEN_IDENTIFIER)
        syntax_error();
    pyc_ti++;
    st->class_def.name = tokName;

    tokens_p *extends = &st->class_def.extends;
    tokens_p_init(extends);
    nodes_init(&st->class_def.methods);
    nodes_init(&st->class_def.properties);

    bool p = paren;
    paren = true;

    if (tokens_over())
        syntax_error();
    if (token_peek(0).type == TOKEN_SYMBOL_LPAREN) {
        pyc_ti++;
        while (!tokens_over()) {
            token *tok2_p = &token_peek(0);
            token tok2 = *tok2_p;

            if (tok2.type == TOKEN_SYMBOL_RPAREN)
                break;

            if (tok2.type == TOKEN_LINE_BREAK_NEWLINE) {
                pyc_ti++;
                continue;
            }

            if ((tok2.type & 0xf) != TOKEN_IDENTIFIER)
                syntax_error();

            tokens_p_push(extends, tok2_p);

            pyc_ti++;

            if (tokens_over())
                syntax_error();

            token t2;
            if (tokens_over() ||
                ((t2 = token_peek(0)).type != TOKEN_SYMBOL_COMMA &&
                 t2.type != TOKEN_SYMBOL_RPAREN))
                syntax_error();

            if (t2.type == TOKEN_SYMBOL_RPAREN)
                break;
            pyc_ti++;
        }

        assert_tokenP(TOKEN_SYMBOL_RPAREN);
    }

    paren = p;

    assert_tokenP(TOKEN_SYMBOL_COLON);

    // no need to allocate a node because we are in a class_def
    node temp = {.parent = st};
    parse_statement_group(&temp, spaces);
    nodes_shrink(&st->class_def.methods);
    nodes_shrink(&st->class_def.properties);
    tokens_p_shrink(extends);
}

int parse_import_level() {
    int level = 0;
    while (!tokens_over() && token_peek(0).type == TOKEN_SYMBOL_DOT) {
        level++;
        pyc_ti++;
    }

    return level;
}

void parse_import_path(tokens_p *ls) {
    tokens_p_init(ls);
    while (!tokens_over()) {
        token *tok_p = &token_peek(0);
        token tok = *tok_p;

        if ((tok.type & 0xf) != TOKEN_IDENTIFIER)
            break;

        tokens_p_push(ls, tok_p);
        pyc_ti++;

        if (tokens_over())
            break;

        token t = token_peek(0);
        if (t.type == TOKEN_SYMBOL_DOT) {
            pyc_ti++;
            continue;
        }

        if (t.type == TOKEN_SYMBOL_COMMA)
            break;
    }

    if (ls->size == 0) {
        raise_error("Expected an import path");
    }
}

void parse_import_aliases(import_aliases *imports) {
    import_aliases_init(imports);

    bool par = tok_now_teq(TOKEN_SYMBOL_LPAREN);
    if (par)
        pyc_ti++;

    while (!tokens_over()) {
        import_alias alias;
        token *tok_p = &token_peek(0);

        if (par) {
            if (tok_p->type == TOKEN_LINE_BREAK_NEWLINE) {
                pyc_ti++;
                continue;
            } else if (tok_p->type == TOKEN_SYMBOL_RPAREN) {
                break;
            }
        }

        if ((tok_p->type & 0xf) != TOKEN_IDENTIFIER) {
            syntax_error();
        }

        pyc_ti++;

        alias.tok = tok_p;

        if (!tokens_over() && tok_now_teq(TOKEN_KEYWORD_AS)) {
            pyc_ti++;
            if (tokens_over())
                syntax_error();
            tok_p = &token_peek(0);
            if ((tok_p->type & 0xf) != TOKEN_IDENTIFIER)
                syntax_error();
            alias.as = tok_p;
            pyc_ti++;
        } else {
            alias.as = NULL;
        }

        import_aliases_push(imports, alias);

        token tok;

        if (tokens_over() ||
            (tok = token_peek(0)).type == TOKEN_LINE_BREAK_SEMICOLON ||
            (!par && tok.type == TOKEN_LINE_BREAK_NEWLINE))
            break;

        if (tok.type == TOKEN_SYMBOL_COMMA) {
            pyc_ti++;
            continue;
        }
    }

    if (par) {
        assert_tokenP(TOKEN_SYMBOL_RPAREN);
    }
}

void parse_import_libs(import_libs *imports) {
    import_libs_init(imports);

    while (!tokens_over()) {
        import_lib alias;
        parse_import_path(&alias.tok);

        if (!tokens_over() && tok_now_teq(TOKEN_KEYWORD_AS)) {
            pyc_ti++;
            if (tokens_over())
                syntax_error();
            token *tok_p = &token_peek(0);
            if ((tok_p->type & 0xf) != TOKEN_IDENTIFIER)
                syntax_error();
            alias.as = tok_p;
            pyc_ti++;
        } else {
            alias.as = NULL;
        }

        import_libs_push(imports, alias);

        if (tokens_over() || token_type_peek_t(0) == TOKEN_LINE_BREAK)
            break;

        if (token_peek(0).type == TOKEN_SYMBOL_COMMA) {
            pyc_ti++;
            continue;
        }
    }
}

void parse_statement_import(node *st, int spaces) {
    st->type = STMT_IMPORT;
    pyc_ti++;
    parse_import_libs(&st->imports.v);
}

void parse_statement_import_from(node *st, int spaces) {
    st->type = STMT_IMPORT_FROM;
    pyc_ti++;
    st->import_from.level = parse_import_level();
    parse_import_path(&st->import_from.lib);
    assert_tokenP(TOKEN_KEYWORD_IMPORT);
    if (tokens_over())
        syntax_error();
    parse_import_aliases(&st->import_from.imports);
}

void parse_statement_raise(node *st, int spaces) {
    st->type = STMT_RAISE;
    pyc_ti++;

    if (tokens_over() || token_type_peek_t(0) == TOKEN_LINE_BREAK) {
        st->raise.exception = NULL;
        st->raise.cause = NULL;
        return;
    }

    if (!tok_now_teq(TOKEN_KEYWORD_FROM))
        st->raise.exception = parse_expression_child(st);

    if (tokens_over() || !tok_now_teq(TOKEN_KEYWORD_FROM)) {
        st->raise.cause = NULL;
        return;
    }

    pyc_ti++;
    st->raise.cause = parse_expression_child(st);

    if (!tokens_over())
        assert_token_type_t(TOKEN_LINE_BREAK);
}

// handles both
void parse_statement_global_nonlocal(node *st, int spaces) {
    bool is_global = tok_now_teq(TOKEN_KEYWORD_GLOBAL);
    st->type = is_global ? STMT_GLOBAL : STMT_NONLOCAL;
    pyc_ti++;
    tokens_p *ls = is_global ? &st->global : &st->nonlocal;
    tokens_p_init(ls);
    while (!tokens_over()) {
        token *tok2 = &token_peek(0);
        if ((tok2->type & 0xf) != TOKEN_IDENTIFIER)
            syntax_error();
        tokens_p_push(ls, tok2);
        pyc_ti++;
        if (tokens_over())
            break;
        if (token_peek(0).type == TOKEN_SYMBOL_COMMA) {
            pyc_ti++;
            if (tokens_over())
                syntax_error();
            continue;
        }

        if (token_type_peek_t(0) == TOKEN_LINE_BREAK)
            break;
    }

    if (!tokens_over())
        assert_token_type_t(TOKEN_LINE_BREAK);
}

void parse_statement_assert(node *st, int spaces) {
    st->type = STMT_ASSERT;
    pyc_ti++;
    st->assert.condition = parse_expression_group_child(st);
    if (tokens_over()) {
        st->assert.message = NULL;
        return;
    }

    if (token_peek(0).type == TOKEN_SYMBOL_COMMA) {
        pyc_ti++;
        if (tokens_over())
            syntax_error();
        st->assert.message = parse_expression_group_child(st);
    } else {
        st->assert.message = NULL;
    }

    assert_token_type_t(TOKEN_LINE_BREAK);
}

void parse_statement_match(node *st, int spaces) {
    st->type = STMT_MATCH;
    pyc_ti++;
    st->match.subject = parse_expression_child(st);
    assert_tokenP(TOKEN_SYMBOL_COLON);
    parse_statement_group_match_def(st, spaces);
}

void parse_statement_try(node *st, int spaces) {
    st->type = STMT_TRY_CATCH;
    pyc_ti++;
    if (tokens_over())
        syntax_error();
    parse_statement_group_try_catch(st, spaces);
}

void parse_decorators(node *parent, int spaces_parent) {
    nodes decorators;
    nodes_init(&decorators);

    while (!tokens_over()) {
        token tok = token_peek(0);
        size_t tok_type = tok.type;
        size_t tok_type_t = tok.type & 0xf;

        equal_indent_group_macro_thing(tok_type, tok, spaces_parent)

        if (tok_type != TOKEN_OPERATOR_MAT_MUL) break;

        pyc_ti++;
        nodes_push(&decorators, parse_expression_group_child(parent));
    }

    nodes_shrink(&decorators);

    parse_statement_next(parent, spaces_parent);
    if (parent->type == STMT_CLASS_DEF)
        parent->class_def.decorators = decorators;
    else if (parent->type == STMT_FUNCTION_DEF)
        parent->function_def.decorators = decorators;
    else {
        raise_error("expected a type or function definition after "
                    "decorators");
    }
}

void parse_statement_next(node *st, int spaces) {
    token *tok_p = &token_peek(0);
    token tok = *tok_p;
    size_t tok_type = tok.type;
    size_t tok_type_t = tok_type & 0xf;

    if (tok.type == TOKEN_OPERATOR_MAT_MUL) {
        parse_decorators(st, spaces);
        return;
    }

    if (tok_type == TOKEN_KEYWORD_RETURN) {
        pyc_ti++;
        st->type = STMT_RETURN;
        if (tokens_over() || token_type_peek_t(0) == TOKEN_LINE_BREAK) {
            st->ret.v = NULL;
            return;
        }

        st->ret.v = parse_expression_child(st);
        if (!tokens_over() && token_type_peek_t(0) != TOKEN_LINE_BREAK)
            syntax_error();
        return;
    } else if (tok_type == TOKEN_KEYWORD_BREAK) {
        pyc_ti++;
        st->type = STMT_BREAK;
        if (!tokens_over() && token_type_peek_t(0) != TOKEN_LINE_BREAK)
            syntax_error();
        return;
    } else if (tok_type == TOKEN_KEYWORD_CONTINUE) {
        pyc_ti++;
        st->type = STMT_CONTINUE;
        if (!tokens_over() && token_type_peek_t(0) != TOKEN_LINE_BREAK)
            syntax_error();
        return;
    } else if (tok_type == TOKEN_KEYWORD_PASS) {
        pyc_ti++;
        st->type = STMT_PASS;
        if (!tokens_over() && token_type_peek_t(0) != TOKEN_LINE_BREAK)
            syntax_error();
        return;
    } else if (tok_type == TOKEN_KEYWORD_DEL) {
        pyc_ti++;
        st->type = STMT_DELETE;
        node *del = st->del = parse_expression_child(st);
        if (del->type != EXPR_INDEX && del->type != EXPR_ATTRIBUTE &&
            del->type != EXPR_IDENTIFIER)
            syntax_error();
        return;
    } else if (tok_type == TOKEN_KEYWORD_ASYNC ||
               tok_type == TOKEN_KEYWORD_DEF || tok_type == TOKEN_KEYWORD_FOR ||
               tok_type == TOKEN_KEYWORD_WITH) {
        bool is_async = tok_type == TOKEN_KEYWORD_ASYNC;

        if (tok_type == TOKEN_KEYWORD_DEF) {
            parse_statement_function_def(st, spaces);
        } else if (tok_type == TOKEN_KEYWORD_FOR) {
            parse_statement_for(st, spaces);
        } else if (tok_type == TOKEN_KEYWORD_WITH) {
            parse_statement_with(st, spaces);
        } else {
            syntax_error();
        }
        return;
    } else if (tok_type == TOKEN_KEYWORD_WHILE) {
        parse_statement_while(st, spaces);
        return;
    } else if (tok_type == TOKEN_KEYWORD_IF) {
        parse_statement_if(st, spaces);
        return;
    } else if (tok_type == TOKEN_KEYWORD_CLASS) {
        parse_statement_class(st, spaces);
        return;
    } else if (tok_type == TOKEN_KEYWORD_FROM) {
        parse_statement_import_from(st, spaces);
        return;
    } else if (tok_type == TOKEN_KEYWORD_IMPORT) {
        parse_statement_import(st, spaces);
        return;
    } else if (tok_type == TOKEN_KEYWORD_ASSERT) {
        parse_statement_assert(st, spaces);
        return;
    } else if (tok_type == TOKEN_KEYWORD_GLOBAL ||
               tok_type == TOKEN_KEYWORD_NONLOCAL) {
        parse_statement_global_nonlocal(st, spaces);
        return;
    } else if (tok_type == TOKEN_KEYWORD_RAISE) {
        parse_statement_raise(st, spaces);
        return;
    } else if (tok_type == TOKEN_KEYWORD_MATCH) {
        parse_statement_match(st, spaces);
        return;
    } else if (tok_type == TOKEN_KEYWORD_TRY) {
        parse_statement_try(st, spaces);
        return;
    }

    node *expr = parse_expression_child(st);

    tok_p = &token_peek(0);
    tok = *tok_p;
    tok_type = tok.type;
    tok_type_t = tok_type & 0xf;
    if (tok_type == TOKEN_SYMBOL_COLON || tok_type_t == TOKEN_SET_OPERATOR) {
        st->type = STMT_ASSIGN;
        st->assign.var = expr;

        if (tok_type == TOKEN_SYMBOL_COLON) {
            pyc_ti++;
            parser_ignore_type();

            tok_p = &token_peek(0);
            tok = *tok_p;
            tok_type_t = tok.type & 0xf;
            if (tokens_over() || tok_type_t != TOKEN_SET_OPERATOR) {
                st->assign.val = NULL;
                st->assign.set_op = NULL;
                return;
            }

            pyc_ti++;
        } else {
            pyc_ti++;
            size_t second_eq =
                    get_node_tok_index(TOKEN_SET_OPERATOR_EQ, true, false, false);
            if (second_eq != -1) {
                st->type = STMT_ASSIGN_MULT;
                nodes_init(&st->assign_mult.targets);
                nodes_push(&st->assign_mult.targets, expr);

                while (!tokens_over()) {
                    expr = parse_expression_child(st);
                    if (tokens_over() ||
                        token_peek(0).type != TOKEN_SET_OPERATOR_EQ) {
                        st->assign_mult.val = expr;
                        break;
                    }

                    nodes_push(&st->assign_mult.targets, expr);
                    pyc_ti++;
                }
                return;
            }
        }

        st->assign.set_op = tok_p;
        st->assign.val = parse_expression_child(st);
        return;
    }

    st->type = STMT_EXPR;
    st->stmt_expr = expr;
}

token *get_node_token(node *node) {
    if (node == NULL) return NULL;
    token *x;
    switch (node->type) {
        case NODE_MODULE:
            return NULL;
        case NODE_GROUP:
            return node->group.tok;
        case EXPR_IDENTIFIER:
            return node->identifier;
            break;
        case EXPR_CONSTANT:
            return node->constant;
            break;
        case EXPR_FORMAT_STRING:
            return node->fstring.start_string;
        case EXPR_OPERATOR:
            return node->operator;
        case EXPR_BINARY_OPERATION:
            return get_node_token(node->bin_op.left);
        case EXPR_UNARY_OPERATION:
            return get_node_token(node->unary_op.expr);
        case EXPR_CMP_OPERATION:
            return get_node_token(node->cmp_op.ex.data[0]);
        case EXPR_WALRUS_OPERATION:
            return get_node_token(node->walrus_op.left);
        case EXPR_LIST_COMP:
            return node->list_comp.tok;
        case EXPR_LIST:
            return node->list.tok;
        case EXPR_DICT_COMP:
            return node->dict_comp.tok;
        case EXPR_DICT:
            return node->dict.tok;
        case EXPR_SET_COMP:
            return node->set_comp.tok;
        case EXPR_SET:
            return node->set.tok;
        case EXPR_TUPLE:
            return node->tuple.tok;
        case EXPR_LAMBDA:
            return node->lambda.tok;
            break;
        case EXPR_IF_EXP:
            return get_node_token(node->if_else_expr.condition);
        case EXPR_GENERATOR:
            return get_node_token(node->generator.value);
        case EXPR_AWAIT:
            return get_node_token(node->await);
        case EXPR_YIELD:
            return get_node_token(node->yield);
        case EXPR_YIELD_FROM:
            return get_node_token(node->yield_from);
        case EXPR_CALL:
            return get_node_token(node->call.base);
        case EXPR_ATTRIBUTE:
            return get_node_token(node->attribute.base);
        case EXPR_INDEX:
            return get_node_token(node->index.base);
        case STMT_FUNCTION_DEF:
            return node->function_def.name;
        case STMT_CLASS_DEF:
            return node->class_def.name;
        case STMT_RETURN:
            return node->ret.tok;
        case STMT_DELETE:
            return get_node_token(node->del);
        case STMT_ASSIGN:
            return get_node_token(node->assign.var);
        case STMT_ASSIGN_MULT:
            return get_node_token(node->assign_mult.targets.data[0]);
        case STMT_FOR:
            return node->for_loop.tok;
        case STMT_WHILE:
            return node->while_loop.tok;
        case STMT_IF:
            return node->if_stmt.tok;
        case STMT_WITH:
            return node->with.tok;
        case STMT_MATCH:
            return node->match.tok;
        case STMT_RAISE:
            return node->raise.tok;
        case STMT_TRY_CATCH:
            return node->try_catch.tok;
        case STMT_ASSERT:
            return node->assert.tok;
        case STMT_IMPORT:
            return node->imports.tok;
        case STMT_IMPORT_FROM:
            return node->import_from.tok;
        case STMT_GLOBAL:
            return node->global.data[0];
        case STMT_NONLOCAL:
            return node->nonlocal.data[0];
        case STMT_EXPR:
            return get_node_token(node->stmt_expr);
        case STMT_PASS:
            return node->pass;
        case STMT_BREAK:
            return node->break_st;
        case STMT_CONTINUE:
            return node->continue_st;
    }

    return NULL;
}

#ifdef NEED_AST_PRINT

void node_print(node *node, int indent) {
    if (node == NULL) {
        printf("node(NULL)");
        return;
    }

    if (node->type == EXPR_IDENTIFIER) {
        printf("I(");
        token_p_print(node->identifier, indent + 1);
        printf(")");
        return;
    }

    printf("node(type=");

    switch (node->type) {
        case NODE_MODULE:
            printf("module, filename=%s, code_len=%ld", node->module.filename, node->module.code_len);
            break;
        case NODE_GROUP:
            printf("group, value=");
            nodes_print_indent(node->group.v, indent + 1);
            break;
        case EXPR_IDENTIFIER:
            printf("identifier, value=");
            token_p_print(node->identifier, indent + 1);
            break;
        case EXPR_CONSTANT:
            printf("constant, value=");
            token_p_print(node->constant, indent + 1);
            break;
        case EXPR_OPERATOR:
            printf("operator, value=");
            token_p_print(node->operator, indent + 1);
            break;
        case EXPR_BINARY_OPERATION:
            printf("binary_operation, operator");
            token_p_print(node->bin_op.op, indent + 1);
            printf(", left=");
            node_print(node->bin_op.left, indent + 1);
            printf(", right=");
            node_print(node->bin_op.right, indent + 1);
            break;
        case EXPR_UNARY_OPERATION:
            printf("unary_operation, operator=");
            token_p_print(node->unary_op.op, indent + 1);
            printf(", value=");
            node_print(node->unary_op.expr, indent + 1);
            break;
        case EXPR_CMP_OPERATION:
            printf("cmp_operation, operator=");
            tokens_p_print_indent(node->cmp_op.op, indent + 1);
            printf(", values=");
            nodes_print_indent(node->cmp_op.ex, indent + 1);
            break;
        case EXPR_WALRUS_OPERATION:
            printf("walrus_operation, left=");
            node_print(node->walrus_op.left, indent + 1);
            printf(", right=");
            node_print(node->walrus_op.right, indent + 1);
            break;
        case EXPR_FORMAT_STRING:
            printf("format_string, start_string=");
            token_p_print(node->fstring.start_string, indent + 1);
            printf(", values=");
            nodes_print_indent(node->fstring.values, indent + 1);
            printf(", strings=");
            tokens_p_print_indent(node->fstring.strings, indent + 1);
            printf(", extras=");
            nodes_print_indent(node->fstring.extras, indent + 1);
            break;
        case EXPR_LIST_COMP:
            printf("list_comp, value=");
            node_print(node->list_comp.value, indent);
            printf(", comp=");
            comprehensions_print_indent(node->list_comp.comp, indent + 1);
            break;
        case EXPR_LIST:
            printf("list, value=");
            nodes_print_indent(node->list.v, indent + 1);
            break;
        case EXPR_DICT_COMP:
            printf("dict_comp, key=");
            node_print(node->dict_comp.key, indent);
            printf(", value=");
            node_print(node->dict_comp.value, indent);
            printf(", comp=");
            comprehensions_print_indent(node->dict_comp.comp, indent + 1);
            break;
        case EXPR_DICT:
            printf("dict, keys=");
            nodes_print_indent(node->dict.keys, indent + 1);
            printf(", values=");
            nodes_print_indent(node->dict.values, indent + 1);
            break;
        case EXPR_SET_COMP:
            printf("set_comp, value=");
            node_print(node->set_comp.value, indent);
            printf(", comp=");
            comprehensions_print_indent(node->set_comp.comp, indent + 1);
            break;
        case EXPR_SET:
            printf("set, value=");
            nodes_print_indent(node->set.v, indent + 1);
            break;
        case EXPR_TUPLE:
            printf("tuple, value=");
            nodes_print_indent(node->tuple.v, indent + 1);
            break;
        case EXPR_LAMBDA:
            printf("lambda, args=");
            tokens_p_print_indent(node->lambda.args, indent + 1);
            printf(", body=");
            node_print(node->lambda.body, indent);
            break;
        case EXPR_IF_EXP:
            printf("if_exp, if_expr=");
            node_print(node->if_else_expr.if_expr, indent);
            printf(", condition=");
            node_print(node->if_else_expr.condition, indent);
            printf(", else_expr=");
            node_print(node->if_else_expr.else_expr, indent);
            break;
        case EXPR_GENERATOR:
            printf("generator, value=");
            node_print(node->generator.value, indent);
            printf(", comp=");
            comprehensions_print_indent(node->generator.comp, indent + 1);
            break;
        case EXPR_AWAIT:
            printf("await, value=");
            node_print(node->await, indent);
            break;
        case EXPR_YIELD:
            printf("yield, value=");
            node_print(node->yield, indent);
            break;
        case EXPR_YIELD_FROM:
            printf("yield_from, value=");
            node_print(node->yield_from, indent);
            break;
        case EXPR_CALL:
            printf("call, base=");
            node_print(node->call.base, indent);
            printf(", args=");
            nodes_print_indent(node->call.args, indent + 1);
            printf(", kws=");
            keywords_print_indent(node->call.kws, indent + 1);
            break;
        case EXPR_ATTRIBUTE:
            printf("attribute, base=");
            node_print(node->attribute.base, indent);
            printf(", key=");
            token_p_print(node->attribute.key, indent + 1);
            break;
        case EXPR_INDEX:
            printf("index, base=");
            node_print(node->index.base, indent);
            printf(", slices=[");
            putchar('\n');
            for (int i = 0; i < 3; i++) {
                _VCINDENT(indent + 1);
                node_print(node->index.slices[i], indent);
                if (i != 2) {
                    putchar(',');
                }
                putchar('\n');
            }
            _VCINDENT(indent);
            putchar(']');
            break;
        case STMT_FUNCTION_DEF:
            printf("function_def, name=");
            token_p_print(node->function_def.name, indent + 1);
            printf(", args=");
            tokens_p_print_indent(node->function_def.args, indent + 1);
            printf(", body=");
            node_print(node->function_def.body, indent + 1);
            printf(", is_async=%s",
                   node->function_def.is_async ? "true" : "false");
            printf(", decorators=");
            nodes_print_indent(node->function_def.decorators, indent + 1);
            break;
        case STMT_CLASS_DEF:
            printf("class_def, name=");
            token_p_print(node->class_def.name, indent + 1);
            printf(", extends=");
            tokens_p_print_indent(node->class_def.extends, indent + 1);
            printf(", methods=");
            nodes_print_indent(node->class_def.methods, indent + 1);
            printf(", properties=");
            nodes_print_indent(node->class_def.properties, indent + 1);
            printf(", decorators=");
            nodes_print_indent(node->class_def.decorators, indent + 1);
            break;
        case STMT_RETURN:
            printf("return, value=");
            node_print(node->ret.v, indent);
            break;
        case STMT_DELETE:
            printf("delete, value=");
            node_print(node->del, indent);
            break;
        case STMT_ASSIGN:
            printf("assign, var=");
            node_print(node->assign.var, indent);
            printf(", set_op=");
            token_p_print(node->assign.set_op, indent + 1);
            printf(", val=");
            node_print(node->assign.val, indent);
            break;
        case STMT_ASSIGN_MULT:
            printf("assign_multiple, var=");
            nodes_print_indent(node->assign_mult.targets, indent + 1);
            printf(", val=");
            node_print(node->assign_mult.val, indent);
            break;
        case STMT_FOR:
            printf("for, is_async=%s",
                   node->for_loop.is_async ? "true" : "false");
            printf(", value=");
            node_print(node->for_loop.value, indent);
            printf(", iter=");
            node_print(node->for_loop.iter, indent);
            printf(", body=");
            node_print(node->for_loop.body, indent + 1);
            break;
        case STMT_WHILE:
            printf("while, condition=");
            node_print(node->while_loop.cond, indent);
            printf(", body=");
            node_print(node->while_loop.body, indent + 1);
            break;
        case STMT_IF:
            printf("if, conditions=");
            nodes_print_indent(node->if_stmt.conditions, indent + 1);
            printf(", bodies=");
            nodes_print_indent(node->if_stmt.bodies, indent + 1);
            break;
        case STMT_WITH:
            printf("with, is_async=%s", node->with.is_async ? "true" : "false");
            printf(", vars=");
            tokens_p_print_indent(node->with.vars, indent + 1);
            printf(", contexts=");
            nodes_print_indent(node->with.contexts, indent + 1);
            printf(", body=");
            node_print(node->with.body, indent + 1);
            break;
        case STMT_MATCH:
            printf("match, subject=");
            node_print(node->match.subject, indent);
            printf(", cases=");
            match_cases_print_indent(node->match.cases, indent + 1);
            break;
        case STMT_RAISE:
            printf("raise, exception=");
            node_print(node->raise.exception, indent);
            printf(", cause=");
            node_print(node->raise.cause, indent);
            break;
        case STMT_TRY_CATCH:
            printf("try_catch, is_star=%s, try=",
                   node->try_catch.is_star ? "true" : "false");
            node_print(node->try_catch.try_body, indent + 1);
            printf(", excepts=");
            except_handlers_print_indent(node->try_catch.handlers, indent + 1);
            printf(", else=");
            node_print(node->try_catch.else_body, indent);
            printf(", finally=");
            node_print(node->try_catch.finally_body, indent);
            break;
        case STMT_ASSERT:
            printf("assert, condition=");
            node_print(node->assert.condition, indent);
            printf(", message=");
            node_print(node->assert.message, indent);
            break;
        case STMT_IMPORT:
            printf("import, imports=");
            import_libs_print_indent(node->imports.v, indent + 1);
            break;
        case STMT_IMPORT_FROM:
            printf("import_from, level=%d", node->import_from.level);
            printf(", lib=");
            tokens_p_print_indent(node->import_from.lib, indent + 1);
            printf(", imports=");
            import_aliases_print_indent(node->import_from.imports, indent + 1);
            break;
        case STMT_GLOBAL:
            printf("global, names=");
            tokens_p_print_indent(node->global, indent + 1);
            break;
        case STMT_NONLOCAL:
            printf("nonlocal, names=");
            tokens_p_print_indent(node->nonlocal, indent + 1);
            break;
        case STMT_EXPR:
            printf("expr, value=");
            node_print(node->stmt_expr, indent);
            break;
        case STMT_PASS:
            printf("pass");
            break;
        case STMT_BREAK:
            printf("break");
            break;
        case STMT_CONTINUE:
            printf("continue");
            break;
    }

    putchar(')');
}

void comprehension_print(comprehension comp, int indent) {
    printf("comprehension(target=");
    node_print(comp.target, indent);
    printf(", iter=");
    node_print(comp.iter, indent);
    printf(", ifs=");
    nodes_print_indent(comp.ifs, indent + 1);
    printf(", is_async=%s)", comp.is_async ? "true" : "false");
}

void import_alias_print(import_alias al, int indent) {
    printf("(val=");
    token_p_print(al.tok, indent + 1);
    printf(", as=");
    token_p_print(al.as, indent + 1);
    putchar(')');
}

void import_lib_print(import_lib al, int indent) {
    printf("(val=");
    tokens_p_print_indent(al.tok, indent + 1);
    printf(", as=");
    token_p_print(al.as, indent + 1);
    putchar(')');
}

void keyword_print(keyword kws, int indent) {
    printf("(key=");
    token_p_print(kws.name, indent + 1);
    printf(", value=");
    node_print(kws.value, indent);
    putchar(')');
}

void match_case_print(match_case cas, int indent) {
    printf("(pattern=");
    node_print(cas.pattern, indent);
    printf(", as=");
    token_p_print(cas.as, indent + 1);
    printf(", condition=");
    node_print(cas.condition, indent);
    printf(", body=");
    node_print(cas.body, indent + 1);
    putchar(')');
}

void except_handler_print(except_handler eh, int indent) {
    printf("(type=");
    node_print(eh.class, indent);
    printf(", name=");
    token_p_print(eh.name, indent + 1);
    printf(", body=");
    node_print(eh.body, indent + 1);
    putchar(')');
}

#endif

void node_free(node *obj) {
    if (!obj)
        return;

    switch (obj->type) {
        case NODE_MODULE:
            free(obj->module.filename);
            free(obj->module.code);
            tokens_clear(&obj->module.tokens);
            break;
        case EXPR_IDENTIFIER:
        case EXPR_CONSTANT:
        case EXPR_OPERATOR:
        case STMT_PASS:
        case STMT_BREAK:
        case STMT_CONTINUE:
            break;
        case EXPR_BINARY_OPERATION:
            node_free(obj->bin_op.left);
            node_free(obj->bin_op.right);
            break;
        case EXPR_UNARY_OPERATION:
            node_free(obj->unary_op.expr);
            break;
        case EXPR_CMP_OPERATION:
            nodes_clear(&obj->cmp_op.ex);
            break;
        case EXPR_WALRUS_OPERATION:
            node_free(obj->walrus_op.left);
            node_free(obj->walrus_op.right);
            break;
        case EXPR_FORMAT_STRING:
            nodes_clear(&obj->fstring.values);
            free(obj->fstring.strings.data);
            nodes_clear(&obj->fstring.extras);
            break;
        case NODE_GROUP:
            nodes_clear(&obj->group.v);
            break;
        case EXPR_LIST_COMP:
            node_free(obj->list_comp.value);
            comprehensions_clear(&obj->list_comp.comp);
            break;
        case EXPR_LIST:
            nodes_clear(&obj->list.v);
            break;
        case EXPR_DICT_COMP:
            node_free(obj->dict_comp.key);
            node_free(obj->dict_comp.value);
            break;
        case EXPR_DICT:
            nodes_clear(&obj->dict.keys);
            nodes_clear(&obj->dict.values);
            break;
        case EXPR_SET_COMP:
            node_free(obj->set_comp.value);
            comprehensions_clear(&obj->set_comp.comp);
            break;
        case EXPR_SET:
            nodes_clear(&obj->set.v);
            break;
        case EXPR_TUPLE:
            nodes_clear(&obj->tuple.v);
            break;
        case EXPR_LAMBDA:
            free(obj->lambda.args.data);
            node_free(obj->lambda.body);
            break;
        case EXPR_IF_EXP:
            node_free(obj->if_else_expr.if_expr);
            node_free(obj->if_else_expr.condition);
            node_free(obj->if_else_expr.else_expr);
            break;
        case EXPR_GENERATOR:
            node_free(obj->generator.value);
            comprehensions_clear(&obj->generator.comp);
            break;
        case EXPR_AWAIT:
            node_free(obj->await);
            break;
        case EXPR_YIELD:
            node_free(obj->yield);
            break;
        case EXPR_YIELD_FROM:
            node_free(obj->yield_from);
            break;
        case EXPR_CALL:
            node_free(obj->call.base);
            nodes_clear(&obj->call.args);
            keywords_clear(&obj->call.kws);
            break;
        case EXPR_ATTRIBUTE:
            node_free(obj->attribute.base);
            break;
        case EXPR_INDEX:
            node_free(obj->index.base);
            node_free(obj->index.slices[0]);
            node_free(obj->index.slices[1]);
            node_free(obj->index.slices[2]);
            break;
        case STMT_FUNCTION_DEF:
            free(obj->function_def.args.data);
            nodes_clear(&obj->function_def.decorators);
            node_free(obj->function_def.body);
            break;
        case STMT_CLASS_DEF:
            free(obj->class_def.extends.data);
            nodes_clear(&obj->class_def.methods);
            nodes_clear(&obj->class_def.properties);
            nodes_clear(&obj->class_def.decorators);
            break;
        case STMT_RETURN:
            node_free(obj->ret.v);
            break;
        case STMT_DELETE:
            node_free(obj->del);
            break;
        case STMT_ASSIGN:
            node_free(obj->assign.var);
            node_free(obj->assign.val);
            break;
        case STMT_ASSIGN_MULT:
            nodes_clear(&obj->assign_mult.targets);
            node_free(obj->assign_mult.val);
            break;
        case STMT_FOR:
            node_free(obj->for_loop.value);
            node_free(obj->for_loop.iter);
            node_free(obj->for_loop.body);
            break;
        case STMT_WHILE:
            node_free(obj->while_loop.cond);
            node_free(obj->while_loop.body);
            break;
        case STMT_IF:
            nodes_clear(&obj->if_stmt.conditions);
            nodes_clear(&obj->if_stmt.bodies);
            break;
        case STMT_WITH:
            nodes_clear(&obj->with.contexts);
            free(obj->with.vars.data);
            node_free(obj->with.body);
            break;
        case STMT_MATCH:
            match_cases_clear(&obj->match.cases);
            node_free(obj->match.subject);
            break;
        case STMT_RAISE:
            node_free(obj->raise.exception);
            node_free(obj->raise.cause);
            break;
        case STMT_TRY_CATCH:
            node_free(obj->try_catch.try_body);
            except_handlers_clear(&obj->try_catch.handlers);
            node_free(obj->try_catch.else_body);
            node_free(obj->try_catch.finally_body);
            break;
        case STMT_ASSERT:
            node_free(obj->assert.condition);
            node_free(obj->assert.message);
            break;
        case STMT_IMPORT:
            import_libs_clear(&obj->imports.v);
            break;
        case STMT_IMPORT_FROM:
            import_aliases_clear(&obj->import_from.imports);
            free(obj->import_from.lib.data);
            break;
        case STMT_GLOBAL:
            free(obj->global.data);
            break;
        case STMT_NONLOCAL:
            free(obj->nonlocal.data);
            break;
        case STMT_EXPR:
            node_free(obj->stmt_expr);
            break;
    }

    free(obj);
}

void comprehension_free(comprehension comp) {
    nodes_clear(&comp.ifs);
    node_free(comp.target);
    node_free(comp.iter);
}

void import_alias_free(import_alias al) {
}

void import_lib_free(import_lib al) {
    free(al.tok.data);
}

void keyword_free(keyword kw) {
    node_free(kw.value);
}

void match_case_free(match_case kw) {
    node_free(kw.condition);
    node_free(kw.body);
    node_free(kw.pattern);
}

void except_handler_free(except_handler eh) {
    node_free(eh.class);
    node_free(eh.body);
}

#pragma clang diagnostic pop