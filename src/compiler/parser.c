#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "../stb/stb_ds.h"
#include "parser.h"

#define ERROR_PREFIX "Parse error on line %d of file %s: "
#define NEW_ERROR_PREFIX sdscatprintf(sdsempty(), ERROR_PREFIX, p->cur_token.line_number, p->l->file_name)

ast ** parse_expression_list(parser *, bool);

void print_program(program p) {

    sds *p_str = program_to_string(p);

    int n = arrlen(p_str);

    for(int i = 0; i < n; i++) {
        printf("%s\n", p_str[i]);
    }
}

bool check_parser_errors(parser *p, bool exit_on_error) {

    int err_len = arrlen(p->errors);

    if (!err_len) {
        return false;
    }

    fprintf(stderr, "parser has %d errors\n", err_len);

    for(int i = 0; i < err_len; i++) {
        fprintf(stderr, "%s", p->errors[i]);
    }

    if(exit_on_error) {
        exit(1);
    }
    else {
        return true;
    }
}

parser * new_parser(lexer *l) {

    parser *p = (parser*)calloc(1, sizeof(parser));
    p->l = l;

    advance_token(p);
    advance_token(p);

    return p;
}

void advance_token(parser *p) {
    p->cur_token = p->peek_token;
    p->peek_token = next_token(p->l);
}

bool cur_token_is(parser *p, token_type t) {
    return p->cur_token.type == t;
}

bool peek_token_is(parser *p, token_type t) {
    return p->peek_token.type == t;
}


bool expect_peek(parser *p, token_type t) {
    if(peek_token_is(p, t)) {
        advance_token(p);
        return true;
    } else {
        peek_error(p, t);
        return false;
    }
}

ast * parse_assignment_statement(parser *p, ast_tag tag, bool skip_ident) {

    ast *stmt = make_assignement_stmt(p->cur_token, tag);

    if(!skip_ident) {
        if (!expect_peek(p, IDENT)) {
            sds msg = NEW_ERROR_PREFIX;
            msg = sdscatprintf(msg, "identifier expected before assignment\n");
            arrput(p->errors, msg);
            return NULL;
        }
    }

    bool has_ode_symbol = (p->cur_token.literal[strlen(p->cur_token.literal)-1] == '\'');
    if(tag == ast_ode_stmt) {
        if(!has_ode_symbol) {
            sds msg = NEW_ERROR_PREFIX;
            msg = sdscatprintf(msg, "ode identifiers needs to end with an ' \n");
            arrput(p->errors, msg);
            return NULL;
        }
    }
    else {
        if(has_ode_symbol) {
            sds msg = NEW_ERROR_PREFIX;
            msg = sdscatprintf(msg, "only ode identifiers can end with an ' \n");
            arrput(p->errors, msg);
            return NULL;
        }
    }

    stmt->assignement_stmt.name = make_identifier(p->cur_token, p->cur_token.literal);

    if (!expect_peek(p, ASSIGN)) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, "= expected\n");
        arrput(p->errors, msg);
        return NULL;
    }

    advance_token(p);

    stmt->assignement_stmt.value = parse_expression(p, LOWEST);

    if(peek_token_is(p, SEMICOLON)) {
        advance_token(p);
    }

    return stmt;
}



ast *parse_return_statement(parser *p) {

    ast *stmt = make_return_stmt(p->cur_token);

    advance_token(p);

    stmt->return_stmt.return_values = parse_expression_list(p, false);

    if(peek_token_is(p, SEMICOLON)) {
        advance_token(p);
    }

    return stmt;
}

ast *parse_import_statement(parser *p) {

    ast *stmt = make_import_stmt(p->cur_token);

    if(!expect_peek(p, STRING)) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, "missing file name after import directive\n");
        arrput(p->errors, msg);
        return NULL;
    }

    stmt->import_stmt.filename = make_string_literal(p->cur_token, p->cur_token.literal);

    if(peek_token_is(p, SEMICOLON)) {
        advance_token(p);
    }

    return stmt;
}

ast ** parse_block_statement(parser *p) {

    ast **statements = NULL;

    advance_token(p);

    while(!cur_token_is(p, RBRACE) && !cur_token_is(p, ENDOF) ) {
        ast *stmt = parse_statement(p);

        if(stmt) {
            arrpush(statements, stmt);
        }
        advance_token(p);
    }

    return statements;

}

