#include "enum_to_string.h"

BEGIN_ENUM(token_type) {
    DECL_ENUM_ELEMENT(ILLEGAL    ) //"ILLEGAL"
    DECL_ENUM_ELEMENT(ENDOF      ) //"EOF"

    DECL_ENUM_ELEMENT(IDENT      ) //"IDENT" // add) foobar) x) y) ...
    DECL_ENUM_ELEMENT(NUMBER     ) //"NUMBER"
    DECL_ENUM_ELEMENT(STRING     ) //"STRING"

    DECL_ENUM_ELEMENT(ASSIGN     ) //"="
    DECL_ENUM_ELEMENT(PLUS       ) //"+"
    DECL_ENUM_ELEMENT(MINUS      ) //"-"
    DECL_ENUM_ELEMENT(BANG       ) //"!"
    DECL_ENUM_ELEMENT(ASTERISK   ) //"*"
    DECL_ENUM_ELEMENT(SLASH      ) //"/"
    DECL_ENUM_ELEMENT(LT         ) //"<"
    DECL_ENUM_ELEMENT(GT         ) //">"
    DECL_ENUM_ELEMENT(LEQ        ) //"<="
    DECL_ENUM_ELEMENT(GEQ        ) //">="
    DECL_ENUM_ELEMENT(EQ         ) //"=="
    DECL_ENUM_ELEMENT(NOT_EQ     ) //"!="
    DECL_ENUM_ELEMENT(AND        ) //"and"
    DECL_ENUM_ELEMENT(OR         ) //"or"

    DECL_ENUM_ELEMENT(COMMA      ) // ")"
    DECL_ENUM_ELEMENT(SEMICOLON  ) // ";"
    DECL_ENUM_ELEMENT(LPAREN     ) // "("
    DECL_ENUM_ELEMENT(RPAREN     ) // ")"
    DECL_ENUM_ELEMENT(LBRACE     ) // "{"
    DECL_ENUM_ELEMENT(RBRACE     ) // "}"
    DECL_ENUM_ELEMENT(LBRACKET   ) // "["
    DECL_ENUM_ELEMENT(RBRACKET   ) // "]"


    DECL_ENUM_ELEMENT(FUNCTION   ) //"FUNCTION"
    DECL_ENUM_ELEMENT(ODE        ) //"ODE"
    DECL_ENUM_ELEMENT(TRUE       ) //"TRUE"
    DECL_ENUM_ELEMENT(FALSE      ) //"FALSE"
    DECL_ENUM_ELEMENT(IF         ) //"IF"
    DECL_ENUM_ELEMENT(ELSE       ) //"ELSE"
    DECL_ENUM_ELEMENT(ELIF       ) //"ELIF"
    DECL_ENUM_ELEMENT(RETURN_STMT) //"RETURN"
    DECL_ENUM_ELEMENT(WHILE      ) //"WHILE"
    DECL_ENUM_ELEMENT(INITIAL    ) //"INITIAL"
    DECL_ENUM_ELEMENT(GLOBAL     ) //"GLOBAL"
    DECL_ENUM_ELEMENT(IMPORT     ) //"IMPORT"
}

END_ENUM(token_type)
