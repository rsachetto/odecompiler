#ifndef __LEXER_H
#define __LEXER_H 

#include "token.h"
#include <stddef.h>

typedef struct lexer_t {
    const char *input;
    const char *file_name;
    uint32_t input_len;
    uint32_t position;
    uint32_t read_position;
    uint32_t current_line;
    char ch;
} lexer;

void read_char(lexer *l);
lexer *new_lexer(const char *input, const char *file_name);
void free_lexer(lexer *l);
token next_token(lexer *l);

#endif /* __LEXER_H */