ast *parse_while_statement(parser *p) {

    ast *exp = make_while_stmt(p->cur_token);

    if(!expect_peek(p, LPAREN)) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, "( expected\n");
        arrput(p->errors, msg);
        return NULL;
    }

    advance_token(p);

    exp->while_stmt.condition = parse_expression(p, LOWEST);

    if(!expect_peek(p, RPAREN)) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, ") expected\n");
        arrput(p->errors, msg);
        return NULL;
    }

    if(!expect_peek(p, LBRACE)) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, "{ expected\n");
        arrput(p->errors, msg);
        return NULL;
    }

    exp->while_stmt.body = parse_block_statement(p);

    return exp;
}

ast *parse_identifier(parser *p) {
    return make_identifier(p->cur_token, p->cur_token.literal);
}

ast *parse_boolean_literal(parser *p) {
    return make_boolean_literal(p->cur_token, cur_token_is(p, TRUE));
}

ast *parse_number_literal(parser *p) {
    ast *lit = make_number_literal(p->cur_token);
    char *end;
    double value = strtod(p->cur_token.literal, &end);
    if(p->cur_token.literal == end) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, "could not parse %s as number\n", p->cur_token.literal);
        arrput(p->errors, msg);
        return NULL;
    }

    lit->num_literal.value = value;
    return lit;
}

ast *parse_string_literal(parser *p) {
    return make_string_literal(p->cur_token, p->cur_token.literal);
}

ast *parse_prefix_expression(parser *p) {

    ast *expression = make_prefix_expression(p->cur_token, p->cur_token.literal);
    advance_token(p);
    expression->prefix_expr.right = parse_expression(p, PREFIX);

    return expression;

}

ast *parse_infix_expression(parser *p, ast *left) {

    ast *expression = make_infix_expression(p->cur_token, p->cur_token.literal, left);

    enum operator_precedence precedence = cur_precedence(p);

    advance_token(p);

    expression->infix_expr.right = parse_expression(p, precedence);

    return expression;

}

ast *parse_grouped_expression(parser *p) {

    advance_token(p);
    ast *exp = parse_expression(p, LOWEST);

    if(!expect_peek(p, RPAREN)) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, ") expected\n");
        arrput(p->errors, msg);
        return NULL;
    }

    return exp;
}

ast **parse_grouped_assignment_names(parser *p) {
    advance_token(p);

    ast ** identifiers = NULL;

    ast *ident = make_identifier(p->cur_token, p->cur_token.literal);
    arrput(identifiers, ident);

    while (peek_token_is(p, COMMA)) {
        advance_token(p);
        advance_token(p);
        ident = make_identifier(p->cur_token, p->cur_token.literal);
        arrput(identifiers, ident);
    }

    return identifiers;
}

ast *parse_grouped_assignment(parser *p) {

    ast *stmt = make_grouped_assignement_stmt(p->cur_token);

    if (peek_token_is(p, RBRACKET)) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, "expected at least one identifier\n");
        arrput(p->errors, msg);
        return NULL;
    }

    stmt->grouped_assignement_stmt.names = parse_grouped_assignment_names(p);

    if (!expect_peek(p, RBRACKET)) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, "] expected\n");
        arrput(p->errors, msg);
        return NULL;
    }

    if (!expect_peek(p, ASSIGN)) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, "= expected\n");
        arrput(p->errors, msg);
        return NULL;
    }

    advance_token(p);

    stmt->grouped_assignement_stmt.call_expr = parse_expression(p, LOWEST);

    if(stmt->grouped_assignement_stmt.call_expr->tag != ast_call_expression) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, "grouped expressions are only supported with function calls\n");
        arrput(p->errors, msg);
        return NULL;
    }

    if(peek_token_is(p, SEMICOLON)) {
        advance_token(p);
    }

    return stmt;
}


