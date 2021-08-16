#include "enum_to_string.h"

BEGIN_ENUM(token_type) {
    DECL_ENUM_ELEMENT(ILLEGAL)//"ILLEGAL"
    DECL_ENUM_ELEMENT_STR(ENDOF, "end of file")  //"EOF"

    DECL_ENUM_ELEMENT(IDENT) //"IDENT" // add) foobar) x) y) ...
    DECL_ENUM_ELEMENT(NUMBER)//"NUMBER"
    DECL_ENUM_ELEMENT(STRING)//"STRING"

    DECL_ENUM_ELEMENT_STR(ASSIGN,    "=")
    DECL_ENUM_ELEMENT_STR(PLUS,      "+")
    DECL_ENUM_ELEMENT_STR(MINUS,     "-")
    DECL_ENUM_ELEMENT_STR(BANG,      "!")
    DECL_ENUM_ELEMENT_STR(ASTERISK,  "*")
    DECL_ENUM_ELEMENT_STR(SLASH,     "/")
    DECL_ENUM_ELEMENT_STR(LT,        "<")
    DECL_ENUM_ELEMENT_STR(GT,        ">")
    DECL_ENUM_ELEMENT_STR(LEQ,      "<=")
    DECL_ENUM_ELEMENT_STR(GEQ,      ">=")
    DECL_ENUM_ELEMENT_STR(EQ,       "==")
    DECL_ENUM_ELEMENT_STR(NOT_EQ,   "!=")
    DECL_ENUM_ELEMENT_STR(AND,     "and")
    DECL_ENUM_ELEMENT_STR(OR,       "or")

    DECL_ENUM_ELEMENT_STR(COMMA,     ",")
    DECL_ENUM_ELEMENT_STR(SEMICOLON, ";")
    DECL_ENUM_ELEMENT_STR(LPAREN,    "(")
    DECL_ENUM_ELEMENT_STR(RPAREN,    ")")
    DECL_ENUM_ELEMENT_STR(LBRACE,    "{")
    DECL_ENUM_ELEMENT_STR(RBRACE,    "}")
    DECL_ENUM_ELEMENT_STR(LBRACKET,  "[")
    DECL_ENUM_ELEMENT_STR(RBRACKET,  "]")


    DECL_ENUM_ELEMENT_STR(FUNCTION,           "fn")
    DECL_ENUM_ELEMENT_STR(ODE,                "ode")
    DECL_ENUM_ELEMENT_STR(TRUE,              "true")
    DECL_ENUM_ELEMENT_STR(FALSE,            "false")
    DECL_ENUM_ELEMENT_STR(IF,                  "if")
    DECL_ENUM_ELEMENT_STR(ELSE,              "else")
    DECL_ENUM_ELEMENT_STR(ELIF,              "elif")
    DECL_ENUM_ELEMENT_STR(RETURN_STMT,     "return")
    DECL_ENUM_ELEMENT_STR(WHILE,            "while")
    DECL_ENUM_ELEMENT_STR(INITIAL,        "initial")
    DECL_ENUM_ELEMENT_STR(GLOBAL,          "global")
    DECL_ENUM_ELEMENT_STR(IMPORT,          "import")
    DECL_ENUM_ELEMENT_STR(EXPR,              "expr")
    DECL_ENUM_ELEMENT_STR(ASSIGNMENT, "assignement")
}

END_ENUM(token_type)
