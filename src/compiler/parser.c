#include "parser.h"
#include "../file_utils/file_utils.h"
#include "../stb/stb_ds.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ERROR_PREFIX "Parse error on line %d of file %s: "
#define NEW_ERROR_PREFIX(ln, fn) sdscatprintf(sdsempty(), ERROR_PREFIX, ln, fn)

#define ADD_ERROR(...)                    \
    sds msg = sdsnew("Parse error: ");    \
    msg = sdscatprintf(msg, __VA_ARGS__); \
    arrput(p->errors, msg)

#define ADD_ERROR_WITH_LINE(ln, fn, ...)  \
    sds msg = NEW_ERROR_PREFIX(ln, fn);   \
    msg = sdscatprintf(msg, __VA_ARGS__); \
    arrput(p->errors, msg)

#define RETURN_ERROR(...)                                                        \
    ADD_ERROR_WITH_LINE(p->cur_token.line_number, p->l->file_name, __VA_ARGS__); \
    return NULL

#define RETURN_ERROR_EXPRESSION(...)                                             \
    ADD_ERROR_WITH_LINE(p->cur_token.line_number, p->l->file_name, __VA_ARGS__); \
    return NULL

#define RETURN_VALUE_AND_ERROR_EXPRESSION(value, ...)                            \
    ADD_ERROR_WITH_LINE(p->cur_token.line_number, p->l->file_name, __VA_ARGS__); \
    return value


static int global_count = 1;
static int local_var_count = 1;
static int ode_count = 1;

ast **parse_expression_list(parser *, bool);

static inline void advance_token(parser *p) {
    p->cur_token = p->peek_token;
    p->peek_token = next_token(p->l);
}

static inline bool cur_token_is(parser *p, token_type t) {
    return p->cur_token.type == t;
}

static inline bool peek_token_is(parser *p, token_type t) {
    return p->peek_token.type == t;
}

static void peek_error(parser *p, token_type t) {
    ADD_ERROR_WITH_LINE(p->cur_token.line_number, p->l->file_name, "expected next token to be %s, got %s instead\n",
                        get_string_token_type(t), get_string_token_type(p->peek_token.type));
}

static inline bool expect_peek(parser *p, token_type t) {
    if(peek_token_is(p, t)) {
        advance_token(p);
        return true;
    } else {
        peek_error(p, t);
        return false;
    }
}

static bool can_be_in_init(parser *p, const ast *a) {

    if(a == NULL) return true; //if the expression is null, we already have an error

    switch(a->tag) {
        case ast_boolean_literal:
            return false;

        case ast_number_literal:
            return true;

        case ast_identifier:
            return shgeti(p->global_scope, a->identifier.value) != -1 || shgeti(p->declared_functions, a->identifier.value) != -1;

        case ast_grouped_assignment_stmt: {
            bool can = false;
            int n = arrlen(a->grouped_assignment_stmt.names);

            for(int i = 0; i < n; i++) {
                can = can_be_in_init(p, a->grouped_assignment_stmt.names[i]);
                if(!can) {
                    return false;
                }
            }

            return can;
        }
        case ast_expression_stmt:
            return can_be_in_init(p, a->expr_stmt);

        case ast_prefix_expression:
            return can_be_in_init(p, a->prefix_expr.right);

        case ast_infix_expression:
            return can_be_in_init(p, a->infix_expr.left) && can_be_in_init(p, a->infix_expr.right);

        case ast_call_expression: {
            int n = arrlen(a->call_expr.arguments);
            for(int i = 0; i < n; i++) {
                bool can = can_be_in_init(p, a->call_expr.arguments[i]);
                if(!can) return false;
            }

            return true;
        }

        default:
            return false;
    }
}

static void add_builtin_function(parser *p, char *name, int n_args) {
    declared_function_entry_value value = {1, n_args};
    shput(p->declared_functions, name, value);
}

static enum operator_precedence get_precedence(token t) {
    switch(t.type) {
        case EQ:
        case NOT_EQ:
            return EQUALS;
        case AND:
            return ANDP;
       case OR:
            return ORP;
       case LT:
       case GT:
       case LEQ:
       case GEQ:
            return LESSGREATER;
       case PLUS:
       case MINUS:
            return SUM;
       case SLASH:
       case ASTERISK:
            return PRODUCT;
       case LPAREN:
            return CALL;
       default:
            return LOWEST;

    }
}

static enum operator_precedence peek_precedence(parser *p) {
    return get_precedence(p->peek_token);
}

static enum operator_precedence cur_precedence(parser *p) {
    return get_precedence(p->cur_token);
}

