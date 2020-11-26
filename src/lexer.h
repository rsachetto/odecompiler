#ifndef __LEXER_H
#define __LEXER_H 

#include "lexer_enum.h"

struct lexer {
    char current_char;
    int current_position;
    const char *source;
};

struct token {
    const char *text;
    token_type kind;
    int size;
};

#define TOKEN(t, k)  \
    (struct token) { \
        t, k, 1      \
    }
#define TOKEN_WITH_TEXT(t, k, n) \
    (struct token) {             \
        t, k, n                  \
    }


void init_lexer(struct lexer *, const char *source);
void next_char(struct lexer *);
char peek(struct lexer *);
void skip_whitespace(struct lexer *);
void skip_comment(struct lexer *);
int is_keyword(char *text);
struct token get_token(struct lexer *);


#endif /* __LEXER_H */
