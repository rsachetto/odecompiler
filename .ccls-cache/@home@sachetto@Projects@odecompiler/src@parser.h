#ifndef __PARSER_H
#define __PARSER_H 

#include "lexer.h"
#include <stdbool.h>
#include <stdio.h>

#include "string/sds.h"

struct symbol {
	char *key;
	token_type value;
};

struct ode_intial_value {
	char *key;
	char *value;
};

struct ast {
	char *token;
	struct ast *left;
	struct ast *right;
};


struct parser {
	struct lexer *lexer;
    struct token current_token;
    struct token peek_token;
	struct symbol *let_identifiers;
	struct symbol *input_identifiers;
	struct symbol *ode_identifiers;
	struct ode_intial_value *ode_intial_values;
    sds parsed_expression;
    bool emit_ode_code;
    bool allow_identifiers;
    FILE * c_file;
	char **identifiers_to_check;
    sds ode_code;

};

void init_parser(struct parser *p, struct lexer *l, FILE *f);
void program(struct parser *p);

#endif /* __PARSER_H */