parser *new_parser(lexer *l) {

    parser *p = (parser *) calloc(1, sizeof(parser));

    if(p == NULL) {
        fprintf(stderr, "%s - Error allocating memory for the parser!\n", __FUNCTION__);
        return NULL;
    }

    sh_new_arena(p->declared_functions);
    shdefault(p->declared_functions, (declared_function_entry_value){0});

    add_builtin_function(p, "acos", 1);
    add_builtin_function(p, "asin", 1);
    add_builtin_function(p, "atan", 1);
    add_builtin_function(p, "atan2", 1);
    add_builtin_function(p, "ceil", 1);
    add_builtin_function(p, "cos", 1);
    add_builtin_function(p, "cosh", 1);
    add_builtin_function(p, "exp", 1);
    add_builtin_function(p, "fabs", 1);
    add_builtin_function(p, "floor", 1);
    add_builtin_function(p, "fmod", 1);
    add_builtin_function(p, "frexp", 1);
    add_builtin_function(p, "ldexp", 1);
    add_builtin_function(p, "log", 1);
    add_builtin_function(p, "log10", 1);
    add_builtin_function(p, "modf", 2);
    add_builtin_function(p, "pow", 2);
    add_builtin_function(p, "sin", 1);
    add_builtin_function(p, "sinh", 1);
    add_builtin_function(p, "sqrt", 1);
    add_builtin_function(p, "tan", 1);
    add_builtin_function(p, "tanh", 1);
    add_builtin_function(p, "cosh", 1);
    add_builtin_function(p, "asinh", 1);
    add_builtin_function(p, "atanh", 1);
    add_builtin_function(p, "cbrt", 1);
    add_builtin_function(p, "copysign", 2);
    add_builtin_function(p, "erf", 1);
    add_builtin_function(p, "erfc", 1);
    add_builtin_function(p, "exp2", 2);
    add_builtin_function(p, "expm1", 2);
    add_builtin_function(p, "fdim", 2);
    add_builtin_function(p, "fma", 3);
    add_builtin_function(p, "fmax", 2);
    add_builtin_function(p, "fmin", 2);
    add_builtin_function(p, "hypot", 2);
    add_builtin_function(p, "ilogb", 1);
    add_builtin_function(p, "lgamma", 1);
    add_builtin_function(p, "llrint", 1);
    add_builtin_function(p, "lrint", 1);
    add_builtin_function(p, "llround", 1);
    add_builtin_function(p, "lround", 1);
    add_builtin_function(p, "log1p", 1);
    add_builtin_function(p, "log2", 1);
    add_builtin_function(p, "logb", 1);
    add_builtin_function(p, "nan", 1);
    add_builtin_function(p, "nearbyint", 1);
    add_builtin_function(p, "nextafter", 2);
    add_builtin_function(p, "nexttoward", 2);
    add_builtin_function(p, "remainder", 1);
    add_builtin_function(p, "remquo", 3);
    add_builtin_function(p, "rint", 1);
    add_builtin_function(p, "round", 1);
    add_builtin_function(p, "scalbln", 2);
    add_builtin_function(p, "scalbn", 2);
    add_builtin_function(p, "tgamma", 1);
    add_builtin_function(p, "trunc", 1);
    add_builtin_function(p, "print", 1);
    add_builtin_function(p, ODE_GET_VALUE, 2);
    add_builtin_function(p, ODE_GET_TIME, 2);
    add_builtin_function(p, ODE_GET_N_IT, 0);

    sh_new_arena(p->declared_variables);
    shdefault(p->declared_variables, ((declared_variable_entry_value){0}));

    //variable time is auto declared in the scope
    shput(p->declared_variables, "time", ((declared_variable_entry_value){0, true, 0, ast_global_stmt}));

    p->l = l;

    advance_token(p);
    advance_token(p);

    return p;
}

void free_parser(parser *p) {
    shfree(p->declared_variables);
    shfree(p->declared_functions);
    shfree(p->global_scope);
    shfree(p->local_scope);

    free(p);
}

bool check_parser_errors(parser *p, bool exit_on_error) {

    int err_len = arrlen(p->errors);

    if(!err_len) {
        return false;
    }

    for(int i = 0; i < err_len; i++) {
        fprintf(stderr, "%s", p->errors[i]);
    }

    if(exit_on_error) {
        exit(1);
    } else {
        return true;
    }
}

ast *parse_identifier(parser *p) {
    return make_identifier(&p->cur_token);
}

ast *parse_boolean_literal(parser *p) {
    return make_boolean_literal(&p->cur_token, cur_token_is(p, TRUE));
}

