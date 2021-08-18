#include "lexer.h"
#include "token.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

lexer *new_lexer(const char *input, const char *file_name) {
	lexer *l = (lexer*) malloc(sizeof(lexer));

    if(l == NULL) {
        fprintf(stderr, "%s - Error allocating memory for the lexer!\n", __FUNCTION__);
        return NULL;
    }

	l->input = input;
	l->read_position = 0;
    l->current_line = 1;
    l->file_name = NULL;

    l->file_name = file_name;

    read_char(l);

    return l;
}

void free_lexer(lexer *l) {
    free(l);
}

void read_char(lexer *l) {

	if(l->read_position > strlen(l->input)) {
		l->ch = 0;
	}
	else {
		l->ch = l->input[l->read_position];
	}

	l->position = l->read_position++;
}

char peek_char(lexer *l) {

	if(l->read_position > strlen(l->input)) {
		return '\0';
	}
	else {
		return l->input[l->read_position];
	}
}

char *read_identifier(lexer *l, uint32_t *len) {
	int position = l->position;
	while(isalpha(l->ch) || isdigit(l->ch) || l->ch == '_' || l->ch == '\'') {
		read_char(l);
	}

    *len = l->position - position;
	return (char*) &(l->input[position]);
}

char *read_string(lexer *l, uint32_t *len) {

	int position = l->position+1;

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

	int position = l->position;

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
	while (isspace(l->ch)) {
        if(l->ch == '\n') l->current_line++;
		read_char(l);
	}
}

void skip_comment(lexer *l) {
    while (l->ch == '#') {
        while (l->ch != '\n') {
            read_char(l);
        }
        l->current_line++;
        read_char(l);
    }
}

token next_token(lexer *l) {
	token tok = {0};

	skip_whitespace(l);

    while (l->ch == '#') {
        skip_comment(l);
        skip_whitespace(l);
    }

	switch (l->ch) {
		case '=':
			if(peek_char(l) == '=') {
				read_char(l);
				tok = new_token(EQ, "==", 2, l->current_line, l->file_name);
			}
			else {
				tok = new_token(ASSIGN, "=", 1, l->current_line, l->file_name);
			}
			break;
		case '+':
			tok = new_token(PLUS, "+", 1, l->current_line, l->file_name);
			break;
		case '-':
			tok = new_token(MINUS, "-", 1, l->current_line, l->file_name);
			break;
		case '!':
			if(peek_char(l) == '=') {
                read_char(l);
				tok = new_token(NOT_EQ, "!=", 2, l->current_line, l->file_name);
			}
			else {
				tok = new_token(BANG, "!", 1, l->current_line, l->file_name);
			}
			break;
		case '/':
			tok = new_token(SLASH, "/", 1,  l->current_line, l->file_name);
			break;
		case '*':
			tok = new_token(ASTERISK, "*", 1, l->current_line, l->file_name);
			break;
		case '<':
            if(peek_char(l) == '=') {
                read_char(l);
                tok = new_token(LEQ, "<=", 2, l->current_line, l->file_name);
            }
            else {
                tok = new_token(LT, "<", 1, l->current_line, l->file_name);
            }
			break;
		case '>':
            if(peek_char(l) == '=') {
                read_char(l);
                tok = new_token(GEQ, ">=", 2, l->current_line, l->file_name);
            }else {
                tok = new_token(GT, ">", 1, l->current_line, l->file_name);
            }
			break;
		case ';':
			tok = new_token(SEMICOLON, ";", 1, l->current_line, l->file_name);
			break;
		case '(':
			tok = new_token(LPAREN, "(", 1, l->current_line, l->file_name);
			break;
		case ')':
			tok = new_token(RPAREN, ")", 1,  l->current_line, l->file_name);
            break;
		case '\"':
			tok.type = STRING;
			tok.literal = read_string(l, &tok.literal_len);
			tok.line_number = l->current_line;
			tok.file_name =  (char*)l->file_name;
			break;
		case ',':
			tok = new_token(COMMA, ",", 1, l->current_line, l->file_name);
			break;
		case '{':
			tok = new_token(LBRACE, "{", 1, l->current_line, l->file_name);
			break;
		case '}':
			tok = new_token(RBRACE, "}", 1, l->current_line, l->file_name);
			break;
        case '[':
            tok = new_token(LBRACKET, "[", 1, l->current_line, l->file_name);
            break;
        case ']':
            tok = new_token(RBRACKET, "]", 1, l->current_line, l->file_name);
            break;
		case '\0':
			tok.literal = NULL;
			tok.type = ENDOF;
			tok.line_number = l->current_line;
			tok.file_name   = (char*) l->file_name;
			break;
		default:
			if(isalpha(l->ch) || l->ch == '_') {
				tok.literal = read_identifier(l, &tok.literal_len);
				tok.type = lookup_ident(tok);
                tok.line_number = l->current_line;
				tok.file_name   = (char*) l->file_name;
				return tok;
			}
			else if(isdigit(l->ch)) {
				tok.literal = read_number(l, &tok.literal_len);
				tok.type = NUMBER;
                tok.line_number = l->current_line;
				tok.file_name   = (char*) l->file_name;

				if(tok.literal == NULL) {
					tok = new_token(ILLEGAL, NULL, 1, l->current_line, l->file_name);
				}

				return tok;
			}
			else {
				tok = new_token(ILLEGAL, "ILLEGAL", 1, l->current_line, l->file_name);
			}
	}

	read_char(l);

	return tok;
}
