#ifndef __LEXER_H
#define __LEXER_H 

#include "token.h"
#include <stddef.h>

typedef struct lexer_t {
    const char *input;
    const char *file_name;
    size_t input_len;
    size_t position;
    size_t read_position;
    size_t current_line;
    char ch;
} lexer;

void read_char(lexer *l);
lexer *new_lexer(const char *input, const char *file_name);
void free_lexer(lexer *l);
token next_token(lexer *l);

#endif /* __LEXER_H */