ast *parse_assignment_statement(parser *p, ast_tag tag, bool skip_ident) {
       
    ast *stmt = make_assignment_stmt(&p->cur_token, tag);

    if(!skip_ident) {
        if(tag == ast_ode_stmt) {
            if(!expect_peek(p, ODE_IDENT)) {
                RETURN_ERROR("ode identifier expected before assignment\n");
            }
        } else if(!expect_peek(p, IDENT)) {
            RETURN_ERROR("identifier expected before assignment\n");
        }
    }

    bool has_ode_symbol = (p->cur_token.literal[p->cur_token.literal_len - 1] == '\'');
    if(tag == ast_ode_stmt) {
        if(!has_ode_symbol) {
            RETURN_ERROR("ode identifiers needs to end with an ' \n");
        }
    }

    stmt->assignment_stmt.name = parse_identifier(p);
    if(!stmt->assignment_stmt.name) {
        return NULL;
    }

    if(tag == ast_global_stmt) {
        int builtin = shgeti(p->declared_functions, stmt->assignment_stmt.name->identifier.value);

        if(builtin != -1) {
            RETURN_ERROR("%.*s is a function name and cannot be used as an global identifier\n", p->cur_token.literal_len, p->cur_token.literal);
        }
    }

    if(!expect_peek(p, ASSIGN)) {
        RETURN_ERROR("= expected\n");
    }

    if(tag == ast_assignment_stmt) {
        declared_variable_entry_value value = {local_var_count, false, p->cur_token.line_number, tag};
        if(!has_ode_symbol) {
            shput(p->declared_variables, stmt->assignment_stmt.name->identifier.value, value);
            stmt->assignment_stmt.declaration_position = local_var_count;
            local_var_count++;
        }
    } else if(tag == ast_global_stmt) {
        declared_variable_entry_value value = {global_count, false, p->cur_token.line_number, tag};

        bool alread_declared = shgeti(p->global_scope, stmt->assignment_stmt.name->identifier.value) != -1;

        if(alread_declared) {
            RETURN_VALUE_AND_ERROR_EXPRESSION(stmt, "global variable %s has already been declared.\n", stmt->assignment_stmt.name->identifier.value);
        }

        shput(p->global_scope, stmt->assignment_stmt.name->identifier.value, value);
        stmt->assignment_stmt.declaration_position = global_count;
        global_count++;
    } else if(tag == ast_ode_stmt) {
        p->have_ode = true;
        char *tmp = strndup(stmt->assignment_stmt.name->identifier.value, (int) strlen(stmt->assignment_stmt.name->identifier.value) - 1);
        //The key in this hash is the order of appearance of the ODE. This is important to define the order of the initial conditions
        declared_variable_entry_value value = {ode_count, false, p->cur_token.line_number, tag};

        declared_variable_entry *existing_entry = stbds_shgetp_null(p->declared_variables, tmp);
        if(existing_entry != NULL) {
            printf("[WARN] - ODE %s redeclared on line %d! Ignore if it was intentionally redeclared!\n",
                    stmt->assignment_stmt.name->identifier.value,
                    p->cur_token.line_number);
            stmt->assignment_stmt.declaration_position = existing_entry->value.declaration_position;
        } else {
            shput(p->declared_variables, tmp, value);
            stmt->assignment_stmt.declaration_position = ode_count;
            ode_count++;
        }

        free(tmp);
    }

    if(p->cur_token.line_number != p->peek_token.line_number) {
        RETURN_VALUE_AND_ERROR_EXPRESSION(stmt, "error parsing assignment statement\n");
    }

    advance_token(p);

    stmt->assignment_stmt.value = parse_expression(p, LOWEST);

    if(peek_token_is(p, UNIT_DECL)) {
        advance_token(p);
        stmt->assignment_stmt.unit = strndup(p->cur_token.literal, p->cur_token.literal_len);
    }

    if(peek_token_is(p, SEMICOLON)) {
        advance_token(p);
    }

    if(peek_token_is(p, UNIT_DECL)) {
        advance_token(p);
        if (stmt->assignment_stmt.unit != NULL) {
            //TODO: warning about unit definition
        }
        stmt->assignment_stmt.unit = strndup(p->cur_token.literal, p->cur_token.literal_len);
    }

    return stmt;
}

ast *parse_return_statement(parser *p) {

    ast *stmt = make_return_stmt(&p->cur_token);

    advance_token(p);

    stmt->return_stmt.return_values = parse_expression_list(p, false);

    if(peek_token_is(p, SEMICOLON)) {
        advance_token(p);
    }

    return stmt;
}

ast *parse_import_statement(parser *p) {

    ast *stmt = make_import_stmt(&p->cur_token);

    if(!expect_peek(p, STRING)) {
        RETURN_ERROR("missing file name after import directive\n");
    }

    stmt->import_stmt.filename = make_string_literal(&p->cur_token);

    if(peek_token_is(p, SEMICOLON)) {
        advance_token(p);
    }

    return stmt;
}

ast **parse_block_statement(parser *p) {

    ast **statements = NULL;

    advance_token(p);

    while(!cur_token_is(p, RBRACE) && !cur_token_is(p, ENDOF)) {
        ast *stmt = parse_statement(p);

        if(stmt) {
            arrpush(statements, stmt); //NOLINT
        }
        advance_token(p);
    }

    return statements;
}

ast *parse_while_statement(parser *p) {

    ast *exp = make_while_stmt(&p->cur_token);

    if(!expect_peek(p, LPAREN)) {
        RETURN_ERROR("( expected\n");
    }

    advance_token(p);

    exp->while_stmt.condition = parse_expression(p, LOWEST);

    if(!expect_peek(p, RPAREN)) {
        RETURN_ERROR(") expected\n");
    }

    if(!expect_peek(p, LBRACE)) {
        RETURN_ERROR("{ expected\n");
    }

    exp->while_stmt.body = parse_block_statement(p);

    return exp;
}

ast *parse_number_literal(parser *p) {
    ast *lit = make_number_literal(&p->cur_token);
    char *end;
    double value = strtod(p->cur_token.literal, &end);
    if(p->cur_token.literal == end) {
        RETURN_ERROR("could not parse %.*s as number\n", p->cur_token.literal_len, p->cur_token.literal);
    }

    lit->num_literal.value = value;
    return lit;
}

ast *parse_string_literal(parser *p) {
    return make_string_literal(&p->cur_token);
}

ast *parse_prefix_expression(parser *p) {

    ast *expression = make_prefix_expression(&p->cur_token);
    advance_token(p);
    expression->prefix_expr.right = parse_expression(p, PREFIX);

    return expression;
}

