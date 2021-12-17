#include "ast.h"
#include "../stb/stb_ds.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int indentation_level = 0;

static ast *make_base_ast(token t, ast_tag tag) {
    ast *a = (ast *) malloc(sizeof(ast));
    if (a == NULL) {
        fprintf(stderr, "%s - Error alocatting memory for the new ast node\n,", __FUNCTION__);
        return NULL;
    }
    a->tag = tag;
    a->token = t;

    if(t.literal) {
        a->token.literal = strndup(t.literal, t.literal_len);
    }

    if(t.file_name) {
        a->token.file_name = strdup(t.file_name);
    }

    return a;
}

ast *make_import_stmt(struct token_t t) {
    return make_base_ast(t, ast_import_stmt);
}

ast *make_assignement_stmt(token t, ast_tag tag) {
    return make_base_ast(t, tag);
}

ast *make_grouped_assignement_stmt(token t) {
    ast *a = make_base_ast(t, ast_grouped_assignment_stmt);

    if (a != NULL) {
        a->grouped_assignement_stmt.names = NULL;
        a->grouped_assignement_stmt.call_expr = NULL;
    }
    return a;
}

ast *make_while_stmt(token t) {
    ast *a = make_base_ast(t, ast_while_stmt);

    if (a != NULL) {
        a->while_stmt.body = NULL;
    }

    return a;
}


ast *make_identifier(token t) {
    ast *a = make_base_ast(t, ast_identifier);

    if (a != NULL) {
        a->identifier.value = strndup(t.literal, t.literal_len);
        a->identifier.global = false;
    }


    return a;
}

ast *make_string_literal(token t) {
    ast *a = make_base_ast(t, ast_string_literal);

    if (a != NULL) {
        a->str_literal.value = strndup(t.literal, t.literal_len);
    }
    return a;
}

ast *make_return_stmt(token t) {
    ast *a = make_base_ast(t, ast_return_stmt);
    return a;
}

ast *make_expression_stmt(token t) {
    ast *a = make_base_ast(t, ast_expression_stmt);
    return a;
}

ast *make_number_literal(token t) {
    ast *a = make_base_ast(t, ast_number_literal);
    return a;
}

ast *make_boolean_literal(token t, bool value) {
    ast *a = make_base_ast(t, ast_boolean_literal);

    if (a != NULL) {
        a->bool_literal.value = value;
    }

    return a;
}

ast *make_prefix_expression(token t) {
    ast *a = make_base_ast(t, ast_prefix_expression);

    if (a != NULL) {
        a->prefix_expr.op = strndup(t.literal, t.literal_len);
    }

    return a;
}

ast *make_infix_expression(token t, ast *left) {
    ast *a = make_base_ast(t, ast_infix_expression);

    if(a != NULL) {
        a->infix_expr.op = strndup(t.literal, t.literal_len);
        a->infix_expr.left = left;
    }
    return a;
}

ast *make_if_expression(token t) {
    ast *a = make_base_ast(t, ast_if_expr);

    if(a != NULL) {
        a->if_expr.alternative = NULL;
        a->if_expr.consequence = NULL;
        a->if_expr.elif_alternative = NULL;
    }

    return a;
}

ast *make_function_statement(token t) {
    ast *a = make_base_ast(t, ast_function_statement);

    if(a != NULL) {
        a->function_stmt.body = NULL;
        a->function_stmt.parameters = NULL;
    }
    return a;
}

ast *make_call_expression(token t, ast *function) {
    ast *a = make_base_ast(t, ast_call_expression);

    if(a != NULL) {
        a->call_expr.function_identifier = function;
        a->call_expr.arguments = NULL;
    }

    return a;
}

ast *make_builtin_function_ast(char *name, int n_params) {

    token t = new_token(FUNCTION, name, strlen(name), 0, NULL);
    ast *a  = make_function_statement(t);

    arrsetlen(a->function_stmt.parameters, n_params);

    a->function_stmt.num_return_values = 1;

    return a;

}


static sds expression_stmt_to_str(ast *a) {
    if (a->expr_stmt != NULL) {
        return ast_to_string(a->expr_stmt);
    }
    return sdsempty();
}

