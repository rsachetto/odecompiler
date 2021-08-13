#include "compiler/ast.h"
#include "to_latex.h"
#include "stb/stb_ds.h"
#include <stdio.h>

const char *latex_symbols[] = {
    "alpha",
    "kappa",
    "psi",
    "digamma",
    "Delta",
    "Theta",
    "beta",
    "lambda",
    "rho",
    "varepsilon",
    "Gamma",
    "Upsilon",
    "chi",
    "mu",
    "sigma",
    "varkappa",
    "Lambda",
    "Xi",
    "delta",
    "nu",
    "tau",
    "varphi",
    "Omega",
    "epsilon",
    "theta",
    "varpi",
    "Phi",
    "aleph",
    "eta",
    "omega",
    "upsilon",
    "varrho",
    "Pi",
    "beth",
    "gamma",
    "phi",
    "xi",
    "varsigma",
    "Psi",
    "daleth",
    "iota",
    "pi",
    "zeta",
    "vartheta",
    "Sigma",
    "gimel"
};

static bool is_latex_symbol(char *name) {
    //TODO: hash?
    int n = sizeof(latex_symbols)/sizeof(latex_symbols[0]);

    for(int i = 0; i < n; i++) {
        if(STRING_EQUALS(name, latex_symbols[i])) {
            return true;
        }
    }

    return false;
}

static sds expression_stmt_to_latex(ast *a) {

    if (a->expr_stmt != NULL) {
        return ast_to_latex(a->expr_stmt);
    }
    return sdsempty();
}

static sds return_stmt_to_latex(ast *a) {

    sds buf = sdsempty();

    buf = sdscatfmt(buf, "return ");

    if(a->return_stmt.return_values != NULL) {
        int n = arrlen(a->return_stmt.return_values);
        buf = sdscat(buf, ast_to_latex(a->return_stmt.return_values[0]));

        for(int i = 1; i < n; i++) {
            buf = sdscatfmt(buf, ", %s", ast_to_latex(a->return_stmt.return_values[i]));
        }
    }

    buf = sdscat(buf, ";");
    return buf;
}

static sds assignement_stmt_to_latex(ast *a) {

    sds buf = sdsempty();

    sds identifier;

    if(a->tag == ast_ode_stmt) {
        identifier = sdscatprintf(sdsempty(), "%.*s", (int)strlen(a->assignement_stmt.name->identifier.value)-1, a->assignement_stmt.name->identifier.value);
    }
    else {
        identifier = sdsnew(a->assignement_stmt.name->identifier.value);
    }

    bool is_symbol = is_latex_symbol(identifier);

    if(a->tag == ast_ode_stmt) {
        if(!is_symbol)
            buf = sdscatfmt(buf, "\\dfrac{d%s}{dt}", identifier);
        else
            buf = sdscatfmt(buf, "\\dfrac{d\\%s}{dt}", identifier);
    }
    else {
        if(!is_symbol)
            buf = sdscatfmt(buf, "%s", identifier);
        else
            buf = sdscatfmt(buf, "\\%s", identifier);
    }

    buf = sdscat(buf, " = ");

    if(a->assignement_stmt.value != NULL) {
        buf = sdscat(buf, ast_to_latex(a->assignement_stmt.value));
    }

    sdsfree(identifier);

    return buf;
}

static sds number_literal_to_latex(ast *a) {
    sds buf = sdsempty();
    buf = sdscatprintf(buf, "%e", a->num_literal.value);
    return buf;
}

static sds identifier_to_latex(ast *a) {

    bool is_symbol = is_latex_symbol(a->identifier.value);

    sds buf = sdsempty();
    if(!is_symbol)
        buf = sdscatprintf(buf, "%s", a->identifier.value);
    else
        buf = sdscatprintf(buf, "\\%s", a->identifier.value);

    return buf;
}

static sds boolean_literal_to_latex(ast *a) {
    sds buf = sdsempty();
    if(a->bool_literal.value)
        buf = sdscatprintf(buf, "true");
    else
        buf = sdscatprintf(buf, "false");
    return buf;
}

