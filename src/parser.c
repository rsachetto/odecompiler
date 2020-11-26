#include "parser.h"

#include <stdio.h>
#include <stdlib.h>

#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

#include "code_generation.h"

//http://web.eecs.utk.edu/~azh/blog/teenytinycompiler2.html
//https://norasandler.com/2018/04/10/Write-a-Compiler-8.html

static int current_ident_level = 1;
#define _4SPACES "    "

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

static bool identifier_was_declared(struct parser *p, char *name) {

    token_type kind = shget(p->ode_identifiers, name);

    if (kind == -1) {
        kind = shget(p->input_identifiers, name);

        if (kind == -1) {
            kind = shget(p->let_identifiers, name);
        }
    }

    return kind != -1;
}

// Production rules.

// nl ::= '\n'+
static void nl(struct parser *p) {
    //printf("NEWLINE\n");

    // Require at least one newline.
    match(p, NEWLINE);
    // But we will allow extra newlines too, of course.
    while (check_token(p, NEWLINE)) {
        next_token(p);
    }
}

//<unary_op> ::= "!" | "~" | "-"
static void unary_op(struct parser *p) {
    //TODO: add more unary tokens
    if (check_token(p, MINUS)) {
        next_token(p);
    } else {
        fprintf(stderr, "Unexpected token at %*.s\n", p->current_token.size, p->current_token.text);
        abort();
    }
}

//primary ::= number | ident
static void primary(struct parser *p) {

    if (check_token(p, NUMBER)) {

        if (p->emit_ode_code) {
            p->ode_code = sdscatprintf(p->ode_code, "%.*s", p->current_token.size, p->current_token.text);
        } else {
            p->parsed_expression = sdscatprintf(p->parsed_expression, "%.*s", p->current_token.size, p->current_token.text);
        }

        next_token(p);

    } else if (check_token(p, IDENT)) {

        //Ensure the variable already exists.
        char *text = strndup(p->current_token.text, p->current_token.size);

        if (!identifier_was_declared(p, text)) {
            arrput(p->identifiers_to_check, text);
        }

        if (p->emit_ode_code) {
            p->ode_code = sdscatprintf(p->ode_code, "%.*s", p->current_token.size, p->current_token.text);
        } else {
            p->parsed_expression = sdscatprintf(p->parsed_expression, "%.*s", p->current_token.size, p->current_token.text);
        }

        next_token(p);

    } else {
        fprintf(stderr, "Unexpected token at %*.s\n", p->current_token.size, p->current_token.text);
        abort();
    }
}

static void expression(struct parser *p);

//<function-call> ::= id "(" [ <exp> { "," <exp> } ] ")"
static void function_call(struct parser *p) {

    if (p->emit_ode_code)
        p->ode_code = sdscatprintf(p->ode_code, "%.*s", p->current_token.size, p->current_token.text);
    else
        p->parsed_expression = sdscatprintf(p->parsed_expression, "%.*s", p->current_token.size, p->current_token.text);

    match(p, IDENT);

    if (p->emit_ode_code)
        p->ode_code = sdscatprintf(p->ode_code, "%.*s", p->current_token.size, p->current_token.text);
    else
        p->parsed_expression = sdscatprintf(p->parsed_expression, "%.*s", p->current_token.size, p->current_token.text);

    match(p, LPAREN);

    expression(p);

    while (check_token(p, COMMA)) {
        if (p->emit_ode_code)
            p->ode_code = sdscatprintf(p->ode_code, "%.*s", p->current_token.size, p->current_token.text);
        else
            p->parsed_expression = sdscatprintf(p->parsed_expression, "%.*s", p->current_token.size, p->current_token.text);

        next_token(p);
        expression(p);
    }

    if (p->emit_ode_code)
        p->ode_code = sdscatprintf(p->ode_code, "%.*s", p->current_token.size, p->current_token.text);
    else
        p->parsed_expression = sdscatprintf(p->parsed_expression, "%.*s", p->current_token.size, p->current_token.text);

    match(p, RPAREN);
}

