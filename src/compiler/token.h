
#ifndef __TOKEN_H
#define __TOKEN_H

#define STRING_EQUALS(s1, s2) (strcmp(s1, s2) == 0)
#define STRING_EQUALS_N(s1, s2, n) (strncmp(s1, s2, n) == 0)
#define TOKEN_TYPE_EQUALS(t1, t2) ((t1).type == t2)
#define TOKEN_TYPE_NOT_EQUALS(t1, t2) ((t1).type != t2)

#include "token_enum.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct token_t {
    token_type type;
    char *literal;
    uint32_t literal_len;
    uint32_t line_number;
    const char *file_name;
} token;

token new_token(token_type type, char *ch, uint32_t len, uint32_t line, const char *file_name);
void copy_token(token *dest, const token *src);
void print_token(const token *t);


#endif /* __TOKEN_H */
