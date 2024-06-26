#include "lexer.h"
#include "token.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../stb/stb_ds.h"

static token_type lookup_ident(const token *t) {

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
    
    if(literal[literal_len - 1] == '\'') {
        return ODE_IDENT;
    }

    return IDENT;
}

lexer *new_lexer(const char *input, const char *file_name) {
    lexer *l = (lexer*) malloc(sizeof(lexer));

    if(l == NULL) {
        fprintf(stderr, "%s - Error allocating memory for the lexer!\n", __FUNCTION__);
        return NULL;
    }

    l->input = input;
    l->input_len = strlen(input);
    l->read_position = 0;
    l->current_line = 1;
    l->file_name = file_name;

    read_char(l);

    return l;
}

void free_lexer(lexer *l) {
    free(l);
}

void read_char(lexer *l) {

    if(l->read_position > l->input_len) {
        l->ch = 0;
    } else {
        l->ch = l->input[l->read_position];
    }

    if(!isascii(l->ch)) {
        fprintf(stderr, "Lexer error on line %d, byte %d of file %s: Invalid character found in input.\n", l->current_line, l->read_position, l->file_name);
        exit(1);
    }

    l->position = l->read_position++;
    if(l->ch == '\n') l->current_line++;

}

char peek_char(lexer *l) {

    if(l->read_position > l->input_len) {
        return '\0';
    } else {
        return l->input[l->read_position];
    }
}

static char *read_identifier(lexer *l, uint32_t *len, bool for_unit) {

    if(for_unit) {
        //skip $
        read_char(l);
    }

    uint32_t position = l->position;

    while(isalpha(l->ch) || isdigit(l->ch) || l->ch == '_' || l->ch == '\'') {
        read_char(l);
    }

    *len = l->position - position;
    return (char*) &(l->input[position]);
}

char *read_string(lexer *l, uint32_t *len) {

    uint32_t position = l->position+1;

    while(true) {
        read_char(l);
        if (l->ch == '\"' || l->ch == '\0' ) {
            break;
        }
    }

    *len = l->position - position;
    return (char*) &(l->input[position]);
}

char *read_number(lexer *l, uint32_t *len) {

    uint32_t position = l->position;

    while (isdigit(l->ch)) {
        read_char(l);
    }

    if (l->ch == '.') {
        read_char(l);
    }

    while(isdigit(l->ch)) {
        read_char(l);
    }

    if (l->ch == 'e' || l->ch == 'E') {
        read_char(l);

        if (l->ch == '-' || l->ch == '+') {
            read_char(l);
        }

        while(isdigit(l->ch)) {
            read_char(l);
        }
    }

    if(l->ch == 'd' || l->ch == 'D' || l->ch == 'F' || l->ch == 'f') {
        read_char(l);
    }

    *len = l->position - position;

    return (char*) &(l->input[position]);
}

void skip_whitespace(lexer *l) {
    while(isspace(l->ch)) {
        read_char(l);
    }
}

void skip_comment(lexer *l) {
    while (l->ch == '#') {
        while(l->ch != '\n') {
            if(l->ch == '\0') {
                fprintf(stderr, "Lexer error on line %u of file %s: Unexpected end of file while reading comment.\n", l->current_line, l->file_name);
                exit(1);
            }
            read_char(l);
        }
        skip_whitespace(l);
    }
}

token next_token(lexer *l) {
    token tok = {0};

    skip_whitespace(l);
    skip_comment(l);

    uint32_t current_line = l->current_line;
    const char *file_name = l->file_name;

    switch (l->ch) {
        case '=':
            if(peek_char(l) == '=') {
                read_char(l);
                tok = new_token(EQ, "==", 2, current_line, file_name);
            }
            else {
                tok = new_token(ASSIGN, "=", 1, current_line, file_name);
            }
            break;
        case '+':
            tok = new_token(PLUS, "+", 1, current_line, file_name);
            break;
        case '-':
            tok = new_token(MINUS, "-", 1, current_line, file_name);
            break;
        case '!':
            if(peek_char(l) == '=') {
                read_char(l);
                tok = new_token(NOT_EQ, "!=", 2, current_line, file_name);
            }
            else {
                tok = new_token(BANG, "!", 1, current_line, file_name);
            }
            break;
        case '/':
            tok = new_token(SLASH, "/", 1,  current_line, file_name);
            break;
        case '*':
            tok = new_token(ASTERISK, "*", 1, current_line, file_name);
            break;
        case '<':
            if(peek_char(l) == '=') {
                read_char(l);
                tok = new_token(LEQ, "<=", 2, current_line, file_name);
            }
            else {
                tok = new_token(LT, "<", 1, current_line, file_name);
            }
            break;
        case '>':
            if(peek_char(l) == '=') {
                read_char(l);
                tok = new_token(GEQ, ">=", 2, current_line, file_name);
            } else {
                tok = new_token(GT, ">", 1, current_line, file_name);
            }
            break;
        case ';':
            tok = new_token(SEMICOLON, ";", 1, current_line, file_name);
            break;
        case '(':
            tok = new_token(LPAREN, "(", 1, current_line, file_name);
            break;
        case ')':
            tok = new_token(RPAREN, ")", 1,  current_line, file_name);
            break;
        case '\"':
            tok.type = STRING;
            tok.literal = read_string(l, &tok.literal_len);
            tok.line_number = current_line;
            tok.file_name   =  file_name;
            break;
        case ',':
            tok = new_token(COMMA, ",", 1, current_line, file_name);
            break;
        case '{':
            tok = new_token(LBRACE, "{", 1, current_line, file_name);
            break;
        case '}':
            tok = new_token(RBRACE, "}", 1, current_line, file_name);
            break;
        case '[':
            tok = new_token(LBRACKET, "[", 1, current_line, file_name);
            break;
        case ']':
            tok = new_token(RBRACKET, "]", 1, current_line, file_name);
            break;
        case '$':
            tok.type = UNIT_DECL;
            tok.line_number = current_line;
            tok.file_name   = file_name;
            tok.literal = read_identifier(l, &tok.literal_len, true);
            break;
        case '\0':
            tok.literal = NULL;
            tok.type = ENDOF;
            tok.line_number = current_line;
            tok.file_name   = file_name;
            break;
        default:
            if(isalpha(l->ch) || l->ch == '_') {
                tok.literal = read_identifier(l, &tok.literal_len, false);
                tok.type = lookup_ident(&tok);
                tok.line_number = current_line;
                tok.file_name   = file_name;

                return tok;
            }
            else if(isdigit(l->ch)) {
                tok.literal = read_number(l, &tok.literal_len);
                tok.type = NUMBER;
                tok.line_number = current_line;
                tok.file_name   = file_name;

                if(tok.literal == NULL) {
                    tok = new_token(ILLEGAL, NULL, 1, current_line, file_name);
                }

                return tok;
            }
            else {
                tok = new_token(ILLEGAL, "ILLEGAL", 1, current_line, file_name);
            }
    }

    read_char(l);
    
    return tok;
}
