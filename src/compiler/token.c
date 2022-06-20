#include "token.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void copy_token(token *dest, const token *src) {

    dest->type        = src->type;
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
    t.file_name = (char*)file_name;
    t.literal = ch;
    t.literal_len = len;

    return t;
}

void print_token(const token *t) {
    printf("Type: %d - Literal: %s\n", t->type, t->literal);
}

token_type lookup_ident(const token *t) {

    char *ident = strndup(t->literal, t->literal_len);

    if(STRING_EQUALS(ident, "fn")) {
        free(ident);
        return FUNCTION;
    }
    if(STRING_EQUALS(ident, "endfn")) {
        free(ident);
        return ENDFUNCTION;
    }
    if(STRING_EQUALS(ident, "ode")) {
        free(ident);
        return ODE;
    }
    if(STRING_EQUALS(ident, "true")) {
        free(ident);
        return TRUE;
    }
    if(STRING_EQUALS(ident, "false")) {
        free(ident);
        return FALSE;
    }
    if(STRING_EQUALS(ident, "if")) {
        free(ident);
        return IF;
    }
    if(STRING_EQUALS(ident, "else")) {
        free(ident);
        return ELSE;
    }
    if(STRING_EQUALS(ident, "elif")) {
        free(ident);
        return ELIF;
    }
    if(STRING_EQUALS(ident, "return")) {
        free(ident);
        return RETURN_STMT;
    }
    if(STRING_EQUALS(ident, "while")) {
        free(ident);
        return WHILE;
    }
    if(STRING_EQUALS(ident, "initial")) {
        free(ident);
        return INITIAL;
    }
    if(STRING_EQUALS(ident, "global")) {
        free(ident);
        return GLOBAL;
    }
    if(STRING_EQUALS(ident, "and")) {
        free(ident);
        return AND;
    }
    if(STRING_EQUALS(ident, "or")) {
        free(ident);
        return OR;
    }
    if(STRING_EQUALS(ident, "import")) {
        free(ident);
        return IMPORT;
    }

    free(ident);
    return IDENT;
}
