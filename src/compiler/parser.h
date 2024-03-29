#ifndef __PARSER_H
#define __PARSER_H 

#include "ast.h"
#include "lexer.h"
#include "token.h"
#include "program.h"

#define ODE_GET_VALUE "ode_get_value"
#define ODE_GET_TIME "ode_get_time"
#define ODE_GET_N_IT "ode_get_num_iterations"

typedef struct declared_variable_entry_value_t {
    uint32_t  declaration_position;
    bool initialized;
    uint32_t line_number;
    ast_tag tag;
} declared_variable_entry_value;

typedef struct declared_function_entry_value_t {
    int n_returns;
    int n_args;
} declared_function_entry_value;

typedef struct declared_variable_entry_t {
    char *key;
    declared_variable_entry_value value;
} declared_variable_entry;

typedef struct declared_function_entry_t {
    char *key;
    declared_function_entry_value value;
} declared_function_entry;

typedef declared_variable_entry * declared_variable_hash;
typedef declared_function_entry * declared_function_hash;

enum operator_precedence {
    BLANK = 0,
    LOWEST,
    EQUALS,
    ANDP,
    ORP,
    LESSGREATER,
    SUM,
    PRODUCT,
    PREFIX,
    CALL
};

typedef struct parser_t {
    lexer *l;
    char **errors;
    token cur_token;
    token peek_token;
    declared_variable_hash declared_variables;
    declared_variable_hash global_scope;
    declared_variable_hash local_scope;
    declared_function_hash declared_functions;
    bool have_ode;
} parser;

parser * new_parser(lexer *l);
void free_parser(parser *p);
program parse_program(parser *p, bool process_imports, bool check_errors, char *import_path);
program parse_program_without_exiting_on_error(parser *p, bool proc_imports, bool check_errors, char *import_path);
ast *parse_prefix_expression(parser *p);
ast *parse_infix_expression(parser *p, ast *left);
ast *parse_expression(parser *p, enum operator_precedence precedence);
ast * parse_statement(parser *p);
bool check_parser_errors(parser *p, bool exit_on_error);

#endif /* __PARSER_H */
