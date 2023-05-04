#ifndef AST_H
#define AST_H

#include "../string/sds.h"
#include "token.h"
#include <stdbool.h>

typedef struct assignment_statement_t {
    struct ast_t *name;
    struct ast_t *value;
    int declaration_position;
} assignment_statement;

typedef struct grouped_assignment_statement_t {
    struct ast_t **names;
    struct ast_t *call_expr;
} grouped_assignment_statement;

typedef struct return_statement_t {
    struct ast_t **return_values;
} return_statement;

typedef struct import_statement_t {
    struct ast_t *filename;
} import_statement;

typedef struct while_statement_t {
    struct ast_t *condition;
    struct ast_t **body;
} while_statement;

typedef struct number_literal_t {
    double value;
} number_literal;

typedef struct string_literal_t {
    char *value;
} string_literal;

typedef struct boolean_literal_t {
    bool value;
} boolean_literal;

typedef struct identifier_t {
    char *value;
    bool global;
} identifier_node;

typedef struct prefix_expression_t {
    char *op;
    struct ast_t *right;
} prefix_expression;

typedef struct infix_expression_t {
    struct ast_t *left;
    char *op;
    struct ast_t *right;
} infix_expression;

typedef struct if_expression_t {
    struct ast_t *condition;
    struct ast_t **consequence;
    struct ast_t **alternative;
    struct ast_t *elif_alternative;
} if_expression;

typedef struct function_statement_t {
    struct ast_t *name;
    struct ast_t **parameters;
    struct ast_t **body;
    int num_return_values;
    bool is_end_fn;
} function_statement;

typedef struct call_expression_t {
    struct ast_t *function_identifier;
    struct ast_t **arguments;
} call_expression;

typedef enum ast_tag_t {
    ast_identifier,
    ast_assignment_stmt,
    ast_grouped_assignment_stmt,
    ast_function_statement,
    ast_ode_stmt,
    ast_initial_stmt,
    ast_global_stmt,
    ast_return_stmt,
    ast_expression_stmt,
    ast_while_stmt,
    ast_import_stmt,
    ast_prefix_expression,
    ast_infix_expression,
    ast_number_literal,
    ast_string_literal,
    ast_boolean_literal,
    ast_if_expr,
    ast_call_expression
} ast_tag;

typedef struct ast_t {

    token token;
    ast_tag tag;

    union {
        identifier_node identifier;
        assignment_statement assignment_stmt;
        while_statement while_stmt;
        return_statement return_stmt;
        number_literal num_literal;
        prefix_expression prefix_expr;
        infix_expression infix_expr;
        boolean_literal bool_literal;
        string_literal str_literal;
        if_expression if_expr;
        function_statement function_stmt;
        call_expression call_expr;
        import_statement import_stmt;
        grouped_assignment_statement grouped_assignment_stmt;

        struct ast_t *expr_stmt;
    };

} ast;

ast *make_assignment_stmt(const token *t, ast_tag tag);
ast *make_grouped_assignment_stmt(const token *t);
ast *make_identifier(const token *t);
ast *make_return_stmt(const token *t);
ast *make_while_stmt(const token *t);
ast *make_expression_stmt(const token *t);
ast *make_number_literal(const token *t);
ast *make_boolean_literal(const token *t, bool value);
ast *make_prefix_expression(const token *t);
ast *make_infix_expression(const token *t, ast *left);
ast *make_if_expression(const token *t);
ast *make_function_statement(const token *t);
ast *make_call_expression(const token *t, ast *function);
ast *make_import_stmt(const token *t);
ast *make_string_literal(const token *t);

sds ast_to_string(ast *a);
ast *copy_ast(ast *src);
void free_ast(ast *src);
void free_asts(ast **asts);

#endif /* AST_H */