static sds return_stmt_to_str(ast *a) {

    sds buf = sdsempty();

    buf = sdscatfmt(buf, "%sreturn ", indent_spaces[indentation_level]);


    if (a->return_stmt.return_values != NULL) {
        int n = arrlen(a->return_stmt.return_values);

        sds tmp;
        tmp = ast_to_string(a->return_stmt.return_values[0]);
        buf = sdscat(buf, tmp);
        sdsfree(tmp);
        

        for (int i = 1; i < n; i++) {
            tmp = ast_to_string(a->return_stmt.return_values[i]);
            buf = sdscatfmt(buf, ", %s", tmp);
            sdsfree(tmp);
        }
    }

    buf = sdscat(buf, ";");
    return buf;
}

static sds assignement_stmt_to_str(ast *a) {

    sds buf = sdsempty();

    if (a->tag == ast_ode_stmt || a->tag == ast_global_stmt || a->tag == ast_initial_stmt) {
        buf = sdscatfmt(buf, "%s%s ", indent_spaces[indentation_level], a->token.literal);
        buf = sdscatfmt(buf, "%s", a->assignement_stmt.name->identifier.value);
    } else {
        buf = sdscatfmt(buf, "%s%s", indent_spaces[indentation_level], a->assignement_stmt.name->identifier.value);
    }

    buf = sdscat(buf, " = ");

    if (a->assignement_stmt.value != NULL) {
        sds tmp = ast_to_string(a->assignement_stmt.value);
        buf = sdscat(buf, tmp);
        sdsfree(tmp);
    }

    return buf;
}

static sds number_literal_to_str(ast *a) {
    sds buf = sdsempty();
    buf = sdscatprintf(buf, "%s", a->token.literal);
    return buf;
}

static sds identifier_to_str(ast *a) {
    sds buf = sdsempty();
    buf = sdscatprintf(buf, "%s", a->identifier.value);
    return buf;
}

static sds boolean_literal_to_str(ast *a) {
    sds buf = sdsempty();
    buf = sdscatprintf(buf, "%s", a->token.literal);

    return buf;
}

static sds string_literal_to_str(ast *a) {
    sds buf = sdsempty();
    buf = sdscatprintf(buf, "\"%s\"", a->str_literal.value);
    return buf;
}

static sds prefix_expr_to_str(ast *a) {

    sds buf = sdsempty();

    buf = sdscat(buf, "(");
    buf = sdscatfmt(buf, "%s", a->prefix_expr.op);
    sds tmp = ast_to_string(a->prefix_expr.right);
    buf = sdscatfmt(buf, "%s", tmp);
    sdsfree(tmp);
    buf = sdscat(buf, ")");

    return buf;
}

static sds infix_expr_to_str(ast *a) {

    sds buf = sdsempty();
    sds tmp;


    buf = sdscat(buf, "(");

    tmp = ast_to_string(a->infix_expr.left);
    buf = sdscatfmt(buf, "%s", tmp);
    sdsfree(tmp);

    buf = sdscatfmt(buf, "%s", a->infix_expr.op);

    tmp = ast_to_string(a->infix_expr.right);
    buf = sdscatfmt(buf, "%s", tmp);
    sdsfree(tmp);

    buf = sdscat(buf, ")");

    return buf;
}

static sds if_expr_to_str(ast *a) {

    sds buf = sdsempty();
    sds tmp;

    buf = sdscatfmt(buf, "%sif", indent_spaces[indentation_level]);

    tmp = ast_to_string(a->if_expr.condition);
    buf = sdscatfmt(buf, "%s {\n", tmp);
    sdsfree(tmp);

    indentation_level++;

    int n = arrlen(a->if_expr.consequence);
    for (int i = 0; i < n; i++) {
        tmp = ast_to_string(a->if_expr.consequence[i]);
        buf = sdscatfmt(buf, "%s\n", tmp);
        sdsfree(tmp);
    }
    indentation_level--;

    buf = sdscatfmt(buf, "%s}", indent_spaces[indentation_level]);

    n = arrlen(a->if_expr.alternative);

    if (n) {

        indentation_level++;
        buf = sdscat(buf, " else {\n");
        for (int i = 0; i < n; i++) {
            tmp = ast_to_string(a->if_expr.alternative[i]);
            buf = sdscatfmt(buf, "%s\n", tmp);
            sdsfree(tmp);
        }
        indentation_level--;

        buf = sdscatfmt(buf, "%s}", indent_spaces[indentation_level]);
    }

    return buf;
}