static sds string_literal_to_latex(ast *a) {
    sds buf = sdsempty();
    buf = sdscatprintf(buf, "\"%s\"", a->str_literal.value);
    return buf;
}

static sds prefix_expr_to_latex(ast *a) {

    sds buf = sdsempty();

    buf = sdscat(buf, "(");
    buf = sdscatfmt(buf, "%s", a->prefix_expr.op);
    buf = sdscatfmt(buf, "%s", ast_to_latex(a->prefix_expr.right));
    buf = sdscat(buf, ")");

    return buf;

}

static sds infix_expr_to_latex(ast *a) {

    sds buf = sdsempty();

    buf = sdscat(buf, "(");
    buf = sdscatfmt(buf, "%s", ast_to_latex(a->infix_expr.left));
    buf = sdscatfmt(buf, "%s", a->infix_expr.op);
    buf = sdscatfmt(buf, "%s", ast_to_latex(a->infix_expr.right));
    buf = sdscat(buf, ")");

    return buf;

}

static sds call_expr_to_latex(ast *a) {

    sds buf = sdsempty();

    buf = sdscat(buf, ast_to_latex(a->call_expr.function_identifier));
    buf = sdscat(buf, "(");

    int n = arrlen(a->call_expr.arguments);

    if(n) {
        buf = sdscat(buf, ast_to_latex(a->call_expr.arguments[0]));

        for(int i = 1; i < n; i++) {
            buf = sdscatfmt(buf, ", %s", ast_to_latex(a->call_expr.arguments[i]));
        }
    }

    buf = sdscat(buf, ")");

    return buf;
}

static sds grouped_assignement_stmt_to_latex(ast *a) {
    sds buf = sdsnew("[");

    int n = arrlen(a->grouped_assignement_stmt.names);

    buf = sdscat(buf, a->grouped_assignement_stmt.names[0]->identifier.value);

    for(int i = 1; i < n; i++) {
        buf = sdscatfmt(buf, ", %s", a->grouped_assignement_stmt.names[i]->identifier.value);
    }

    buf = sdscat(buf, "]");

    buf = sdscat(buf, " = ");

    buf = sdscat(buf, ast_to_latex(a->grouped_assignement_stmt.call_expr));

    return buf;

}

sds ast_to_latex(ast *a) {

    if(a->tag == ast_assignment_stmt || a->tag == ast_ode_stmt || a->tag == ast_initial_stmt || a->tag == ast_global_stmt) {
        return assignement_stmt_to_latex(a);
    }

    if(a->tag == ast_grouped_assignment_stmt) {
        return grouped_assignement_stmt_to_latex(a);
    }

    if(a->tag == ast_return_stmt) {
        return return_stmt_to_latex(a);
    }

    if(a->tag == ast_expression_stmt) {
        return expression_stmt_to_latex(a);
    }

    if(a->tag == ast_number_literal) {
        return number_literal_to_latex(a);
    }

    if(a->tag == ast_boolean_literal) {
        return boolean_literal_to_latex(a);
    }

    if(a->tag == ast_string_literal) {
        return string_literal_to_latex(a);
    }

    if(a->tag == ast_identifier) {
        return identifier_to_latex(a);
    }

    if(a->tag == ast_prefix_expression) {
        return prefix_expr_to_latex(a);
    }

    if(a->tag == ast_infix_expression) {
        return infix_expr_to_latex(a);
    }


    if(a->tag == ast_call_expression) {
        return call_expr_to_latex(a);
    }

    printf("[WARN] - to_latex not implemented to token %s\n", a->token.literal);

    return NULL;

}

sds * odes_to_latex(program p) {

    int n_stmt = arrlen(p);
    sds *return_str = NULL;

    for (int i = 0; i < n_stmt; i++) {
        if(p[i]->tag == ast_ode_stmt) {
            sds s = ast_to_latex(p[i]);

            if(s) {
                arrput(return_str, s);
            }
        }
    }

    return return_str;

}
