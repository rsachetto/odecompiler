
#ifndef __TOKEN_H
#define __TOKEN_H

#define STRING_EQUALS(s1, s2) (strcmp(s1, s2) == 0)
#define TOKEN_TYPE_EQUALS(t1, t2) ((t1).type == t2)

#include "token_enum.h"

typedef struct token_t {
    token_type type;
    char *literal;
    int line_number;
    const char *file_name;
} token;

token new_token(token_type type, char *ch, int line, const char *file_name);
token_type lookup_ident(char *ident);
void print_token(token t);


#endif /* __TOKEN_H */
