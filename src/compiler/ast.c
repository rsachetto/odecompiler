#include "ast.h"
#include "../stb/stb_ds.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//For printing
#define NO_SPACES  ""
#define _4SPACES   "    "
#define _8SPACES  _4SPACES _4SPACES
#define _12SPACES _8SPACES _4SPACES
#define _16SPACES _12SPACES _4SPACES
#define _20SPACES _16SPACES _4SPACES
#define _24SPACES _20SPACES _4SPACES
#define _28SPACES _24SPACES _4SPACES

char *indent_spaces[] = {NO_SPACES, _4SPACES, _8SPACES, _12SPACES, _16SPACES, _20SPACES, _24SPACES, _28SPACES};

static ast *make_base_ast(const token *t, ast_tag tag) {
    ast *a = (ast *) calloc(1, sizeof(ast));

    if (a == NULL) {
        fprintf(stderr, "%s - Error allocating memory for the new ast node\n,", __FUNCTION__);
        return NULL;
    }
    a->tag = tag;

    copy_token(&a->token, t);

    return a;
}

ast *make_import_stmt(const token *t) {
    return make_base_ast(t, ast_import_stmt);
}

ast *make_assignment_stmt(const token *t, ast_tag tag) {
    ast *a = make_base_ast(t, tag);
    return a;
}

ast *make_grouped_assignment_stmt(const token *t) {
    ast *a = make_base_ast(t, ast_grouped_assignment_stmt);
    return a;
}

ast *make_while_stmt(const token *t) {
    ast *a = make_base_ast(t, ast_while_stmt);
    return a;
}

ast *make_identifier(const token *t) {
    ast *a = make_base_ast(t, ast_identifier);

    if (a != NULL) {
        a->identifier.value = strndup(t->literal, t->literal_len);
        a->identifier.global = false;
    }

    return a;
}

ast *make_string_literal(const token *t) {
    ast *a = make_base_ast(t, ast_string_literal);

    if (a != NULL) {
        a->str_literal.value = strndup(t->literal, t->literal_len);
    }
    return a;
}

ast *make_return_stmt(const token *t) {
    ast *a = make_base_ast(t, ast_return_stmt);
    return a;
}

ast *make_expression_stmt(const token *t) {
    ast *a = make_base_ast(t, ast_expression_stmt);
    return a;
}

ast *make_number_literal(const token *t) {
    ast *a = make_base_ast(t, ast_number_literal);
    return a;
}

ast *make_boolean_literal(const token *t, bool value) {
    ast *a = make_base_ast(t, ast_boolean_literal);

    if (a != NULL) {
        a->bool_literal.value = value;
    }

    return a;
}

ast *make_prefix_expression(const token *t) {
    ast *a = make_base_ast(t, ast_prefix_expression);

    if (a != NULL) {
        a->prefix_expr.op = strndup(t->literal, t->literal_len);
    }

    return a;
}

ast *make_infix_expression(const token *t, ast *left) {
    ast *a = make_base_ast(t, ast_infix_expression);

    if(a != NULL) {
        a->infix_expr.op = strndup(t->literal, t->literal_len);
        a->infix_expr.left = left;
    }
    return a;
}

ast *make_if_expression(const token *t) {
    ast *a = make_base_ast(t, ast_if_expr);
    return a;
}

ast *make_function_statement(const token *t) {

    ast *a = make_base_ast(t, ast_function_statement);

    if(a != NULL) {
        a->function_stmt.body = NULL;
        a->function_stmt.parameters = NULL;
        if(t->type == ENDFUNCTION) {
            a->function_stmt.is_end_fn = true;
        }
        else {
            a->function_stmt.is_end_fn = false;
        }
    }
    return a;
}

ast *make_call_expression(const token *t, ast *function) {
    ast *a = make_base_ast(t, ast_call_expression);

    if(a != NULL) {
        a->call_expr.function_identifier = function;
    }

    return a;
}

static sds expression_stmt_to_str(ast *a, unsigned int *indentation_level) {
    if (a->expr_stmt != NULL) {
        return ast_to_string(a->expr_stmt, indentation_level);
    }
    return sdsempty();
}

static sds return_stmt_to_str(ast *a, unsigned int *indentation_level) {

    sds buf = sdsempty();

    buf = sdscatfmt(buf, "%sreturn ", indent_spaces[*indentation_level]);


    if (a->return_stmt.return_values != NULL) {
        int n = arrlen(a->return_stmt.return_values);

        sds tmp;
        tmp = ast_to_string(a->return_stmt.return_values[0], indentation_level);
        buf = sdscat(buf, tmp);
        sdsfree(tmp);


        for (int i = 1; i < n; i++) {
            tmp = ast_to_string(a->return_stmt.return_values[i], indentation_level);
            buf = sdscatfmt(buf, ", %s", tmp);
            sdsfree(tmp);
        }
    }

    buf = sdscat(buf, ";");
    return buf;
}