//<factor> ::= <function-call> | "(" <exp> ")" | <unary_op> <factor> | primary
static void factor(struct parser *p) {

    if (check_token(p, IDENT) && check_peek(p, LPAREN)) {
        function_call(p);
    }

    else if (check_token(p, LPAREN)) {
        if (p->emit_ode_code) {
            p->ode_code = sdscatprintf(p->ode_code, "%.*s", p->current_token.size, p->current_token.text);
        } else {
            p->parsed_expression = sdscatprintf(p->parsed_expression, "%.*s", p->current_token.size, p->current_token.text);
        }

        next_token(p);

        expression(p);

        if (!check_token(p, RPAREN)) {
            fprintf(stderr, "Unexpected token at %*.s. Expecting )\n", p->current_token.size, p->current_token.text);
            abort();
        }
        if (p->emit_ode_code) {
            p->ode_code = sdscatprintf(p->ode_code, "%.*s", p->current_token.size, p->current_token.text);
        } else {
            p->parsed_expression = sdscatprintf(p->parsed_expression, "%.*s", p->current_token.size, p->current_token.text);
        }

        next_token(p);

    } else if (check_token(p, MINUS)) {
        //TODO: add more unary operators
        if (p->emit_ode_code) {
            p->ode_code = sdscatprintf(p->ode_code, "%.*s", p->current_token.size, p->current_token.text);
        } else {
            p->parsed_expression = sdscatprintf(p->parsed_expression, "%.*s", p->current_token.size, p->current_token.text);
        }

        next_token(p);
        factor(p);
    } else {
        primary(p);
    }
}

//<term> ::= <factor> { ("*" | "/") <factor> }
static void term(struct parser *p) {

    factor(p);

    while (check_token(p, ASTERISK) || check_token(p, SLASH)) {

        if (p->emit_ode_code) {
            p->ode_code = sdscatprintf(p->ode_code, "%.*s", p->current_token.size, p->current_token.text);
        } else {
            p->parsed_expression = sdscatprintf(p->parsed_expression, "%.*s", p->current_token.size, p->current_token.text);
        }

        next_token(p);
        factor(p);
    }
}


//<exp> ::= <term> { ("+" | "-") <term> }
static void expression(struct parser *p) {

    term(p);

    //Can have 0 or more +/- and expressions.
    while (check_token(p, PLUS) || check_token(p, MINUS)) {
        if (p->emit_ode_code) {
            p->ode_code = sdscatprintf(p->ode_code, "%.*s", p->current_token.size, p->current_token.text);
        } else {
            p->parsed_expression = sdscatprintf(p->parsed_expression, "%.*s", p->current_token.size, p->current_token.text);
        }

        next_token(p);
        term(p);
    }
}

// comparison ::= expression (("==" | "!=" | ">" | ">=" | "<" | "<=") expression)+
static void comparison(struct parser *p) {

    expression(p);
    // Must be at least one comparison operator and another expression.
    if (is_comparison_operator(p)) {
        if (p->emit_ode_code) {
            p->ode_code = sdscatprintf(p->ode_code, "%.*s", p->current_token.size, p->current_token.text);
        } else {
            p->parsed_expression = sdscatprintf(p->parsed_expression, "%.*s", p->current_token.size, p->current_token.text);
        }

        next_token(p);
        expression(p);
    } else {
        fprintf(stderr, "Expected comparison operator at: %.*s", p->current_token.size, p->current_token.text);
    }

    while (is_comparison_operator(p)) {

        if (p->emit_ode_code) {
            p->ode_code = sdscatprintf(p->ode_code, "%.*s", p->current_token.size, p->current_token.text);
        } else {
            p->parsed_expression = sdscatprintf(p->parsed_expression, "%.*s", p->current_token.size, p->current_token.text);
        }

        next_token(p);
        expression(p);
    }
}

