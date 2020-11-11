#ifndef __PARSER_H
#define __PARSER_H 

#include "lexer.h"
#include <stdbool.h>
#include <stdio.h>

struct symbol {
	char *key;
	token_type value;
};

struct parser {
	struct lexer *lexer;
    struct token current_token;
    struct token peek_token;
	struct symbol *let_identifiers;
	struct symbol *input_identifiers;
	struct symbol *ode_identifiers;
    FILE * c_file;
	char **identifiers_to_check;
};

void init_parser(struct parser *p, struct lexer *l, FILE *f);
void program(struct parser *p);

#endif /* __PARSER_H */