ast *parse_infix_expression(parser *p, ast *left) {

    ast *expression = make_infix_expression(&p->cur_token, left);

    enum operator_precedence precedence = cur_precedence(p);

    advance_token(p);

    expression->infix_expr.right = parse_expression(p, precedence);

    return expression;
}

ast *parse_grouped_expression(parser *p) {

    advance_token(p);
    ast *exp = parse_expression(p, LOWEST);

    if(!expect_peek(p, RPAREN)) {
        RETURN_ERROR(") expected\n");
    }

    return exp;
}

ast **parse_grouped_assignment_names(parser *p) {
    advance_token(p);

    ast **identifiers = NULL;

    ast *ident = parse_identifier(p);
    arrput(identifiers, ident); //NOLINT

    declared_variable_entry_value value = {p->cur_token.line_number, false, p->cur_token.line_number, ast_grouped_assignment_stmt};

    shput(p->declared_variables, ident->identifier.value, value);

    while(peek_token_is(p, COMMA)) {
        advance_token(p);
        advance_token(p);
        ident = parse_identifier(p);
        arrput(identifiers, ident); //NOLINT
        value = (declared_variable_entry_value){p->cur_token.line_number, false, p->cur_token.line_number, ast_grouped_assignment_stmt};
        shput(p->declared_variables, ident->identifier.value, value);
    }

    return identifiers;
}

ast *parse_grouped_assignment(parser *p) {

    ast *stmt = make_grouped_assignment_stmt(&p->cur_token);

    if(peek_token_is(p, RBRACKET)) {
        RETURN_ERROR("expected at least one identifier\n");
    }

    stmt->grouped_assignment_stmt.names = parse_grouped_assignment_names(p);

    if(!expect_peek(p, RBRACKET)) {
        RETURN_ERROR("] expected\n");
    }

    if(!expect_peek(p, ASSIGN)) {
        RETURN_ERROR("= expected\n");
    }

    advance_token(p);

    stmt->grouped_assignment_stmt.call_expr = parse_expression(p, LOWEST);

    if(stmt->grouped_assignment_stmt.call_expr->tag != ast_call_expression) {
        RETURN_ERROR("grouped expressions are only supported with function calls\n");
    }

    if(peek_token_is(p, SEMICOLON)) {
        advance_token(p);
    }

    return stmt;
}


ast *parse_if_expression(parser *p) {

    ast *exp = make_if_expression(&p->cur_token);

    if(!expect_peek(p, LPAREN)) {
        sds msg = sdscatprintf(sdsempty(), "Parse error on line %d: ( expected\n", p->cur_token.line_number);
        arrput(p->errors, msg);
        return NULL;
    }

    advance_token(p);

    exp->if_expr.condition = parse_expression(p, LOWEST);

    if(!expect_peek(p, RPAREN)) {
        RETURN_ERROR(") expected\n");
    }

    if(!expect_peek(p, LBRACE)) {
        RETURN_ERROR("{ expected\n");
    }

    exp->if_expr.consequence = parse_block_statement(p);

    if(peek_token_is(p, ELSE)) {
        advance_token(p);
        if(!expect_peek(p, LBRACE)) {
            RETURN_ERROR("} expected\n");
        }
        exp->if_expr.alternative = parse_block_statement(p);
    }

    else if(peek_token_is(p, ELIF)) {
        advance_token(p);
        exp->if_expr.elif_alternative = parse_if_expression(p);
    }

    return exp;
}

ast **parse_function_parameters(parser *p) {
    ast **identifiers = NULL;

    if(peek_token_is(p, RPAREN)) {
        advance_token(p);
        return identifiers;
    }

    advance_token(p);

    ast *ident = parse_identifier(p);

    arrput(identifiers, ident); //NOLINT

    while(peek_token_is(p, COMMA)) {
        advance_token(p);
        advance_token(p);
        ident = parse_identifier(p);
        arrput(identifiers, ident); //NOLINT
    }

    if(!expect_peek(p, RPAREN)) {
        RETURN_ERROR(") expected\n");
    }

    return identifiers;
}

void count_return(ast *a, program *return_stmts);

static void count_in_expression(ast *a, program *stmts) {

    if(a->expr_stmt != NULL) {
        count_return(a->expr_stmt, stmts);
    }
}

static void count_in_if(ast *a, program *stmts) {

    int n = arrlen(a->if_expr.consequence);
    for(int i = 0; i < n; i++) {
        count_return(a->if_expr.consequence[i], stmts);
    }

    n = arrlen(a->if_expr.alternative);

    if(n) {
        for(int i = 0; i < n; i++) {
            count_return(a->if_expr.alternative[i], stmts);
        }
    }
}

static void count_in_while(ast *a, program *stmts) {

    int n = arrlen(a->while_stmt.body);
    for(int i = 0; i < n; i++) {
        count_return(a->while_stmt.body[i], stmts);
    }
}

void count_return(ast *a, program *stmts) {

    if(a->tag == ast_return_stmt) {
        arrput(*stmts, a); //NOLINT
    }

    if(a->tag == ast_expression_stmt) {
        return count_in_expression(a, stmts);
    }

    if(a->tag == ast_if_expr) {
        return count_in_if(a, stmts);
    }

    if(a->tag == ast_while_stmt) {
        return count_in_while(a, stmts);
    }
}