static sds while_stmt_to_str(ast *a) {

    sds buf = sdsempty();
    sds tmp;

    buf = sdscat(buf, "while");

    tmp = ast_to_string(a->while_stmt.condition);
    buf = sdscatfmt(buf, "%s ", tmp);
    sdsfree(tmp);

    buf = sdscat(buf, "{");

    indentation_level++;
    int n = arrlen(a->while_stmt.body);

    for (int i = 0; i < n; i++) {
        tmp = ast_to_string(a->while_stmt.body[i]);
        buf = sdscatfmt(buf, "%s\n", tmp);
        sdsfree(tmp);
    }

    indentation_level--;
    buf = sdscatfmt(buf, "%s}", indent_spaces[indentation_level]);
    return buf;
}

static sds function_stmt_to_str(ast *a) {

    sds buf = sdsempty();
    buf = sdscatfmt(buf, "fn %s", a->function_stmt.name->identifier.value);
    buf = sdscat(buf, "(");

    sds tmp;

    int n = arrlen(a->function_stmt.parameters);

    if (n) {
        tmp = ast_to_string(a->function_stmt.parameters[0]);
        buf = sdscat(buf, tmp);
        sdsfree(tmp);

        for (int i = 1; i < n; i++) {
            tmp = ast_to_string(a->function_stmt.parameters[i]);
            buf = sdscatfmt(buf, ", %s", tmp);
            sdsfree(tmp);
        }
    }

    buf = sdscat(buf, ") ");

    n = arrlen(a->function_stmt.body);
    buf = sdscat(buf, "{\n");

    indentation_level++;
    for (int i = 0; i < n; i++) {
        tmp = ast_to_string(a->function_stmt.body[i]);
        buf = sdscatfmt(buf, "%s\n", tmp);
        sdsfree(tmp);
    }
    indentation_level--;

    buf = sdscat(buf, "}\n");


    return buf;
}

static sds call_expr_to_str(ast *a) {

    sds buf = sdsempty();

    buf = sdscat(buf, a->call_expr.function_identifier->identifier.value);
    buf = sdscat(buf, "(");

    int n = arrlen(a->call_expr.arguments);

    if (n) {
        sds tmp = ast_to_string(a->call_expr.arguments[0]);
        buf = sdscat(buf, tmp);
        sdsfree(tmp);

        for (int i = 1; i < n; i++) {
            tmp = ast_to_string(a->call_expr.arguments[i]);
            buf = sdscatfmt(buf, ", %s", tmp);
            sdsfree(tmp);
        }
    }

    buf = sdscat(buf, ")");

    return buf;
}

static sds import_stmt_to_str(ast *a) {

    sds buf = sdsempty();

    sds tmp = ast_to_string(a->import_stmt.filename);
    buf = sdscatfmt(buf, "import %s", tmp);
    sdsfree(tmp);

    return buf;
}

static sds grouped_assignement_stmt_to_str(ast *a) {
    sds buf = sdsnew("[");

    int n = arrlen(a->grouped_assignement_stmt.names);

    buf = sdscat(buf, a->grouped_assignement_stmt.names[0]->identifier.value);

    for (int i = 1; i < n; i++) {
        buf = sdscatfmt(buf, ", %s", a->grouped_assignement_stmt.names[i]->identifier.value);
    }

    buf = sdscat(buf, "]");

    buf = sdscat(buf, " = ");

    sds tmp = ast_to_string(a->grouped_assignement_stmt.call_expr);
    buf = sdscat(buf, tmp);
    sdsfree(tmp);


    return buf;
}

