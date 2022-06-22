#include "token.h"
#include <stdio.h>
#include <stdlib.h>
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

token new_token(token_type type, char *ch, uint32_t len, int line, const char *file_name) {
    token t;
    t.type = type;
    t.line_number = line;
    t.file_name = (char *) file_name;
    t.literal = ch;
    t.literal_len = len;

    return t;
}

void print_token(const token *t) {
    printf("Type: %d - Literal: %s\n", t->type, t->literal);
}

token_type lookup_ident(const token *t) {

    char *literal = t->literal;
    uint32_t literal_len = t->literal_len;

    if(literal_len == 2) {
        if(STRING_EQUALS_N(literal, "fn", literal_len)) {
            return FUNCTION;
        }

        if(STRING_EQUALS_N(literal, "or", literal_len)) {
            return OR;
        }

        if(STRING_EQUALS_N(literal, "if", literal_len)) {
            return IF;
        }

    } else if(literal_len == 3) {

        if(STRING_EQUALS_N(literal, "ode", literal_len)) {
            return ODE;
        }

        if(STRING_EQUALS_N(literal, "and", literal_len)) {
            return AND;
        }

    } else if(literal_len == 4) {

        if(STRING_EQUALS_N(literal, "true", literal_len)) {
            return TRUE;
        }

        if(STRING_EQUALS_N(literal, "else", literal_len)) {
            return ELSE;
        }

        if(STRING_EQUALS_N(literal, "elif", literal_len)) {
            return ELIF;
        }

    } else if(literal_len == 5) {

        if(STRING_EQUALS_N(literal, "endfn", literal_len)) {
            return ENDFUNCTION;
        }
        if(STRING_EQUALS_N(literal, "false", literal_len)) {
            return FALSE;
        }
        if(STRING_EQUALS_N(literal, "while", literal_len)) {
            return WHILE;
        }

    } else if(literal_len == 6) {
        if(STRING_EQUALS_N(literal, "return", literal_len)) {
            return RETURN_STMT;
        }
        if(STRING_EQUALS_N(literal, "global", literal_len)) {
            return GLOBAL;
        }

        if(STRING_EQUALS_N(literal, "import", literal_len)) {
            return IMPORT;
        }

    } else if(literal_len == 7) {
        if(STRING_EQUALS_N(literal, "initial", literal_len)) {
            return INITIAL;
        }
    }

    return IDENT;
}
