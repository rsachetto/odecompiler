#include "token.h"
#include <string.h>

void copy_token(token *dest, const token *src) {

    dest->type = src->type;
    dest->line_number = src->line_number;

    if(src->literal) {
        dest->literal = strndup(src->literal, src->literal_len);
        dest->literal_len = src->literal_len;
    }

    if(src->file_name) {
        dest->file_name = strdup(src->file_name);
    }
}

token new_token(token_type type, char *ch, uint32_t len, uint32_t line, const char *file_name) {
    token t;
    t.type = type;
    t.line_number = line;
    t.file_name = file_name;
    t.literal = ch;
    t.literal_len = len;
    return t;
}
#ifdef DEBUG_INFO
#include <stdio.h>
void print_token(const token *t) {
    printf("Type: %d - Literal: %s\n", t->type, t->literal);
}
#endif
