#include "ast.h"
#include "../stb/stb_ds.h"
#include <stdio.h>
#include <stdlib.h>

char *token_literal(ast *ast) {
	return ast->token.literal;
}

static ast *make_base_ast(token t, ast_tag tag) {
	ast *a = (ast*)malloc(sizeof(ast));
	a->tag = tag;
	a->token = t;
	return a;
}

ast *make_import_stmt(struct token_t t) {
    return make_base_ast(t, ast_import_stmt);
}

ast *make_assignement_stmt(token t, ast_tag tag) {
	return make_base_ast(t, tag);
}

ast *make_grouped_assignement_stmt(token t) {
    ast *a = make_base_ast(t, ast_grouped_assignment_stmt);
    a->grouped_assignement_stmt.names = NULL;
    a->grouped_assignement_stmt.call_expr = NULL;
    return a;
}

ast *make_while_stmt(token t) {
    ast *a = make_base_ast(t, ast_while_stmt);
    a->while_stmt.body = NULL;
    return a;
}


ast *make_identifier(token t, char *name) {
	ast *a = make_base_ast(t, ast_identifier);
	a->identifier.value = strdup(name);
	return a;

}

ast *make_string_literal(token t, char *name) {
	ast *a = make_base_ast(t, ast_string_literal);
	a->str_literal.value = strdup(name);
	return a;
}

ast *make_return_stmt(token t) {
	ast *a = make_base_ast(t, ast_return_stmt);
	return a;
}

ast *make_expression_stmt(token t) {
	ast *a = make_base_ast(t, ast_expression_stmt);
	return a;
}

ast *make_number_literal(token t) {
	ast *a = make_base_ast(t, ast_number_literal);
	return a;
}

ast *make_boolean_literal(token t, bool value) {
    ast *a = make_base_ast(t, ast_boolean_literal);
    a->bool_literal.value = value;
    return a;
}

ast *make_prefix_expression(token t, char *op) {
	ast *a = make_base_ast(t, ast_prefix_expression);
	a->prefix_expr.op = strdup(op);
	return a;
}

ast *make_infix_expression(token t, char *op, ast *left) {
	ast *a = make_base_ast(t, ast_infix_expression);
	a->infix_expr.op = strdup(op);
	a->infix_expr.left = left;
	return a;
}

ast *make_if_expression(token t) {
    ast *a = make_base_ast(t, ast_if_expr);
    a->if_expr.alternative = NULL;
    a->if_expr.consequence = NULL;
    return a;
}

ast *make_function_statement(token t) {
    ast *a = make_base_ast(t, ast_function_statement);
    a->function_stmt.body = NULL;
    a->function_stmt.parameters = NULL;
    return a;
}

ast *make_call_expression(token t, ast *function) {
    ast *a = make_base_ast(t, ast_call_expression);
    a->call_expr.function = function;
    a->call_expr.arguments = NULL;

    return a;
}

static sds expression_stmt_to_str(ast *a) {

	if (a->expr_stmt != NULL) {
		return ast_to_string(a->expr_stmt);
	}
	return sdsempty();
}

static sds return_stmt_to_str(ast *a) {

	sds buf = sdsempty();

	buf = sdscatfmt(buf, "%s ", token_literal(a));

	if(a->return_stmt.return_values != NULL) {
        int n = arrlen(a->return_stmt.return_values);
		buf = sdscat(buf, ast_to_string(a->return_stmt.return_values[0]));

        for(int i = 1; i < n; i++) {
            buf = sdscatfmt(buf, ", %s", ast_to_string(a->return_stmt.return_values[i]));
        }
	}

	buf = sdscat(buf, ";");
	return buf;
}

static sds assignement_stmt_to_str(ast *a) {

	sds buf = sdsempty();

	buf = sdscatfmt(buf, "%s ", token_literal(a));
	buf = sdscatfmt(buf, "%s", a->assignement_stmt.name->identifier.value);
	buf = sdscat(buf, " = ");


	if(a->assignement_stmt.value != NULL) {
		buf = sdscat(buf, ast_to_string(a->assignement_stmt.value));
	}

	return buf;

}

static sds number_literal_to_str(ast *a) {
	sds buf = sdsempty();
	sdscatprintf(buf, "%lf", a->num_literal.value);
	return buf;
}

static sds identifier_to_str(ast *a) {
    sds buf = sdsempty();
    sdscatprintf(buf, "%s", a->identifier.value);
    return buf;
}
static sds boolean_literal_to_str(ast *a) {
    sds buf = sdsempty();
    sdscatprintf(buf, "%s", a->token.literal);
    return buf;
}


static sds prefix_expr_to_str(ast *a) {

	sds buf = sdsempty();

	buf = sdscat(buf, "(");
	buf = sdscatfmt(buf, "%s", a->prefix_expr.op);
	buf = sdscatfmt(buf, "%s", ast_to_string(a->prefix_expr.right));
	buf = sdscat(buf, ")");

	return buf;

}