ast *parse_function_statement(parser *p) {

    ast *stmt = make_function_statement(&p->cur_token);

    if(!expect_peek(p, IDENT)) {
        RETURN_ERROR("expected identifier after fn statement\n");
    }

    stmt->function_stmt.name = parse_identifier(p);
    //TODO: create a declared function hash map

    if(!expect_peek(p, LPAREN)) {
        RETURN_ERROR("( expected\n");
    }

    stmt->function_stmt.parameters = parse_function_parameters(p);

    if(!expect_peek(p, LBRACE)) {
        RETURN_ERROR("{ expected\n");
    }

    stmt->function_stmt.body = parse_block_statement(p);

    int len = arrlen(stmt->function_stmt.body);

    int return_len = 0;

    program return_stmts = NULL;
    for(int i = 0; i < len; i++) {
        count_return(stmt->function_stmt.body[i], &return_stmts);
    }

    len = arrlen(return_stmts);

    if(len) {

        return_len = arrlen(return_stmts[0]->return_stmt.return_values);

        for(int i = 1; i < len; i++) {
            int tmp = arrlen(return_stmts[i]->return_stmt.return_values);

            if(tmp != return_len) {
                RETURN_ERROR("a function has always to return the same number of values\n");
            }
        }
    }

    arrfree(return_stmts);

    stmt->function_stmt.num_return_values = return_len;

    int n_args = arrlen(stmt->function_stmt.parameters);

    declared_function_entry_value value = {return_len, n_args};
    shput(p->declared_functions, stmt->function_stmt.name->identifier.value, value);

    return stmt;
}

ast **parse_expression_list(parser *p, bool with_paren) {

    ast **list = NULL;

    if(with_paren) {
        if(peek_token_is(p, RPAREN)) {
            advance_token(p);
            return list;
        }

        advance_token(p);
    }

    arrpush(list, parse_expression(p, LOWEST)); //NOLINT

    while(peek_token_is(p, COMMA)) {
        advance_token(p);
        advance_token(p);
        arrpush(list, parse_expression(p, LOWEST)); //NOLINT
    }

    if(with_paren) {
        if(!expect_peek(p, RPAREN)) {
            RETURN_ERROR(") expected\n");
        }
    }

    return list;
}

ast *parse_call_expression(parser *p, ast *function) {
    ast *exp = make_call_expression(&p->cur_token, function);
    exp->call_expr.arguments = parse_expression_list(p, true);
    return exp;
}

ast *parse_expression(parser *p, enum operator_precedence precedence) {

    ast *left_expr;

    //prefix expression
    if(TOKEN_TYPE_EQUALS(p->cur_token, IDENT)) {
        left_expr = parse_identifier(p);
    } else if(TOKEN_TYPE_EQUALS(p->cur_token, NUMBER)) {
        left_expr = parse_number_literal(p);
    } else if(TOKEN_TYPE_EQUALS(p->cur_token, STRING)) {
        left_expr = parse_string_literal(p);
    } else if(TOKEN_TYPE_EQUALS(p->cur_token, TRUE) || TOKEN_TYPE_EQUALS(p->cur_token, FALSE)) {
        left_expr = parse_boolean_literal(p);
    } else if(TOKEN_TYPE_EQUALS(p->cur_token, BANG) || TOKEN_TYPE_EQUALS(p->cur_token, MINUS)) {
        left_expr = parse_prefix_expression(p);
    } else if(TOKEN_TYPE_EQUALS(p->cur_token, LPAREN)) {
        left_expr = parse_grouped_expression(p);
    } else if(TOKEN_TYPE_EQUALS(p->cur_token, IF)) {
        left_expr = parse_if_expression(p);
    } else {
        RETURN_ERROR_EXPRESSION("error parsing expression\n");
    }

    bool is_infix = TOKEN_TYPE_EQUALS(p->peek_token, PLUS)     ||
                    TOKEN_TYPE_EQUALS(p->peek_token, MINUS)    ||
                    TOKEN_TYPE_EQUALS(p->peek_token, SLASH)    ||
                    TOKEN_TYPE_EQUALS(p->peek_token, ASTERISK) ||
                    TOKEN_TYPE_EQUALS(p->peek_token, EQ)       ||
                    TOKEN_TYPE_EQUALS(p->peek_token, NOT_EQ)   ||
                    TOKEN_TYPE_EQUALS(p->peek_token, LT)       ||
                    TOKEN_TYPE_EQUALS(p->peek_token, GT)       ||
                    TOKEN_TYPE_EQUALS(p->peek_token, LEQ)      ||
                    TOKEN_TYPE_EQUALS(p->peek_token, GEQ)      ||
                    TOKEN_TYPE_EQUALS(p->peek_token, AND)      ||
                    TOKEN_TYPE_EQUALS(p->peek_token, OR)       ||
                    TOKEN_TYPE_EQUALS(p->peek_token, LPAREN);

    if((cur_token_is(p, IDENT) || cur_token_is(p, NUMBER)) && (p->cur_token.line_number == p->peek_token.line_number)) {

        bool is_valid = TOKEN_TYPE_EQUALS(p->peek_token, COMMA)     ||
                        TOKEN_TYPE_EQUALS(p->peek_token, RPAREN)    ||
                        TOKEN_TYPE_EQUALS(p->peek_token, UNIT_DECL) ||
                        TOKEN_TYPE_EQUALS(p->peek_token, RBRACE)    ||
                        TOKEN_TYPE_EQUALS(p->peek_token, ENDOF)     ||
                        TOKEN_TYPE_EQUALS(p->peek_token, SEMICOLON);

        if(!(is_infix || is_valid )) {
            if(cur_token_is(p, IDENT)) {
                RETURN_VALUE_AND_ERROR_EXPRESSION(left_expr, "error parsing expression, expected operator after an identifier!\n");
            } else {
                RETURN_VALUE_AND_ERROR_EXPRESSION(left_expr, "error parsing expression, expected operator after an numerical value!\n");
            }
        }
    }

    //infix expression
    while(!peek_token_is(p, SEMICOLON) && precedence < peek_precedence(p)) {

        if(!is_infix) {
            return left_expr;
        }

        advance_token(p);

        if(TOKEN_TYPE_EQUALS(p->cur_token, LPAREN)) {
            left_expr = parse_call_expression(p, left_expr);
        } else {
            if(p->cur_token.line_number != p->peek_token.line_number) {
                RETURN_VALUE_AND_ERROR_EXPRESSION(left_expr, "error parsing infix expression\n");
            }
            left_expr = parse_infix_expression(p, left_expr);
        }
    }

    return left_expr;
}

