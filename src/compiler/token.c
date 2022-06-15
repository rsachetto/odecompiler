#include "token.h"
#include <stdio.h>
#include <string.h>

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

    char *ident = t->literal;

	if(STRING_EQUALS_N(ident, "fn", 2)) {
	   return FUNCTION;
	}
    if(STRING_EQUALS_N(ident, "endfn", 2)) {
	    return ENDFUNCTION;
	}
	if(STRING_EQUALS_N(ident, "ode", 3)) {
	   return ODE;
	}
	if(STRING_EQUALS_N(ident, "true", 4)) {
	   return TRUE;
	}
	if(STRING_EQUALS_N(ident, "false", 5)) {
	   return FALSE;
	}
	if(STRING_EQUALS_N(ident, "if", 2)) {
	   return IF;
	}
	if(STRING_EQUALS_N(ident, "else", 4)) {
	   return ELSE;
	}
    if(STRING_EQUALS_N(ident, "elif", 4)) {
	   return ELIF;
	}
	if(STRING_EQUALS_N(ident, "return", 6)) {
	   return RETURN_STMT;
	}
	if(STRING_EQUALS_N(ident, "while", 5)) {
	   return WHILE;
	}
	if(STRING_EQUALS_N(ident, "foreachstep", 11)) {
	   return FOREACHSTEP;
	}
	if(STRING_EQUALS_N(ident, "initial", 7)) {
	   return INITIAL;
	}
    if(STRING_EQUALS_N(ident, "global", 6)) {
       return GLOBAL;
	}
    if(STRING_EQUALS_N(ident, "and", 3)) {
       return AND;
	}
    if(STRING_EQUALS_N(ident, "or", 2)) {
        return OR;
	}
    if(STRING_EQUALS_N(ident, "import", 6)) {
        return IMPORT;
	}

	return IDENT;
}