static sds infix_expr_to_str(ast *a) {

	sds buf = sdsempty();

	buf = sdscat(buf, "(");
	buf = sdscatfmt(buf, "%s", ast_to_string(a->infix_expr.left));
	buf = sdscatfmt(buf, "%s", a->infix_expr.op);
	buf = sdscatfmt(buf, "%s", ast_to_string(a->infix_expr.right));
	buf = sdscat(buf, ")");

	return buf;

}

static sds if_expr_to_str(ast *a) {

    sds buf = sdsempty();

    buf = sdscat(buf, "if");
    buf = sdscatfmt(buf, "%s {\n", ast_to_string(a->if_expr.condition));

    int n = arrlen(a->if_expr.consequence);
    for(int i = 0; i < n; i++) {
        buf = sdscatfmt(buf, "%s\n", ast_to_string(a->if_expr.consequence[i]));
    }

    buf = sdscat(buf, "}");

    n = arrlen(a->if_expr.alternative);

    if(n) {
        buf = sdscat(buf, "else {\n");
        for(int i = 0; i < n; i++) {
            buf = sdscatfmt(buf, "%s\n", ast_to_string(a->if_expr.alternative[i]));
        }
        buf = sdscat(buf, "}");
    }


    return buf;

}

static sds while_stmt_to_str(ast *a) {

    sds buf = sdsempty();

    buf = sdscat(buf, "while");
    buf = sdscatfmt(buf, "%s ", ast_to_string(a->while_stmt.condition));

    buf = sdscat(buf, "{");

    int n = arrlen(a->while_stmt.body);
    for(int i = 0; i < n; i++) {
        buf = sdscatfmt(buf, "%s\n", ast_to_string(a->while_stmt.body[i]));
    }

    buf = sdscat(buf, "}");
    return buf;

}

static sds function_stmt_to_str(ast *a) {

    sds buf = sdsempty();

    buf = sdscat(buf, token_literal(a));

	buf = sdscatfmt(buf, " %s", ast_to_string(a->function_stmt.name));

    buf = sdscat(buf, "(");

    int n = arrlen(a->function_stmt.parameters);

    if(n) {
        buf = sdscat(buf, ast_to_string(a->function_stmt.parameters[0]));

        for(int i = 1; i < n; i++) {
            buf = sdscatfmt(buf, ", %s", ast_to_string(a->function_stmt.parameters[i]));
        }
    }

    buf = sdscat(buf, ")\n");

    n = arrlen(a->function_stmt.body);
    buf = sdscat(buf, "{\n");
    for(int i = 0; i < n; i++) {
        buf = sdscatfmt(buf, "%s\n", ast_to_string(a->function_stmt.body[i]));
    }
    buf = sdscat(buf, "}\n");
    return buf;
}

static sds call_expr_to_str(ast *a) {

    sds buf = sdsempty();

    buf = sdscat(buf, ast_to_string(a->call_expr.function));
    buf = sdscat(buf, "(");

    int n = arrlen(a->call_expr.arguments);

    if(n) {
        buf = sdscat(buf, ast_to_string(a->call_expr.arguments[0]));

        for(int i = 1; i < n; i++) {
            buf = sdscatfmt(buf, ", %s", ast_to_string(a->call_expr.arguments[i]));
        }
    }

    buf = sdscat(buf, ")");

    return buf;
}

sds ast_to_string(ast *a) {

	if(a->tag == ast_ode_stmt || a->tag == ast_initial_stmt || a->tag == ast_global_stmt) {
		return assignement_stmt_to_str(a);
	}

	if(a->tag == ast_return_stmt) {
		return return_stmt_to_str(a);
	}

	if(a->tag == ast_expression_stmt) {
		return expression_stmt_to_str(a);
	}

	if(a->tag == ast_number_literal) {
		return number_literal_to_str(a);
	}

    if(a->tag == ast_boolean_literal) {
        return boolean_literal_to_str(a);
    }

    if(a->tag == ast_identifier) {
        return identifier_to_str(a);
    }

	if(a->tag == ast_prefix_expression) {
		return prefix_expr_to_str(a);
	}

	if(a->tag == ast_infix_expression) {
		return infix_expr_to_str(a);
	}

    if(a->tag == ast_if_expr) {
        return if_expr_to_str(a);
    }

    if(a->tag == ast_while_stmt) {
        return while_stmt_to_str(a);
    }

    if(a->tag == ast_function_statement) {
        return function_stmt_to_str(a);
    }

    if(a->tag == ast_call_expression) {
        return call_expr_to_str(a);
    }

    printf("[WARN] - to_str not implemented to operator %d\n", a->token.type);

	return NULL;

}

sds * program_to_string(program p) {

	int n_stmt = arrlen(p);
	sds *return_str = NULL;

	for (int i = 0; i < n_stmt; i++) {
		sds s = ast_to_string(p[i]);

		if(s) {
			arrput(return_str, s);
		}
	}

	return return_str;

}