ast *parse_if_expression(parser *p) {

    ast *exp = make_if_expression(p->cur_token);

    if(!expect_peek(p, LPAREN)) {
        sds msg = sdscatprintf(sdsempty(), "Parse error on line %d: ( expected\n", p->cur_token.line_number);
        arrput(p->errors, msg);
        return NULL;
    }

    advance_token(p);

    exp->if_expr.condition = parse_expression(p, LOWEST);

    if(!expect_peek(p, RPAREN)) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, ") expected\n");
        arrput(p->errors, msg);
        return NULL;
    }

    if(!expect_peek(p, LBRACE)) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, "{ expected\n");
        arrput(p->errors, msg);
        return NULL;
    }

    exp->if_expr.consequence = parse_block_statement(p);

    if (peek_token_is(p, ELSE)) {
        advance_token(p);
        if (!expect_peek(p, LBRACE)) {
            sds msg = NEW_ERROR_PREFIX;
            msg = sdscatprintf(msg, "} expected\n");
            arrput(p->errors, msg);
            return NULL;
        }
        exp->if_expr.alternative = parse_block_statement(p);
    }

    return exp;
}

ast ** parse_function_parameters(parser *p) {
    ast ** identifiers = NULL;

    if (peek_token_is(p, RPAREN)) {
        advance_token(p);
        return identifiers;
    }

    advance_token(p);

    ast *ident = make_identifier(p->cur_token, p->cur_token.literal);

    arrput(identifiers, ident);

    while (peek_token_is(p, COMMA)) {
        advance_token(p);
        advance_token(p);
        ident = make_identifier(p->cur_token, p->cur_token.literal);
        arrput(identifiers, ident);
    }

    if (!expect_peek(p, RPAREN)) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, ") expected\n");
        arrput(p->errors, msg);
        return NULL;
    }

    return identifiers;

}

void count_return(ast *a, program *return_stmts);

static void count_in_expression(ast *a, program *stmts) {

    if (a->expr_stmt != NULL) {
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
        arrput(*stmts, a);
    }

    if(a->tag == ast_expression_stmt) {
        return count_in_expression(a,stmts);
    }

    if(a->tag == ast_if_expr) {
        return count_in_if(a, stmts);
    }

    if(a->tag == ast_while_stmt) {
        return count_in_while(a, stmts);
    }
}

ast *parse_function_statement(parser *p) {

    ast * stmt = make_function_statement(p->cur_token);

    if(!expect_peek(p, IDENT)) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, "expected identifier after fn statement\n");
        arrput(p->errors, msg);
        return NULL;
    }

    stmt->function_stmt.name = make_identifier(p->cur_token, p->cur_token.literal);

    if(!expect_peek(p, LPAREN)) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, "( expected\n");
        arrput(p->errors, msg);
        return NULL;
    }

    stmt->function_stmt.parameters = parse_function_parameters(p);

    if(!expect_peek(p, LBRACE)) {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, "{ expected\n");
        arrput(p->errors, msg);
        return NULL;
    }

    stmt->function_stmt.body = parse_block_statement(p);

    int len = arrlen(stmt->function_stmt.body);

    int return_len = 0;

    program return_stmts = NULL;
    for(int i = 0; i < len; i++) {
        count_return(stmt->function_stmt.body[i], &return_stmts);
    }

    //TODO: I think we always need a return stmt
    len = arrlen(return_stmts);

    if(len) {

        return_len = arrlen(return_stmts[0]->return_stmt.return_values);

        for(int i = 1; i < len; i++) {
            int tmp = arrlen(return_stmts[i]->return_stmt.return_values);

            if(tmp != return_len) {
                sds msg = NEW_ERROR_PREFIX;
                msg = sdscatprintf(msg, "a function has always to return the same number of values\n");
                arrput(p->errors, msg);
                return NULL;

            }
        }
    }

    stmt->function_stmt.num_return_values = return_len;

    return stmt;

}

ast ** parse_expression_list(parser *p, bool with_paren) {

    ast **list = NULL;

    if (with_paren) {
        if (peek_token_is(p, RPAREN)) {
            advance_token(p);
            return list;
        }

        advance_token(p);
    }

    arrpush(list, parse_expression(p, LOWEST));

    while(peek_token_is(p, COMMA)) {
        advance_token(p);
        advance_token(p);
        arrpush(list, parse_expression(p, LOWEST));
    }

    if(with_paren) {
        if (!expect_peek(p, RPAREN)) {
            sds msg = NEW_ERROR_PREFIX;
            msg = sdscatprintf(msg, ") expected\n");
            arrput(p->errors, msg);
            return NULL;
        }
    }

    return list;
}

ast *parse_call_expression(parser *p, ast *function) {

    ast *exp = make_call_expression(p->cur_token, function);
    exp->call_expr.arguments = parse_expression_list(p, true);
    return exp;
}

