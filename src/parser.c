#include "parser.h"

#include <stdio.h>
#include <stdlib.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

//http://web.eecs.utk.edu/~azh/blog/teenytinycompiler2.html
//https://norasandler.com/2018/04/10/Write-a-Compiler-8.html

static void next_token(struct parser *p) {
    p->current_token = p->peek_token;
    p->peek_token = get_token(p->lexer);
}

static bool check_token(struct parser *p, token_type kind) {
    return kind == p->current_token.kind;
}

static bool check_peek(struct parser *p, token_type kind) {
    return kind == p->peek_token.kind;
}

static void match(struct parser *p, token_type kind) {
    if (!check_token(p, kind)) {
        fprintf(stderr, "Expected %s, got %s\n", get_stringtoken_type(kind), get_stringtoken_type(p->current_token.kind));
       abort();
    }
    next_token(p);
}

static bool is_comparison_operator(struct parser *p) {
    return check_token(p, GT) || check_token(p, GTEQ) || check_token(p, LT) || check_token(p, LTEQ) ||
           check_token(p, EQEQ) || check_token(p, NOTEQ);
}
// Production rules.

// nl ::= '\n'+
static void nl(struct parser *p) {
    printf("NEWLINE\n");

    // Require at least one newline.
    match(p, NEWLINE);
    // But we will allow extra newlines too, of course.
    while (check_token(p, NEWLINE)) {
        next_token(p);
    }
}

//<unary_op> ::= "!" | "~" | "-"
static void unary_op(struct  parser *p) {
    //TODO: add more unary tokens
    if (check_token(p, MINUS)) {
        next_token(p);
    }
    else {
        fprintf(stderr, "Unexpected token at %*.s\n", p->current_token.size, p->current_token.text);
       abort();
    }

}

//primary ::= number | ident
static void primary(struct parser *p) {
    printf("PRIMARY (%.*s)\n", p->current_token.size, p->current_token.text);

    if (check_token(p, NUMBER)) {
        fprintf(p->c_file, "%.*s", p->current_token.size, p->current_token.text);
        next_token(p);
    } else if (check_token(p, IDENT)) {
        //Ensure the variable already exists.
        char *text = strndup(p->current_token.text, p->current_token.size);

        if (shget(p->symbols, text) != IDENT) {
            fprintf(stderr, "Referencing variable before assignment: %s\n", text);
           //abort();
        }

        fprintf(p->c_file, "%.*s", p->current_token.size, p->current_token.text);
        next_token(p);
    } else {
        fprintf(stderr, "Unexpected token at %*.s\n", p->current_token.size, p->current_token.text);
        abort();
    }
}

static void expression(struct parser *p);

//<function-call> ::= id "(" [ <exp> { "," <exp> } ] ")"
static void function_call(struct parser *p) {

    printf("FUNCTION_CALL\n");

    fprintf(p->c_file, "%.*s", p->current_token.size, p->current_token.text);
    match(p, IDENT);

    fprintf(p->c_file, "%.*s", p->current_token.size, p->current_token.text);
    match(p, LPAREN);

    expression(p);

    while(check_token(p, COMMA)) {
        fprintf(p->c_file, "%.*s", p->current_token.size, p->current_token.text);
        next_token(p);
        expression(p);
    }

    fprintf(p->c_file, "%.*s", p->current_token.size, p->current_token.text);
    match(p, RPAREN);
}

//<factor> ::= <function-call> | "(" <exp> ")" | <unary_op> <factor> | primary
static void factor(struct parser *p) {

    if(check_token(p, IDENT) && check_peek(p, LPAREN)) {
        function_call(p);
    }

    if(check_token(p, LPAREN)) {
        fprintf(p->c_file, "%.*s", p->current_token.size, p->current_token.text);

        next_token(p);

        expression(p);

        if(!check_token(p, RPAREN)) {
            fprintf(stderr, "Unexpected token at %*.s. Expecting )\n", p->current_token.size, p->current_token.text);
           abort();
        }

        fprintf(p->c_file, "%.*s", p->current_token.size, p->current_token.text);

        next_token(p);

    }
    //TODO: add more unary operators
    else if(check_token(p, MINUS)) {
        fprintf(p->c_file, "%.*s", p->current_token.size, p->current_token.text);
        next_token(p);
        factor(p);
    }
    else {
        primary(p);
    }


}

