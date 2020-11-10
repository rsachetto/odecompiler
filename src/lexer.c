// http://web.eecs.utk.edu/~azh/blog/teenytinycompiler1.html

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h> 
#include <string.h>


#include "lexer.h"

void next_char(struct lexer *state) {
    state->current_position++;
    state->current_char = state->source[state->current_position];
}

char peek(struct lexer *state) {
    return state->source[state->current_position + 1];
}

void skip_whitespace(struct lexer *state) {
    while(state->current_char == ' ' || state->current_char == '\t' || state->current_char == '\r')
        next_char(state);
}

void skip_comment(struct lexer *state) {
	if (state->current_char == '#') {
		while (state->current_char != '\n') {
			next_char(state);
		}
	}
}

int is_keyword(char *text) {

	for(int i = 0; i < NUM_TOKENS - 1 ; i++) {
		if (strcmp(text, get_stringtoken_type(i)) == 0)
			return i;
	}
	return -1;
}

struct token get_token(struct lexer *state) {
    skip_whitespace(state);
	skip_comment(state);

    struct token token = {0};

	const char *value = &(state->source[state->current_position]);

    switch(state->current_char) {
        case '+':
            token = TOKEN(value, PLUS);
            break;
        case '-':
            token = TOKEN(value, MINUS);
            break;
        case '*':
            token = TOKEN(value, ASTERISK);
            break;
        case '/':
            token = TOKEN(value, SLASH);
            break;
		case '(':
			token = TOKEN(value, LPAREN);
			break;
		case ')':
			token = TOKEN(value, RPAREN);
			break;
        case ',':
            token = TOKEN(value, COMMA);
            break;
        case '\n':
            token = TOKEN(value, NEWLINE);
            break;
        case '\0':
            token = TOKEN(NULL, TEOF);
            break;
        case '=': {
            if(peek(state) == '=') {
                next_char(state);
                token = TOKEN_WITH_TEXT(value, EQEQ, 2);
            } else {
                token = TOKEN(value, EQ);
            }
        } break;
        case '>': {
            if(peek(state) == '=') {
                next_char(state);
                token = TOKEN_WITH_TEXT(value, GTEQ, 2);
            } else {
                token = TOKEN(value, GT);
            }
        }

        break;
        case '<': {
            if(peek(state) == '=') {
                next_char(state);
                token = TOKEN_WITH_TEXT(value, LTEQ, 2);
            } else {
                token = TOKEN(value, LT);
            }
        }

        break;
        case '!': {
            if(peek(state) == '=') {
                next_char(state);
                token = TOKEN_WITH_TEXT(value, NOTEQ, 2);
            }
			else {
                fprintf(stderr, "Expected !=, got !%c\n", peek(state));
				exit(0);
			 }
		break;
        }
		case '\"': {
            next_char(state);
            value = &(state->source[state->current_position]); 
			int first_position = state->current_position;

            while (state->current_char != '\"') {
                if (state->current_char == '\r' || state->current_char == '\n' || state->current_char == '\t' || state->current_char == '\\' || state->current_char == '%') {
					fprintf(stderr, "Illegal character %d in string.", state->current_char);
					exit(0);
				}
                next_char(state);
			}

            token = TOKEN_WITH_TEXT(value, STRING, state->current_position - first_position);
		}
		break;

		default: {

			if ( isdigit(state->current_char) ) {
			
				int first_position = state->current_position;
            	value = &(state->source[state->current_position]); 
				
				while (isdigit(peek(state))) {
					next_char(state);
				}

				if (peek(state) == '.') {
					
					next_char(state);

					if (!isdigit(peek(state))) {
						fprintf(stderr, "Illegal character in number.");
						exit(0);
					}
					while (isdigit(peek(state))) {
						next_char(state);
					}
				}
				
            	token = TOKEN_WITH_TEXT(value, NUMBER, state->current_position + 1 - first_position);
			}
			else if (isalpha(state->current_char)) {
				int first_position = state->current_position;

				while ( isalnum(peek(state)) || peek(state) == '\'' ) {
					next_char(state);
				}

				//Check if the token is in the list of keywords.
				char *tok_text = strndup(value, state->current_position + 1 - first_position);
				
				int keyword = is_keyword(tok_text);
				
				free(tok_text);

				if (keyword == -1) {
            		token = TOKEN_WITH_TEXT(value, IDENT, state->current_position + 1 - first_position);
				}
				else {
            		token = TOKEN_WITH_TEXT(value, keyword, state->current_position + 1 - first_position);
				}
				
			}
			else {
				fprintf(stderr, "Unknown token %c!\n", state->current_char);
			}
		}			 

        break;
    }
    next_char(state);
    return token;
}