ast *parse_expression(parser *p, enum operator_precedence precedence) {

    ast *left_expr;

    //prefix expression
    if(TOKEN_TYPE_EQUALS(p->cur_token, IDENT)) {
        left_expr = parse_identifier(p);
    } else if(TOKEN_TYPE_EQUALS(p->cur_token, NUMBER)) {
        left_expr =  parse_number_literal(p);
    } else if(TOKEN_TYPE_EQUALS(p->cur_token, STRING)) {
        left_expr =  parse_string_literal(p);
    } else if(TOKEN_TYPE_EQUALS(p->cur_token, TRUE) || TOKEN_TYPE_EQUALS(p->cur_token, FALSE)) {
        left_expr = parse_boolean_literal(p);
    } else if(TOKEN_TYPE_EQUALS(p->cur_token, BANG) || TOKEN_TYPE_EQUALS(p->cur_token, MINUS)) {
        left_expr = parse_prefix_expression(p);
    } else if(TOKEN_TYPE_EQUALS(p->cur_token, LPAREN)) {
        left_expr = parse_grouped_expression(p);
    } else if(TOKEN_TYPE_EQUALS(p->cur_token, IF)) {
        left_expr = parse_if_expression(p);
    } else {
        sds msg = NEW_ERROR_PREFIX;
        msg = sdscatprintf(msg, "no prefix parse function for the token \"%s\"\n", p->cur_token.literal);
        arrput(p->errors, msg);
        return NULL;
    }

    //infix expression
    while(!peek_token_is(p, SEMICOLON) && precedence < peek_precedence(p)) {

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

        if (!is_infix) {
            return left_expr;
        }

        advance_token(p);

        if(TOKEN_TYPE_EQUALS(p->cur_token, LPAREN)) {
            left_expr = parse_call_expression(p, left_expr);
        }
        else {
            left_expr = parse_infix_expression(p, left_expr);
        }

    }

    return left_expr;

}

ast * parse_expression_statement(parser *p) {
    ast *stmt = make_expression_stmt(p->cur_token);
    stmt->expr_stmt = parse_expression(p, LOWEST);
    if (peek_token_is(p, SEMICOLON)) {
        advance_token(p);
    }
    return stmt;
}

ast * parse_statement(parser *p) {

    if(cur_token_is(p, IDENT) && peek_token_is(p, ASSIGN) ) {
        return parse_assignment_statement(p, ast_assignment_stmt, true);
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

    if(cur_token_is(p, FUNCTION)) {
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

program parse_program(parser *p) {

    program  program = NULL;

    while(!TOKEN_TYPE_EQUALS(p->cur_token, ENDOF)) {
          ast *stmt = parse_statement(p);
          if (stmt != NULL) {
              arrput(program, stmt);
          }
          advance_token(p);
    }
    return program;

}

void peek_error(parser *p, token_type t) {
    sds msg = NEW_ERROR_PREFIX;
    msg = sdscatprintf(msg, "expected next token to be %d, got %d instead\n",  t, p->peek_token.type);
    arrput(p->errors, msg);
}

static enum operator_precedence get_precedence (token t) {

    if(TOKEN_TYPE_EQUALS(t, EQ) || TOKEN_TYPE_EQUALS(t, NOT_EQ)) {
        return EQUALS;
    }

    if(TOKEN_TYPE_EQUALS(t, AND)) {
        return ANDP;
    }

    if(TOKEN_TYPE_EQUALS(t, OR)) {
        return ORP;
    }

    if(TOKEN_TYPE_EQUALS(t, LT) || TOKEN_TYPE_EQUALS(t, GT) || TOKEN_TYPE_EQUALS(t, LEQ) || TOKEN_TYPE_EQUALS(t, GEQ)) {
        return LESSGREATER;
    }

    if(TOKEN_TYPE_EQUALS(t, PLUS) || TOKEN_TYPE_EQUALS(t, MINUS))  {
        return SUM;
    }

    if(TOKEN_TYPE_EQUALS(t, SLASH) || TOKEN_TYPE_EQUALS(t, ASTERISK)) {
        return PRODUCT;
    }

    if(TOKEN_TYPE_EQUALS(t, LPAREN)) {
        return CALL;
    }

    return LOWEST;

}

enum operator_precedence peek_precedence(parser *p) {
    return get_precedence(p->peek_token);
}

enum operator_precedence cur_precedence(parser *p) {
    return get_precedence(p->cur_token);
}