//<term> ::= <factor> { ("*" | "/") <factor> }
static void term(struct parser *p) {
    printf("TERM\n");

    factor(p);
    //Can have 0 or more *// and expressions.
    while (check_token(p, ASTERISK) || check_token(p, SLASH)) {
        fprintf(p->c_file, "%.*s", p->current_token.size, p->current_token.text);
        next_token(p);
        factor(p);
    }
}


//<exp> ::= <term> { ("+" | "-") <term> }
static void expression(struct parser *p) {

    printf("EXPRESSION\n");

    term(p);

    //Can have 0 or more +/- and expressions.
    while (check_token(p, PLUS) || check_token(p, MINUS)) {
        fprintf(p->c_file, "%.*s", p->current_token.size, p->current_token.text);
        next_token(p);
        term(p);
    }
}

// comparison ::= expression (("==" | "!=" | ">" | ">=" | "<" | "<=") expression)+
static void comparison(struct parser *p) {
    printf("COMPARISSON\n");

    expression(p);
    // Must be at least one comparison operator and another expression.
    if (is_comparison_operator(p)) {
        fprintf(p->c_file, "%.*s", p->current_token.size, p->current_token.text);
        next_token(p);
        expression(p);
    } else {
        fprintf(stderr, "Expected comparison operator at: %.*s", p->current_token.size, p->current_token.text);
    }

    while (is_comparison_operator(p)) {
        fprintf(p->c_file, "%.*s", p->current_token.size, p->current_token.text);
        next_token(p);
        expression(p);
    }
}

static void statement(struct parser *p) {
    // Check the first token to see what kind of statement this is.

    //"PRINT" (expression | string)
    if (check_token(p, PRINT)) {
        printf("STATEMENT-PRINT\n");
        next_token(p);

        if (check_token(p, STRING)) {
            fprintf(p->c_file, "printf(\"%.*s\");\n", p->current_token.size, p->current_token.text);
            next_token(p);
        } else {
            fprintf(p->c_file, "printf(\"%%.2f\\n\", (float)(");
            expression(p);
            fprintf(p->c_file, "));\n");
        }
    }

    // "IF" comparison "THEN" {statement} { "ELSE" {statement} } "ENDIF"
    else if (check_token(p, IF)) {
        printf("STATEMENT-IF\n");

        next_token(p);

        fprintf(p->c_file, "if(");

        comparison(p);

        match(p, THEN);
        nl(p);

        fprintf(p->c_file, "){\n");

        // Zero or more statements in the body.
        while (!check_token(p, ENDIF)) {
            statement(p);
        }

        match(p, ENDIF);
        fprintf(p->c_file, "}\n");

    }
    else if (check_token(p, ELIF)) {
        printf("STATEMENT-ELIF\n");

        next_token(p);

        fprintf(p->c_file, "} else if(");

        comparison(p);

        match(p, THEN);
        fprintf(p->c_file, "){\n");
    }
    else if (check_token(p, ELSE)) {
        printf("STATEMENT-ELSE\n");
        next_token(p);
        fprintf(p->c_file, " } else {\n");
    }

    // "WHILE" comparison "REPEAT" {statement} "ENDWHILE"
    else if (check_token(p, WHILE)) {
        printf("STATEMENT-WHILE\n");

        next_token(p);
        fprintf(p->c_file, "while(");

        comparison(p);

        match(p, REPEAT);

        nl(p);
        fprintf(p->c_file, "){\n");

        // Zero or more statements in the body.
        while (!check_token(p, ENDWHILE)) {
            statement(p);
        }

        match(p, ENDWHILE);
        fprintf(p->c_file, "}\n");
    }

    //"LET" ident "=" expression
    //"INPUT" ident "=" expression
    //"ODE" ident "=" expression
    //ident "=" expression
    //TODO: simplify
    else if (check_token(p, ODE) || check_token(p, IDENT) || check_token(p, LET) || check_token(p, INPUT)) {

        printf("STATEMENT-LET OR INPUT\n");

        char *text;
        token_type kind;

        if (p->current_token.kind == LET || p->current_token.kind == INPUT || p->current_token.kind == ODE) {
            next_token(p);

            text = strndup(p->current_token.text, p->current_token.size);
            kind = shget(p->symbols, text);

            if (kind == -1) {
                shput(p->symbols, text, IDENT);
            }

            fprintf(p->c_file, "float %.*s = ", p->current_token.size, p->current_token.text);

            match(p, IDENT);
        } else {
            text = strndup(p->current_token.text, p->current_token.size);
            kind = shget(p->symbols, text);

            if (kind == -1) {
                fprintf(stderr, "Referencing variable before assignment: %s\n", text);
                //abort();
            }

            fprintf(p->c_file, "%.*s = ", p->current_token.size, p->current_token.text);

            match(p, IDENT);

            next_token(p);
        }

        free(text);

        match(p, EQ);
        expression(p);
        fprintf(p->c_file, ";\n");
    }
    // This is not a valid statement. Error!
    else {
        fprintf(stderr, "Invalid statement at %*s (%s)\n", p->current_token.size, p->current_token.text,
                get_stringtoken_type(p->current_token.kind));
       abort();
    }
    // Newline.
    nl(p);
}

