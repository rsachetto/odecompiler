#include "token.h"
#include <stdio.h>
#include <string.h>

token new_token(token_type type, char *ch, int line, const char *file_name, bool dup) {
    token t;
    t.type = type;
    t.line_number = line;
	t.file_name = file_name;
    t.literal = NULL;

    if(ch != NULL && dup) {
        t.literal = strdup(ch);
    }
    else {
        t.literal = ch;
    }

    return t;
}

void print_token(token t) {
    printf("Type: %d - Literal: %s\n", t.type, t.literal);
}

token_type lookup_ident(char *ident) {

	if(STRING_EQUALS(ident, "fn")) {
	   return FUNCTION;
	}
	if(STRING_EQUALS(ident, "ode")) {
	   return ODE;
	}
	if(STRING_EQUALS(ident, "true")) {
	   return TRUE;
	}
	if(STRING_EQUALS(ident, "false")) {
	   return FALSE;
	}
	if(STRING_EQUALS(ident, "if")) {
	   return IF;
	}
	if(STRING_EQUALS(ident, "else")) {
	   return ELSE;
	}
    if(STRING_EQUALS(ident, "elif")) {
	   return ELIF;
	}
	if(STRING_EQUALS(ident, "return")) {
	   return RETURN_STMT;
	}
	if(STRING_EQUALS(ident, "while")) {
	   return WHILE;
	}
	if(STRING_EQUALS(ident, "initial")) {
	   return INITIAL;
	}
    if(STRING_EQUALS(ident, "global")) {
       return GLOBAL;
	}
    if(STRING_EQUALS(ident, "and")) {
       return AND;
	}
    if(STRING_EQUALS(ident, "or")) {
        return OR;
	}
    if(STRING_EQUALS(ident, "import")) {
        return IMPORT;
	}

	return IDENT;
}