sds ast_to_string(ast *a) {

    if (a->tag == ast_assignment_stmt || a->tag == ast_ode_stmt || a->tag == ast_initial_stmt || a->tag == ast_global_stmt) {
        return assignement_stmt_to_str(a);
    }

    if (a->tag == ast_grouped_assignment_stmt) {
        return grouped_assignement_stmt_to_str(a);
    }

    if (a->tag == ast_return_stmt) {
        return return_stmt_to_str(a);
    }

    if (a->tag == ast_expression_stmt) {
        return expression_stmt_to_str(a);
    }

    if (a->tag == ast_number_literal) {
        return number_literal_to_str(a);
    }

    if (a->tag == ast_boolean_literal) {
        return boolean_literal_to_str(a);
    }

    if (a->tag == ast_string_literal) {
        return string_literal_to_str(a);
    }

    if (a->tag == ast_identifier) {
        return identifier_to_str(a);
    }

    if (a->tag == ast_prefix_expression) {
        return prefix_expr_to_str(a);
    }

    if (a->tag == ast_infix_expression) {
        return infix_expr_to_str(a);
    }

    if (a->tag == ast_if_expr) {
        return if_expr_to_str(a);
    }

    if (a->tag == ast_while_stmt) {
        return while_stmt_to_str(a);
    }

    if (a->tag == ast_function_statement) {
        return function_stmt_to_str(a);
    }

    if (a->tag == ast_call_expression) {
        return call_expr_to_str(a);
    }

    if (a->tag == ast_import_stmt) {
        return import_stmt_to_str(a);
    }

    printf("[WARN] - to_str not implemented to token %s\n", a->token.literal);

    return NULL;
}

sds *program_to_string(program p) {

    indentation_level = 0;

    int n_stmt = arrlen(p);
    sds *return_str = NULL;

    for (int i = 0; i < n_stmt; i++) {
        sds s = ast_to_string(p[i]);

        if (s) {
            arrput(return_str, s);
        }
    }

    return return_str;
}

ast *copy_ast(ast *src) {

    ast *a = (ast *) malloc(sizeof(ast));

    if(a == NULL) {
        fprintf(stderr, "%s - Error allocating memory for the new ast!\n", __FUNCTION__);
        return NULL;
    }

    a->tag = src->tag;

    a->token = src->token;
    a->token.literal = strdup(src->token.literal);
    a->token.file_name = strdup(src->token.file_name);

    switch (src->tag) {

        case ast_identifier:
            a->identifier.value  = strdup(src->identifier.value);
            a->identifier.global = src->identifier.global;
            break;
        case ast_number_literal:
            a->num_literal.value = src->num_literal.value;
            break;
        case ast_boolean_literal:
            a->bool_literal.value = src->bool_literal.value;
            break;
        case ast_string_literal:
            a->str_literal.value = strdup(src->str_literal.value);
            break;
        case ast_assignment_stmt:
        case ast_ode_stmt:
        case ast_global_stmt:
        case ast_initial_stmt:
            a->assignement_stmt.name = copy_ast(src->assignement_stmt.name);
            a->assignement_stmt.value = copy_ast(src->assignement_stmt.value);
            a->assignement_stmt.declaration_position = src->assignement_stmt.declaration_position;
            break;
        case ast_grouped_assignment_stmt: {
                                              a->grouped_assignement_stmt.names = NULL;
                                              int n = arrlen(src->grouped_assignement_stmt.names);

                                              for (int i = 0; i < n; i++) {
                                                  arrput(a->grouped_assignement_stmt.names, copy_ast(src->grouped_assignement_stmt.names[i]));
                                              }

                                              a->grouped_assignement_stmt.call_expr = src->grouped_assignement_stmt.call_expr;
                                          } break;
        case ast_function_statement: {
                                         a->function_stmt.name = copy_ast(src->assignement_stmt.name);

                                         int n = arrlen(src->function_stmt.parameters);
                                         a->function_stmt.parameters = NULL;
                                         for (int i = 0; i < n; i++) {
                                             arrput(a->function_stmt.parameters, copy_ast(src->function_stmt.parameters[i]));
                                         }

                                         n = arrlen(src->function_stmt.body);
                                         a->function_stmt.body = NULL;
                                         for (int i = 0; i < n; i++) {
                                             arrput(a->function_stmt.body, copy_ast(src->function_stmt.body[i]));
                                         }

                                         a->function_stmt.num_return_values = src->function_stmt.num_return_values;

                                     } break;
        case ast_return_stmt: {
                                  int n = arrlen(src->return_stmt.return_values);
                                  a->return_stmt.return_values = NULL;
                                  for (int i = 0; i < n; i++) {
                                      arrput(a->return_stmt.return_values, copy_ast(src->return_stmt.return_values[i]));
                                  }

                              } break;
        case ast_expression_stmt:
                              a->expr_stmt = copy_ast(src->expr_stmt);
                              break;
        case ast_while_stmt: {
                                 a->while_stmt.condition = copy_ast(src->while_stmt.condition);

                                 int n = arrlen(src->while_stmt.body);
                                 a->while_stmt.body = NULL;
                                 for (int i = 0; i < n; i++) {
                                     arrput(a->while_stmt.body, copy_ast(src->while_stmt.body[i]));
                                 }
                             } break;
        case ast_import_stmt:
                             a->import_stmt.filename = copy_ast(src->import_stmt.filename);
                             break;
        case ast_prefix_expression:
                             a->prefix_expr.op = strdup(src->prefix_expr.op);
                             a->prefix_expr.right = copy_ast(src->prefix_expr.right);
                             break;
        case ast_infix_expression:
                             a->infix_expr.left = copy_ast(src->infix_expr.left);
                             a->infix_expr.op = strdup(src->infix_expr.op);
                             a->infix_expr.right = copy_ast(src->infix_expr.right);
                             break;
        case ast_if_expr: {
                              a->if_expr.condition = copy_ast(src->if_expr.condition);

                              int n = arrlen(src->if_expr.consequence);
                              a->if_expr.consequence = NULL;
                              for (int i = 0; i < n; i++) {
                                  arrput(a->if_expr.consequence, copy_ast(src->if_expr.consequence[i]));
                              }

                              n = arrlen(src->if_expr.alternative);
                              a->if_expr.alternative = NULL;
                              for (int i = 0; i < n; i++) {
                                  arrput(a->if_expr.alternative, copy_ast(src->if_expr.alternative[i]));
                              }

                          } break;
        case ast_call_expression:
                          a->call_expr.function_identifier = copy_ast(src->call_expr.function_identifier);

                          int n = arrlen(src->call_expr.arguments);
                          a->call_expr.arguments = NULL;
                          for (int i = 0; i < n; i++) {
                              arrput(a->call_expr.arguments, copy_ast(src->call_expr.arguments[i]));
                          }

                          break;
    }

    return a;
}

