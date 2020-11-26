// http://web.eecs.utk.edu/~azh/blog/teenytinycompiler1.html

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h> 
#include <string.h>


#include "lexer.h"

token_type last_kind = -1;

void init_lexer(struct lexer *l, const char *source) {
	l->current_char = '\0';
    l->current_position = -1;
    l->source = source;
    next_char(l);
}

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
		if (i > LEX_FIRST_KEYWORD && i < LEX_LAST_KEYWORD && strcmp(text, get_stringtoken_type(i)) == 0) {
			return i;
		}
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
			last_kind = PLUS;
            break;
        case '-':
            token = TOKEN(value, MINUS);
			last_kind = MINUS;
            break;
        case '*':
            token = TOKEN(value, ASTERISK);
			last_kind = ASTERISK;
            break;
        case '/':
            token = TOKEN(value, SLASH);
			last_kind = SLASH;
            break;
		case '(':
			token = TOKEN(value, LPAREN);
			last_kind = LPAREN;
			break;
		case ')':
			token = TOKEN(value, RPAREN);
			last_kind = RPAREN;
			break;
        case ',':
            token = TOKEN(value, COMMA);
			last_kind = COMMA;
            break;
        case '\n':
            token = TOKEN(value, NEWLINE);
			last_kind = NEWLINE;
            break;
        case '\0':
            token = TOKEN(NULL, TEOF);
			last_kind = TEOF;
            break;
        case '=': {
            if(peek(state) == '=') {
                next_char(state);
                token = TOKEN_WITH_TEXT(value, EQEQ, 2);
				last_kind = EQEQ;
            } else {
                token = TOKEN(value, EQ);
				last_kind = EQ;
            }
        } break;
        case '>': {
            if(peek(state) == '=') {
                next_char(state);
                token = TOKEN_WITH_TEXT(value, GTEQ, 2);
				last_kind = GTEQ;
            } else {
                token = TOKEN(value, GT);
				last_kind = GT;
            }
        }

        break;
        case '<': {
            if(peek(state) == '=') {
                next_char(state);
                token = TOKEN_WITH_TEXT(value, LTEQ, 2);
				last_kind = LTEQ;
            } else {
                token = TOKEN(value, LT);
				last_kind = LT;
            }
        }

        break;
        case '!': {
            if(peek(state) == '=') {
                next_char(state);
                token = TOKEN_WITH_TEXT(value, NOTEQ, 2);
				last_kind = NOTEQ;
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
			last_kind = STRING;
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
						abort();
					}

					while (isdigit(peek(state)) || peek(state) == 'e' ) {

                        if(peek(state) == 'e') {
                            next_char(state);
                            if(peek(state) != '+' && peek(state) != '-') {
                                fprintf(stderr, "Illegal character %c in number.", peek(state));
                                abort();
                            }

                            while (isdigit(peek(state))) {
                                next_char(state);
                            }
                        }
                        else {
                            next_char(state);
                        }
					}
				}
				
            	token = TOKEN_WITH_TEXT(value, NUMBER, state->current_position + 1 - first_position);
				last_kind = NUMBER;
			}
			else if (isalpha(state->current_char) || state->current_char == '_') {
				int first_position = state->current_position;

				while ( isalnum(peek(state)) || peek(state) == '_' ) {
					next_char(state);
				}

				// we only allow ' character in the end of an identifier of ODE
				if(last_kind == ODE) {
					if(peek(state) == '\'') {
						next_char(state);
                        if(isalnum(peek(state)) || state->current_char == '_' ) {
                            fprintf(stderr, "' is only allowed in the end with an ODE identifier  .\n");
                            abort();
                        }
                    }
                    else {
                        fprintf(stderr, "ODE identifiers need to end with an '.\n");
                        abort();
                    }
				}

				//Check if the token is in the list of keywords.
				char *tok_text = strndup(value, state->current_position + 1 - first_position);
				
				int keyword = is_keyword(tok_text);
				
				free(tok_text);

				if (keyword == -1) {
            		token = TOKEN_WITH_TEXT(value, IDENT, state->current_position + 1 - first_position);					
					last_kind = IDENT;
				}
				else {
            		token = TOKEN_WITH_TEXT(value, keyword, state->current_position + 1 - first_position);
					last_kind = keyword;
				}
			}	
			else {
				fprintf(stderr, "Unknown token %c!\n", state->current_char);
				abort();
			}
		}			 

        break;
    }
    next_char(state);
    return token;
}