ast *parse_expression_statement(parser *p) {

    ast *stmt = make_expression_stmt(&p->cur_token);
    stmt->expr_stmt = parse_expression(p, LOWEST);
    if(peek_token_is(p, SEMICOLON)) {
        advance_token(p);
    }
    return stmt;
}

ast *parse_statement(parser *p) {

    if(cur_token_is(p, IDENT) && peek_token_is(p, ASSIGN)) {
        return parse_assignment_statement(p, ast_assignment_stmt, true);
    }

    if(cur_token_is(p, ODE_IDENT) && peek_token_is(p, ASSIGN)) {
        return parse_assignment_statement(p, ast_ode_stmt, true);
    }

    if(cur_token_is(p, ODE)) {
        return parse_assignment_statement(p, ast_ode_stmt, false);
    }

    if(cur_token_is(p, INITIAL)) {
        return parse_assignment_statement(p, ast_initial_stmt, false);
    }

    if(cur_token_is(p, GLOBAL)) {
        return parse_assignment_statement(p, ast_global_stmt, false);
    }

    if(cur_token_is(p, RETURN_STMT)) {
        return parse_return_statement(p);
    }

    if(cur_token_is(p, WHILE)) {
        return parse_while_statement(p);
    }

    if(cur_token_is(p, FUNCTION) || cur_token_is(p, ENDFUNCTION)) {
        return parse_function_statement(p);
    }

    if(cur_token_is(p, IMPORT)) {
        return parse_import_statement(p);
    }

    if(cur_token_is(p, LBRACKET)) {
        return parse_grouped_assignment(p);
    }

    return parse_expression_statement(p);
}

static void check_variable_declarations(parser *p, program program);