program copy_program(program src_program) {

    program dst_program = NULL;

    int p_len = arrlen(src_program);

    for (int i = 0; i < p_len; i++) {
        ast *a = copy_ast(src_program[i]);
        arrput(dst_program, a);
    }

    return dst_program;
}

void free_program(program src_program) {

    int p_len = arrlen(src_program);

    for (int i = 0; i < p_len; i++) {
        free_ast(src_program[i]);
    }

    arrfree(src_program);
}

void free_ast(ast *src) {

    if(src == NULL) return;

    switch (src->tag) {

        case ast_number_literal:
        case ast_boolean_literal:
            break;
        case ast_identifier:
            free(src->identifier.value);
            break;
        case ast_string_literal:
            free(src->str_literal.value);
            break;
        case ast_assignment_stmt:
        case ast_ode_stmt:
        case ast_global_stmt:
        case ast_initial_stmt:
            free_ast(src->assignement_stmt.name);
            free_ast(src->assignement_stmt.value);
            break;
        case ast_grouped_assignment_stmt:
            free_program(src->grouped_assignement_stmt.names);
            break;
        case ast_function_statement:
            free_ast(src->function_stmt.name);
            free_program(src->function_stmt.parameters);
            free_program(src->function_stmt.body);
            break;
        case ast_return_stmt:
            free_program(src->return_stmt.return_values);
            break;
        case ast_expression_stmt:
            free_ast(src->expr_stmt);
            break;
        case ast_while_stmt:
            free_program(src->while_stmt.body);
            break;
        case ast_import_stmt:
            free_ast(src->import_stmt.filename);
            break;
        case ast_prefix_expression:
            free(src->prefix_expr.op);
            free_ast(src->prefix_expr.right);
            break;
        case ast_infix_expression:
            free_ast(src->infix_expr.left);
            free(src->infix_expr.op);
            free_ast(src->infix_expr.right);
            break;
        case ast_if_expr:
            free_ast(src->if_expr.condition);
            free_program(src->if_expr.consequence);
            free_program(src->if_expr.alternative);
            free_ast(src->if_expr.elif_alternative);
            break;
        case ast_call_expression:
            free_ast(src->call_expr.function_identifier);
            free_program(src->call_expr.arguments);
            break;
    }

    free(src->token.literal);
    free(src->token.file_name);
    free(src);
}
