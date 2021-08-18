#ifndef __LEXER_H
#define __LEXER_H 

#include "token.h"

typedef struct lexer_t {
	const char *input;
    const char *file_name;
	int position;
	int read_position;
    int current_line;
	char ch;
} lexer;

void read_char(lexer *l);
lexer *new_lexer(const char *input, const char *file_name);
void free_lexer(lexer *l);
token next_token(lexer *l);

#endif /* __LEXER_H */
