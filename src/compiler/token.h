#ifndef __TOKEN_H
#define __TOKEN_H

//typedef char * token_type;

#define STRING_EQUALS(s1, s2) (strcmp(s1, s2) == 0)
#define TOKEN_TYPE_EQUALS(t1, t2) ((t1).type == t2)

typedef enum token_type_t {
	ILLEGAL    , //"ILLEGAL"
	ENDOF      , //"EOF"

	IDENT      , //"IDENT" // add, foobar, x, y, ...
	NUMBER     , //"NUMBER"
	STRING     , //"STRING"

	ASSIGN     , //"="
	PLUS       , //"+"
	MINUS      , //"-"
	BANG       , //"!"
	ASTERISK   , //"*"
	SLASH      , //"/"
	LT         , //"<"
	GT         , //">"
    LEQ        , //"<="
    GEQ        , //">="
	EQ         , //"=="
	NOT_EQ     , //"!="
	AND        , //"and"
	OR         , //"or"

	COMMA      , // ","
	SEMICOLON  , // ";"
	LPAREN     , // "("
	RPAREN     , // ")"
	LBRACE     , // "{"
	RBRACE     , // "}"
    LBRACKET   , // "["
    RBRACKET   , // "]"


	FUNCTION   , //"FUNCTION"
	ODE        , //"ODE"
	TRUE       , //"TRUE"
	FALSE      , //"FALSE"
	IF         , //"IF"
	ELSE       , //"ELSE"
	RETURN_STMT, //"RETURN"
	WHILE      , //"WHILE"
	INITIAL    , //"INITIAL"
	GLOBAL     , //"GLOBAL"
	IMPORT     , //"IMPORT"
} token_type;

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