static sds assignment_stmt_to_str(ast *a, unsigned int *indentation_level) {

    sds buf = sdsempty();

    if (a->tag == ast_ode_stmt || a->tag == ast_global_stmt || a->tag == ast_initial_stmt) {
        buf = sdscatfmt(buf, "%s%s ", indent_spaces[*indentation_level], a->token.literal);
        buf = sdscatfmt(buf, "%s", a->assignment_stmt.name->identifier.value);
    } else {
        buf = sdscatfmt(buf, "%s%s", indent_spaces[*indentation_level], a->assignment_stmt.name->identifier.value);
    }

    buf = sdscat(buf, " = ");

    if (a->assignment_stmt.value != NULL) {
        sds tmp = ast_to_string(a->assignment_stmt.value, indentation_level);
        buf = sdscat(buf, tmp);
        sdsfree(tmp);
    }

   if (a->assignment_stmt.unit != NULL) {
        buf = sdscatfmt(buf, " $%s", a->assignment_stmt.unit);
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

static sds prefix_expr_to_str(ast *a, unsigned int *indentation_level) {

    sds buf = sdsempty();

    buf = sdscat(buf, "(");
    buf = sdscatfmt(buf, "%s", a->prefix_expr.op);
    sds tmp = ast_to_string(a->prefix_expr.right, indentation_level);
    buf = sdscatfmt(buf, "%s", tmp);
    sdsfree(tmp);
    buf = sdscat(buf, ")");

    return buf;
}

static sds infix_expr_to_str(ast *a, unsigned int *indentation_level) {

    sds buf = sdsempty();
    sds tmp;


    buf = sdscat(buf, "(");

    tmp = ast_to_string(a->infix_expr.left, indentation_level);
    buf = sdscatfmt(buf, "%s", tmp);
    sdsfree(tmp);

    buf = sdscatfmt(buf, "%s", a->infix_expr.op);

    tmp = ast_to_string(a->infix_expr.right, indentation_level);
    buf = sdscatfmt(buf, "%s", tmp);
    sdsfree(tmp);

    buf = sdscat(buf, ")");

    return buf;
}

static sds if_expr_to_str(ast *a, unsigned int *indentation_level) {

    sds buf = sdsempty();
    sds tmp;

    buf = sdscatfmt(buf, "%sif", indent_spaces[*indentation_level]);

    tmp = ast_to_string(a->if_expr.condition, indentation_level);
    buf = sdscatfmt(buf, "%s {\n", tmp);
    sdsfree(tmp);

    (*indentation_level)++;

    int n = arrlen(a->if_expr.consequence);

    for (int i = 0; i < n; i++) {
        tmp = ast_to_string(a->if_expr.consequence[i], indentation_level);
        buf = sdscatfmt(buf, "%s\n", tmp);
        sdsfree(tmp);
    }
    (*indentation_level)--;

    buf = sdscatfmt(buf, "%s}", indent_spaces[*indentation_level]);

    n = arrlen(a->if_expr.alternative);

    if (n > 0) {

        (*indentation_level)++;
        buf = sdscat(buf, " else {\n");
        for (int i = 0; i < n; i++) {
            tmp = ast_to_string(a->if_expr.alternative[i], indentation_level);
            buf = sdscatfmt(buf, "%s\n", tmp);
            sdsfree(tmp);
        }
        (*indentation_level)--;

        buf = sdscatfmt(buf, "%s}", indent_spaces[*indentation_level]);
    }

    return buf;
}

static sds while_stmt_to_str(ast *a, unsigned int *indentation_level) {

    sds buf = sdsempty();
    sds tmp;

    buf = sdscat(buf, "while");

    tmp = ast_to_string(a->while_stmt.condition, indentation_level);
    buf = sdscatfmt(buf, "%s ", tmp);
    sdsfree(tmp);

    buf = sdscat(buf, "{");

    (*indentation_level)++;
    int n = arrlen(a->while_stmt.body);

    for (int i = 0; i < n; i++) {
        tmp = ast_to_string(a->while_stmt.body[i], indentation_level);
        buf = sdscatfmt(buf, "%s\n", tmp);
        sdsfree(tmp);
    }

    (*indentation_level)--;
    buf = sdscatfmt(buf, "%s}", indent_spaces[*indentation_level]);
    return buf;
}

static sds function_stmt_to_str(ast *a, unsigned int *indentation_level) {

    sds buf = sdsempty();

    if(!a->function_stmt.is_end_fn) {
        buf = sdscatfmt(buf, "fn %s", a->function_stmt.name->identifier.value);
    } else {
        buf = sdscatfmt(buf, "endfn %s", a->function_stmt.name->identifier.value);
    }

    buf = sdscat(buf, "(");

    sds tmp;

    int n = arrlen(a->function_stmt.parameters);

    if (n > 0) {
        tmp = ast_to_string(a->function_stmt.parameters[0], indentation_level);
        buf = sdscat(buf, tmp);
        sdsfree(tmp);

        for (int i = 1; i < n; i++) {
            tmp = ast_to_string(a->function_stmt.parameters[i], indentation_level);
            buf = sdscatfmt(buf, ", %s", tmp);
            sdsfree(tmp);
        }
    }

    buf = sdscat(buf, ") ");

    n = arrlen(a->function_stmt.body);
    buf = sdscat(buf, "{\n");

    (*indentation_level)++;
    for (int i = 0; i < n; i++) {
        tmp = ast_to_string(a->function_stmt.body[i], indentation_level);
        buf = sdscatfmt(buf, "%s\n", tmp);
        sdsfree(tmp);
    }

    (*indentation_level)--;
    buf = sdscat(buf, "}\n");


    return buf;
}

static sds call_expr_to_str(ast *a, unsigned int *indentation_level) {

    sds buf = sdsempty();

    buf = sdscatfmt(buf, "%s%s", indent_spaces[*indentation_level], a->call_expr.function_identifier->identifier.value);
    buf = sdscat(buf, "(");

    int n = arrlen(a->call_expr.arguments);

    if (n > 0) {
        sds tmp = ast_to_string(a->call_expr.arguments[0], indentation_level);
        buf = sdscat(buf, tmp);
        sdsfree(tmp);

        for (int i = 1; i < n; i++) {
            tmp = ast_to_string(a->call_expr.arguments[i], indentation_level);
            buf = sdscatfmt(buf, ", %s", tmp);
            sdsfree(tmp);
        }
    }

    buf = sdscat(buf, ")");

    return buf;
}

static sds import_stmt_to_str(ast *a, unsigned int *indentation_level) {

    sds buf = sdsempty();

    sds tmp = ast_to_string(a->import_stmt.filename, indentation_level);
    buf = sdscatfmt(buf, "import %s", tmp);
    sdsfree(tmp);

    return buf;
}

static sds grouped_assignment_stmt_to_str(ast *a, unsigned int *indentation_level) {
    sds buf = sdsnew("[");

    int n = arrlen(a->grouped_assignment_stmt.names);

    buf = sdscat(buf, a->grouped_assignment_stmt.names[0]->identifier.value);

    for (int i = 1; i < n; i++) {
        buf = sdscatfmt(buf, ", %s", a->grouped_assignment_stmt.names[i]->identifier.value);
    }

    buf = sdscat(buf, "]");

    buf = sdscat(buf, " = ");

    sds tmp = ast_to_string(a->grouped_assignment_stmt.call_expr, indentation_level);
    buf = sdscat(buf, tmp);
    sdsfree(tmp);


    return buf;
}

sds ast_to_string(ast *a, unsigned int *indentation_level) {

    if (a->tag == ast_assignment_stmt || a->tag == ast_ode_stmt || a->tag == ast_initial_stmt || a->tag == ast_global_stmt) {
        return assignment_stmt_to_str(a, indentation_level);
    }

    if (a->tag == ast_grouped_assignment_stmt) {
        return grouped_assignment_stmt_to_str(a, indentation_level);
    }

    if (a->tag == ast_return_stmt) {
        return return_stmt_to_str(a, indentation_level);
    }

    if (a->tag == ast_expression_stmt) {
        return expression_stmt_to_str(a, indentation_level);
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
        return prefix_expr_to_str(a, indentation_level);
    }

    if (a->tag == ast_infix_expression) {
        return infix_expr_to_str(a, indentation_level);
    }

    if (a->tag == ast_if_expr) {
        return if_expr_to_str(a, indentation_level);
    }

    if (a->tag == ast_while_stmt) {
        return while_stmt_to_str(a, indentation_level);
    }

    if (a->tag == ast_function_statement) {
        return function_stmt_to_str(a, indentation_level);
    }

    if (a->tag == ast_call_expression) {
        return call_expr_to_str(a, indentation_level);
    }

    if (a->tag == ast_import_stmt) {
        return import_stmt_to_str(a, indentation_level);
    }

    printf("[WARN] - to_str not implemented to token %s\n", a->token.literal);

    return NULL;
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
            a->assignment_stmt.name = copy_ast(src->assignment_stmt.name);
            a->assignment_stmt.value = copy_ast(src->assignment_stmt.value);
            a->assignment_stmt.declaration_position = src->assignment_stmt.declaration_position;
            if(src->assignment_stmt.unit != NULL) {
                a->assignment_stmt.unit = strdup(src->assignment_stmt.unit);
            } else {
                 a->assignment_stmt.unit = NULL;
            }

            break;
        case ast_grouped_assignment_stmt: {
                                              a->grouped_assignment_stmt.names = NULL;
                                              int n = arrlen(src->grouped_assignment_stmt.names);

                                              for (int i = 0; i < n; i++) {
                                                  arrput(a->grouped_assignment_stmt.names, copy_ast(src->grouped_assignment_stmt.names[i])); //NOLINT
                                              }

                                              a->grouped_assignment_stmt.call_expr = src->grouped_assignment_stmt.call_expr;
                                          } break;
        case ast_function_statement: {
                                         a->function_stmt.name = copy_ast(src->assignment_stmt.name);

                                         int n = arrlen(src->function_stmt.parameters);
                                         a->function_stmt.parameters = NULL;
                                         for (int i = 0; i < n; i++) {
                                             arrput(a->function_stmt.parameters, copy_ast(src->function_stmt.parameters[i])); //NOLINT
                                         }

                                         n = arrlen(src->function_stmt.body);
                                         a->function_stmt.body = NULL;
                                         for (int i = 0; i < n; i++) {
                                             arrput(a->function_stmt.body, copy_ast(src->function_stmt.body[i])); //NOLINT
                                         }

                                         a->function_stmt.num_return_values = src->function_stmt.num_return_values;
                                         a->function_stmt.is_end_fn = src->function_stmt.is_end_fn;

                                     } break;
        case ast_return_stmt: {
                                  int n = arrlen(src->return_stmt.return_values);
                                  a->return_stmt.return_values = NULL;
                                  for (int i = 0; i < n; i++) {
                                      arrput(a->return_stmt.return_values, copy_ast(src->return_stmt.return_values[i])); //NOLINT
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
                                     arrput(a->while_stmt.body, copy_ast(src->while_stmt.body[i])); //NOLINT
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
                                  arrput(a->if_expr.consequence, copy_ast(src->if_expr.consequence[i])); //NOLINT
                              }

                              n = arrlen(src->if_expr.alternative);
                              a->if_expr.alternative = NULL;
                              for (int i = 0; i < n; i++) {
                                  arrput(a->if_expr.alternative, copy_ast(src->if_expr.alternative[i])); //NOLINT
                              }

                          } break;
        case ast_call_expression:
                          a->call_expr.function_identifier = copy_ast(src->call_expr.function_identifier);

                          int n = arrlen(src->call_expr.arguments);
                          a->call_expr.arguments = NULL;
                          for (int i = 0; i < n; i++) {
                              arrput(a->call_expr.arguments, copy_ast(src->call_expr.arguments[i])); //NOLINT
                          }

                          break;
    }

    return a;
}

void free_asts(ast **asts) {

    if(asts == NULL) {
        return;
    }

    int p_len = arrlen(asts);

    for (int i = 0; i < p_len; i++) {
        free_ast(asts[i]);
    }

    arrfree(asts);
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
            free_ast(src->assignment_stmt.name);
            free_ast(src->assignment_stmt.value);
            free(src->assignment_stmt.unit);
            break;
        case ast_grouped_assignment_stmt:
            free_asts(src->grouped_assignment_stmt.names);
            free_ast(src->grouped_assignment_stmt.call_expr);
            break;
        case ast_function_statement:
            free_ast(src->function_stmt.name);
            free_asts(src->function_stmt.parameters);
            free_asts(src->function_stmt.body);
            break;
        case ast_return_stmt:
            free_asts(src->return_stmt.return_values);
            break;
        case ast_expression_stmt:
            free_ast(src->expr_stmt);
            break;
        case ast_while_stmt:
            free_ast(src->while_stmt.condition);
            free_asts(src->while_stmt.body);
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
            free_asts(src->if_expr.consequence);
            free_asts(src->if_expr.alternative);
            free_ast(src->if_expr.elif_alternative);
            break;
        case ast_call_expression:
            free_ast(src->call_expr.function_identifier);
            free_asts(src->call_expr.arguments);
            break;
    }

    free(src->token.literal);
    free( (char*) src->token.file_name);
    free(src);
}