static void check_declaration(parser *p, ast *src) {

    if(src == NULL) return;

    switch(src->tag) {

        case ast_string_literal:
        case ast_number_literal:
        case ast_boolean_literal:
            break;

        case ast_identifier:
            {
                char *id_name = src->identifier.value;

                bool is_local = shgeti(p->declared_variables, id_name) != -1;
                bool is_global = shgeti(p->global_scope, id_name) != -1;
                bool is_function = shgeti(p->declared_functions, id_name) != -1;

                bool is_fn_param = false;

                if(!is_local && !is_global && !is_function) {
                    is_fn_param = shgeti(p->local_scope, id_name) != -1;
                }

                if(!is_local && !is_global && !is_function && !is_fn_param) {
                    ADD_ERROR_WITH_LINE(src->token.line_number, src->token.file_name, "Identifier %s not declared, assign a value to it before using!\n",
                                        id_name);
                }

            }
            break;
        case ast_assignment_stmt:
            {
                char *id_name = src->assignment_stmt.name->identifier.value;
                size_t s = strlen(id_name) - 1;
                bool has_ode_symbol = (id_name[s] == '\'');

                if(has_ode_symbol) {
                    char *tmp = strndup(id_name, s);

                    int i = shgeti(p->declared_variables, tmp);

                    free(tmp);

                    if(i == -1) {
                        ADD_ERROR_WITH_LINE(src->token.line_number, src->token.file_name, "ODE %s not declared. Declare with 'ode %s = expr' before using!\n",
                                            id_name, id_name);
                    } else {
                        if(p->declared_variables[i].value.line_number > src->token.line_number) {
                            ADD_ERROR_WITH_LINE(src->token.line_number, src->token.file_name, "ODE %s is being assigned before being declared!\n", id_name);
                        } else {
                            src->assignment_stmt.declaration_position = p->declared_variables[i].value.declaration_position;
                        }
                    }
                }

                check_declaration(p, src->assignment_stmt.value);
            }
            break;
        case ast_ode_stmt:
            case ast_global_stmt:
            {
                check_declaration(p, src->assignment_stmt.value);
                if(src->assignment_stmt.value != NULL && src->assignment_stmt.value->tag == ast_call_expression) {
                    char *f_name = src->assignment_stmt.value->call_expr.function_identifier->identifier.value;

                    declared_function_entry dv = shgets(p->declared_functions, f_name);
                    int num_return_values = dv.value.n_returns;

                    if(num_return_values != 1) {
                        ADD_ERROR_WITH_LINE(src->token.line_number, src->token.file_name, "Function %s returns %d values but 1 is being assigned!\n", f_name,
                                            num_return_values);
                    }
                }
                if(src->tag == ast_assignment_stmt) {

                    bool var_is_global = shgeti(p->global_scope, src->assignment_stmt.name->identifier.value) != -1;
                    if(var_is_global) {
                        printf("%s is global\n", src->assignment_stmt.name->identifier.value);
                        src->assignment_stmt.name->identifier.global = true;
                    }
                }
            }
            break;

        case ast_initial_stmt:
            {
                check_declaration(p, src->assignment_stmt.value);
                if(!can_be_in_init(p, src->assignment_stmt.value)) {
                    ADD_ERROR_WITH_LINE(src->token.line_number, src->token.file_name,
                                        "ODE variables can only be initialized with function calls with no parameters, global variables or numerical values.\n");
                }

                char *ode_name = src->assignment_stmt.name->identifier.value;

                int i = shgeti(p->declared_variables, ode_name);

                if(i != -1) {

                    if(p->declared_variables[i].value.initialized) {
                        ADD_ERROR_WITH_LINE(src->token.line_number, src->token.file_name, "Duplicate initialization ode variable %s\n", ode_name);
                    }

                    if(p->declared_variables[i].value.tag != ast_ode_stmt) {
                        ADD_ERROR_WITH_LINE(src->token.line_number, src->token.file_name, "Initialization of a non ode variable (%s).\n", ode_name);
                    }

                    p->declared_variables[i].value.initialized = true;
                    src->assignment_stmt.declaration_position = p->declared_variables[i].value.declaration_position;
                } else {
                    ADD_ERROR_WITH_LINE(src->token.line_number, src->token.file_name, "ODE %s' not declared.\n", ode_name);
                }
            }
            break;
        case ast_grouped_assignment_stmt:
            {
                check_declaration(p, src->grouped_assignment_stmt.call_expr);

                int n = arrlen(src->grouped_assignment_stmt.names);

                for(int i = 0; i < n; i++) {
                    bool var_is_global = shgeti(p->global_scope, src->grouped_assignment_stmt.names[i]->identifier.value) != -1;
                    if(var_is_global) {
                        src->grouped_assignment_stmt.names[i]->identifier.global = true;
                    }
                }

                if(n > 1) {

                    char *f_name = src->grouped_assignment_stmt.call_expr->call_expr.function_identifier->identifier.value;
                    declared_function_entry dv = shgets(p->declared_functions, f_name);

                    int num_expected_assignments = dv.value.n_returns;

                    if(n != num_expected_assignments) {
                        ADD_ERROR_WITH_LINE(src->token.line_number, src->token.file_name, "Function %s returns %d value(s) but %d are being assigned!\n", f_name,
                                            num_expected_assignments, n);
                    }
                }
            }
            break;

        case ast_function_statement:
            {
                int n = arrlen(src->function_stmt.body);

                int np = arrlen(src->function_stmt.parameters);

                for(int i = 0; i < np; i++) {
                    char *var_name = src->function_stmt.parameters[i]->identifier.value;
                    declared_variable_entry var_entry = {var_name, {0}};
                    shputs(p->local_scope, var_entry);
                }

                for(int i = 0; i < n; i++) {
                    check_declaration(p, src->function_stmt.body[i]);
                }

                for(int i = 0; i < np; i++) {
                    char *var_name = src->function_stmt.parameters[i]->identifier.value;
                    (void)shdel(p->local_scope, var_name);
                }
            }
            break;
        case ast_return_stmt:
            {
                int nr = arrlen(src->return_stmt.return_values);
                for (int i = 0; i < nr; i++) {
                    check_declaration(p, src->return_stmt.return_values[i]);
                }
            }
            break;
        case ast_expression_stmt:
            check_declaration(p, src->expr_stmt);
            break;
        case ast_while_stmt:
            {
                check_declaration(p, src->while_stmt.condition);
                int n = arrlen(src->while_stmt.body);
                for (int i = 0; i < n; i++) {
                    check_declaration(p, src->while_stmt.body[i]);
                }
            }
            break;
        case ast_import_stmt:
            break;
        case ast_prefix_expression:
            check_declaration(p, src->prefix_expr.right);
            break;
        case ast_infix_expression:
            check_declaration(p, src->infix_expr.left);
            check_declaration(p, src->infix_expr.right);
            break;
        case ast_if_expr:
            check_declaration(p, src->if_expr.condition);
            check_variable_declarations(p, src->if_expr.consequence);
            check_variable_declarations(p, src->if_expr.alternative);
            check_declaration(p, src->if_expr.elif_alternative);
            break;
        case ast_call_expression:
            {
                check_declaration(p, src->call_expr.function_identifier);
                check_variable_declarations(p, src->call_expr.arguments);

                char *f_name = src->call_expr.function_identifier->identifier.value;
                declared_function_entry dv = shgets(p->declared_functions, f_name);

                int num_expected_args = dv.value.n_args;
                int n_real_args = arrlen(src->call_expr.arguments);
                if(n_real_args != num_expected_args) {
                    ADD_ERROR_WITH_LINE(src->token.line_number, src->token.file_name, "Function %s expects %d parameters but %d are being passed!\n", f_name,
                                        num_expected_args, n_real_args);
                }
            }
            break;
    }
}