static void declare_identifier(struct parser *p, token_type last_kind) {

    char *text;
    token_type kind;

    if (last_kind == ODE) {
        text = strndup(p->current_token.text, p->current_token.size - 1);
        kind = shget(p->ode_identifiers, text);

        if (kind == -1) {
            shput(p->ode_identifiers, text, IDENT);
        } else {
            fprintf(stderr, "Identifier %s already declared!\n", text);
            abort();
        }

    } else if (last_kind == LET) {
        text = strndup(p->current_token.text, p->current_token.size);
        kind = shget(p->let_identifiers, text);

        if (kind == -1) {
            shput(p->let_identifiers, text, IDENT);
        } else {
            fprintf(stderr, "Identifier %s already declared!\n", text);
            abort();
        }
    } else {
        text = strndup(p->current_token.text, p->current_token.size);
        kind = shget(p->input_identifiers, text);

        if (kind == -1) {
            shput(p->input_identifiers, text, IDENT);
        } else {
            fprintf(stderr, "Identifier %s already declared!\n", text);
            abort();
        }
    }
}

static void statement(struct parser *p) {
    // Check the first token to see what kind of statement this is.

    //"PRINT" (expression | string)
    if (check_token(p, PRINT)) {

        next_token(p);

        if (check_token(p, STRING)) {
            p->ode_code = sdscatprintf(p->ode_code, "print(\"%.*s\")\n", p->current_token.size, p->current_token.text);

            next_token(p);
        } else {
            p->ode_code = sdscat(p->ode_code, "print(\"%%.2f\\n\", (float)(");
            expression(p);
            p->ode_code = sdscat(p->ode_code, "))\n");
        }
    }
    // "IF" comparison "THEN" {statement} { "ELSE" {statement} } "ENDIF"
    else if (check_token(p, IF)) {

        next_token(p);

        for (int i = 0; i < current_ident_level; i++) p->ode_code = sdscat(p->ode_code, _4SPACES);
        p->ode_code = sdscat(p->ode_code, "if ");
        current_ident_level++;

        comparison(p);

        match(p, THEN);
        nl(p);

        p->ode_code = sdscat(p->ode_code, ":\n");

        // Zero or more statements in the body.
        while (!check_token(p, ENDIF)) {
            statement(p);
        }

        match(p, ENDIF);
        p->ode_code = sdscat(p->ode_code, "\n");
        current_ident_level--;

    }
    //"ELIF" comparison "THEN"
    //TODO: match this with the last opened if. Maybe use a stack here.
    else if (check_token(p, ELIF)) {

        next_token(p);

        current_ident_level--;

        for (int i = 0; i < current_ident_level; i++) {
            p->ode_code = sdscat(p->ode_code, _4SPACES);
        }

        p->ode_code = sdscat(p->ode_code, "elif ");

        comparison(p);

        match(p, THEN);

        p->ode_code = sdscat(p->ode_code, ":\n");

        current_ident_level++;

    } else if (check_token(p, ELSE)) {

        next_token(p);

        current_ident_level--;
        for (int i = 0; i < current_ident_level; i++) p->ode_code = sdscat(p->ode_code, _4SPACES);
        p->ode_code = sdscat(p->ode_code, "else:\n");
        current_ident_level++;
    }
    // "WHILE" comparison "REPEAT" {statement} "ENDWHILE"
    else if (check_token(p, WHILE)) {

        next_token(p);

        for (int i = 0; i < current_ident_level; i++) p->ode_code = sdscat(p->ode_code, _4SPACES);
        p->ode_code = sdscat(p->ode_code, "while ");

        current_ident_level++;

        comparison(p);

        match(p, REPEAT);

        nl(p);
        p->ode_code = sdscat(p->ode_code, ":\n");

        // Zero or more statements in the body.
        while (!check_token(p, ENDWHILE)) {
            statement(p);
        }

        match(p, ENDWHILE);
        p->ode_code = sdscat(p->ode_code, "\n");
        current_ident_level--;
    }

    //FN indentifier "(" {"."} ")" BEGINFN {statement} ENDFN
    else if (check_token(p, FN)) {

        next_token(p);

        match(p, IDENT);
        match(p, LPAREN);

        //TODO: add this identifier to the function scope
        match(p, IDENT);

        while (check_token(p, COMMA)) {
            next_token(p);
            //TODO: add this identifier to the function scope
            match(p, IDENT);
        }

        match(p, RPAREN);

        match(p, BEGINFN);

        nl(p);

        // Zero or more statements in the body.
        while (!check_token(p, ENDFN)) {

            if (check_token(p, FN)) {
                fprintf(stderr, "Function declaration inside functions is not allowed!\n");
                abort();
            }

            statement(p);
        }

        match(p, ENDFN);

    }

    else if (check_token(p, RETURN)) {
        next_token(p);
        expression(p);
    }

    //"LET" ident "=" expression
    //"INPUT" ident "=" expression
    //"ODE" ident "=" expression
    //ident "=" expression
    else if (check_token(p, IDENT) || check_token(p, ODE) || check_token(p, LET) || check_token(p, INPUT)) {

        if (p->current_token.kind != IDENT) {
            token_type last_kind = p->current_token.kind;

            next_token(p);

            declare_identifier(p, last_kind);

            if (last_kind == ODE) {
                static int ode_counter = 0;
                p->ode_code = sdscatprintf(p->ode_code, "    rDY[%d] = ", ode_counter);
                ode_counter++;
            } else {
                for (int i = 0; i < current_ident_level; i++) p->ode_code = sdscat(p->ode_code, _4SPACES);
                p->ode_code = sdscatprintf(p->ode_code, "%.*s = ", p->current_token.size, p->current_token.text);
            }

            match(p, IDENT);

        } else {
            char *text = strndup(p->current_token.text, p->current_token.size);

            if (!identifier_was_declared(p, text)) {
                fprintf(stderr, "Referencing variable before assignment: %s\n", text);
                abort();
            }

            for (int i = 0; i < current_ident_level; i++) p->ode_code = sdscat(p->ode_code, _4SPACES);
            p->ode_code = sdscatprintf(p->ode_code, "%.*s = ", p->current_token.size, p->current_token.text);
            free(text);

            match(p, IDENT);
        }

        match(p, EQ);
        expression(p);
        p->ode_code = sdscat(p->ode_code, "\n");
    }
    //"LET" ident "=" expression_without_identifiers
    else if (check_token(p, INITIAL)) {

        next_token(p);

        char *var = strndup(p->current_token.text, p->current_token.size);

        match(p, IDENT);

        match(p, EQ);

        p->allow_identifiers = false;
        p->emit_ode_code = false;

        expression(p);

        shput(p->ode_intial_values, var, strdup(p->parsed_expression));

        sdsfree(p->parsed_expression);
        p->parsed_expression = sdsempty();

        free(var);

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

// program ::= {statement}
void program(struct parser *p) {

    while (check_token(p, NEWLINE))
        next_token(p);

    // Parse all the statements in the program.
    while (!check_token(p, TEOF)) {
        statement(p);
    }

    int check_len = arrlen(p->identifiers_to_check);

    for (int i = 0; i < check_len; i++) {
        if (!identifier_was_declared(p, p->identifiers_to_check[i])) {
            fprintf(stderr, "Referencing variable %s before assignment. This is only allowed for ODE variables.\n", p->identifiers_to_check[i]);
            abort();
        }
    }

    generate_main_python(p);
}

void init_parser(struct parser *p, struct lexer *l, FILE *f) {

    p->lexer = l;
    p->c_file = f;

    p->let_identifiers = NULL;
    sh_new_arena(p->let_identifiers);
    shdefault(p->let_identifiers, -1);

    //variable time is always defined
    shput(p->let_identifiers, "time", IDENT);

    p->input_identifiers = NULL;
    sh_new_arena(p->input_identifiers);
    shdefault(p->input_identifiers, -1);

    p->ode_identifiers = NULL;
    sh_new_arena(p->ode_identifiers);
    shdefault(p->ode_identifiers, -1);

    p->ode_intial_values = NULL;
    sh_new_arena(p->ode_intial_values);
    shdefault(p->ode_intial_values, 0);

    p->identifiers_to_check = NULL;

    p->ode_code = sdsempty();

    p->emit_ode_code = true;
    p->allow_identifiers = true;
    p->parsed_expression = sdsempty();

    next_token(p);
    next_token(p);//Call this twice to initialize current and peek;
}