static void generate_main_python(FILE *c_file, int num_odes) {
    fprintf(c_file, "if __name__ == \"__main__\":\n");

    fprintf(c_file, "    x0 = []\n");
    fprintf(c_file, "    ts = range(int(argv[1]))\n");

    fprintf(c_file, "    set_initial_conditions(x0)\n");

    fprintf(c_file, "    result = odeint(ode, x0, ts)\n");

    fprintf(c_file, "    out = open(\"out.txt\", \"w\")\n");

    fprintf(c_file, "    for i in range(len(ts)):\n");
    fprintf(c_file, "        out.write(str(ts[i]) + \", \")\n");
    fprintf(c_file, "        for j in range(%d):\n", num_odes);
    fprintf(c_file, "            if j < 1:\n");
    fprintf(c_file, "                out.write(str(result[i, j]) + \", \")\n");
    fprintf(c_file, "            else:\n");
    fprintf(c_file, "                out.write(str(result[i, j]))\n");

    fprintf(c_file, "        out.write(\"\\n\")\n");

    fprintf(c_file, "    out.close()\n");

    fprintf(c_file, "    try:\n");
    fprintf(c_file, "        import pylab\n");
    fprintf(c_file, "        pylab.figure(1)\n");
    fprintf(c_file, "        pylab.plot(ts, result[:,0])\n");
    fprintf(c_file, "        pylab.show()\n");
    fprintf(c_file, "    except ImportError:\n");
    fprintf(c_file, "        pass\n");
}

// program ::= {statement}
void program(struct parser *p) {

    while (check_token(p, NEWLINE))
        next_token(p);

    // Parse all the statements in the program.
    while (!check_token(p, TEOF)) {
        statement(p);
    }

    //TODO: get the number of defined odes and generate the main function
    generate_main_python(p->c_file, 10);

}

void init_parser(struct parser *p, struct lexer *l, FILE *f) {
    p->lexer = l;
    p->symbols = NULL;
    p->c_file = f;
    sh_new_arena(p->symbols);
    shdefault(p->symbols, -1);
    next_token(p);
    next_token(p);//Call this twice to initialize current and peek;
}