static void check_variable_declarations(parser *p, program program) {

    int n = arrlen(program);

    for(int i = 0; i < n; i++) {
        check_declaration(p, program[i]);
    }
}

static void check_ode_initializations(parser *p, program program) {

    int n = arrlen(program);

    for(int i = 0; i < n; i++) {
        if(program[i]->tag == ast_ode_stmt) {

            ast *src = program[i];
            char *ode_name = strndup(src->assignment_stmt.name->identifier.value, (int) strlen(src->assignment_stmt.name->identifier.value) - 1);
            declared_variable_entry dv = shgets(p->declared_variables, ode_name);

            if(!dv.value.initialized) {
                fprintf(stderr, "Warning - No initial condition provided for %s' (declared on line %d)!\n", ode_name, dv.value.line_number);
            }
            free(ode_name);
        }
    }
}

static void process_imports(parser *p, program original_program, char *import_path) {

    int n = arrlen(original_program);

    for(int i = 0; i < n; i++) {
        ast *a = original_program[i];

        if(a->tag != ast_import_stmt) continue;

        sds import_file_name;

        if(import_path == NULL) {
            import_file_name = sdsnew(a->import_stmt.filename->identifier.value);
        } else {
            import_file_name = sdscatprintf(sdsempty(), "%s/%s", import_path, a->import_stmt.filename->identifier.value);
        }

        size_t file_size;

        char *source = read_entire_file_with_mmap(import_file_name, &file_size);

        if(!source) {
            fprintf(stderr, "Error importing file %s.\n", import_file_name);
            sdsfree(import_file_name);
            exit(0);
        }

        lexer *l = new_lexer(source, import_file_name);
        parser *parser_new = new_parser(l);
        program program_new = parse_program(parser_new, false, true, NULL);

        int n_stmt = arrlen(program_new);
        for(int s = 0; s < n_stmt; s++) {
            //we only import functions for now
            if(program_new[s]->tag == ast_function_statement) {
                int exists = shgeti(p->declared_functions, program_new[s]->function_stmt.name->identifier.value) != -1;
                if(exists) {
                    ADD_ERROR("Function %s alread exists.\n", program_new[s]->function_stmt.name->identifier.value);
                } else {
                    declared_function_entry fn_entry = shgets(parser_new->declared_functions, program_new[s]->function_stmt.name->identifier.value);
                    shputs(p->declared_functions, fn_entry);
                    arrput(original_program, program_new[s]); //NOLINT
                }

            } else if(program_new[s]->tag == ast_global_stmt) {
                int exists = shgeti(p->global_scope, program_new[s]->assignment_stmt.name->identifier.value) != -1;
                if(exists) {
                    ADD_ERROR("Global variable %s alread exists.\n", program_new[s]->function_stmt.name->identifier.value);
                } else {
                    declared_variable_entry var_entry = shgets(parser_new->global_scope, program_new[s]->function_stmt.name->identifier.value);
                    shputs(p->global_scope, var_entry);

                    arrput(original_program, program_new[s]); //NOLINT
                }
            } else {
                free_ast(program_new[s]);
                fprintf(stderr, "[WARN] - Importing from file %s in line %d. Currently, we only import functions or global variables. Nested imports will not be imported!\n", import_file_name, program_new[s]->token.line_number);
            }
        }

        sdsfree(import_file_name);
        free_lexer(l);
        free_parser(parser_new);
        arrfree(program_new);
        munmap(source, file_size);
    }
}

static program parse_program_helper(parser *p, bool proc_imports, bool check_errors, bool exit_on_error, char *import_path) {

    global_count = 1;
    local_var_count = 1;
    ode_count = 1;

    program program = NULL;

    while(TOKEN_TYPE_NOT_EQUALS(p->cur_token, ENDOF)) {
        ast *stmt = parse_statement(p);
        if(stmt != NULL) {
            arrput(program, stmt);
        }
        advance_token(p);
    }

    if(proc_imports) {
        process_imports(p, program, import_path);
    }

    check_variable_declarations(p, program);
    check_ode_initializations(p, program);

    if(check_errors) {

        //if process_imports==false we are comming from a imported file
        if(proc_imports && !p->have_ode) {
            ADD_ERROR("No ODE(s) defined\n");
        }

        if(check_parser_errors(p, exit_on_error)) {
            free_program(program);
            return NULL;
        }
    }

    return program;
}

program parse_program_without_exiting_on_error(parser *p, bool proc_imports, bool check_errors, char *import_path) {
    return parse_program_helper(p, proc_imports, check_errors, false, import_path);
}

program parse_program(parser *p, bool proc_imports, bool check_errors, char *import_path) {
    return parse_program_helper(p, proc_imports, check_errors, true, import_path);
}
