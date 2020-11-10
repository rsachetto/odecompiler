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
	struct symbol *symbols;
    FILE * c_file;
};

void init_parser(struct parser *p, struct lexer *l, FILE *f);
void program(struct parser *p);

#endif /* __PARSER_H */
