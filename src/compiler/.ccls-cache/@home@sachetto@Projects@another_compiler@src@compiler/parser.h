#ifndef __PARSER_H
#define __PARSER_H 

#include "ast.h"
#include "lexer.h"
#include "token.h"

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

} parser;

parser * new_parser(lexer *l);
void advance_token(parser *p);
program parse_program(parser *p);
void peek_error(parser *p, token_type t);
ast *parse_prefix_expression(parser *p);
ast *parse_infix_expression(parser *p, ast *left);
ast *parse_expression(parser *p, enum operator_precedence precedence); 
enum operator_precedence peek_precedence(parser *p);
enum operator_precedence cur_precedence(parser *p);
ast * parse_statement(parser *p);
void print_program(program p);
void check_parser_errors(parser *p);

#endif /* __PARSER_H */
