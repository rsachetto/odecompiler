#include "file_utils/file_utils.h"
#include "code_converter.h"
#include "stb/stb_ds.h"

static const char *builtin_functions[] = {
    "acos",
    "asin",
    "atan",
    "atan2",
    "ceil",
    "cos",
    "cosh",
    "exp",
    "fabs",
    "floor",
    "fmod",
    "frexp",
    "ldexp",
    "log",
    "log10",
    "modf",
    "pow",
    "sin",
    "sinh",
    "sqrt",
    "tan",
    "tanh",
    "cosh",
    "asinh",
    "atanh",
    "cbrt",
    "copysign",
    "erf",
    "erfc",
    "exp2",
    "expm1",
    "fdim",
    "fma",
    "fmax",
    "fmin",
    "hypot",
    "ilogb",
    "lgamma",
    "llrint",
    "lrint",
    "llround",
    "lround",
    "log1p",
    "log2",
    "logb",
    "nan",
    "nearbyint",
    "nextafter",
    "nexttoward",
    "remainder",
    "remquo",
    "rint",
    "round",
    "scalbln",
    "scalbn",
    "tgamma",
    "trunc"
};

static solver_type solver = EULER_ADPT_SOLVER;

static int indentation_level = 0;

static sds ast_to_c(ast *a, declared_variable_hash *declared_variables_in_scope, declared_variable_hash global_scope);

static sds expression_stmt_to_c(ast *a, declared_variable_hash *declared_variables_in_scope, declared_variable_hash global_scope) {

    if (a->expr_stmt != NULL) {
        return ast_to_c(a->expr_stmt, declared_variables_in_scope, global_scope);
    }
    return sdsempty();

}

static sds return_stmt_to_c(ast *a, declared_variable_hash *declared_variables_in_scope, declared_variable_hash global_scope) {

    sds buf = sdsempty();

    if(a->return_stmt.return_values != NULL) {
        int n = arrlen(a->return_stmt.return_values);

        if(n == 1) {
            buf = sdscatfmt(buf, "%s%s ", indent_spaces[indentation_level], token_literal(a));
            sds aux = ast_to_c(a->return_stmt.return_values[0], declared_variables_in_scope, global_scope);
            buf = sdscat(buf, aux);
            buf = sdscat(buf, ";");
        }

        else {
            for(int i = 0; i < n; i++) {
                buf = sdscatfmt(buf, "%s*ret_val_%i = %s;\n", indent_spaces[indentation_level], i, ast_to_c(a->return_stmt.return_values[i], declared_variables_in_scope, global_scope));
            }
        }

    }

    return buf;
}

static sds assignement_stmt_to_c(ast *a, declared_variable_hash *declared_variables_in_scope, declared_variable_hash global_scope) {

    sds buf = sdsempty();
    char *var_type;

    if(a->tag == ast_ode_stmt) {
        sds name = sdscatprintf(sdsempty(), "%.*s", (int)strlen(a->assignement_stmt.name->identifier.value)-1, a->assignement_stmt.name->identifier.value);

        int position = shget(*declared_variables_in_scope, name);

        sdsfree(name);
        if(solver == CVODE_SOLVER) {
            buf = sdscatprintf(buf, "%sNV_Ith_S(rDY, %d) = %s;\n", indent_spaces[indentation_level], position-1, ast_to_c(a->assignement_stmt.value, declared_variables_in_scope, global_scope));
        }
        else if(solver == EULER_ADPT_SOLVER) {
            buf = sdscatprintf(buf, "%srDY[%d] = %s;\n", indent_spaces[indentation_level], position-1, ast_to_c(a->assignement_stmt.value, declared_variables_in_scope, global_scope));
        }
    }

    else if (a->tag == ast_assignment_stmt) {

        if (a->assignement_stmt.value->tag == ast_boolean_literal || a->assignement_stmt.value->tag == ast_if_expr) {
            var_type = "bool";
        } else if (a->assignement_stmt.value->tag == ast_string_literal) {
            var_type = "char *";
        } else {
            var_type = "real";
        }

        int global = shget(global_scope, a->assignement_stmt.name->identifier.value);

        if(global) {
            buf = sdscatfmt(buf, "%s%s = %s;\n", indent_spaces[indentation_level], a->assignement_stmt.name->identifier.value, ast_to_c(a->assignement_stmt.value, declared_variables_in_scope, global_scope));
        }

        else {
            int declared = 1;
            if(declared_variables_in_scope)
                declared = shget(*(declared_variables_in_scope), a->assignement_stmt.name->identifier.value);

            if (!declared) {
                buf = sdscatfmt(buf, "%s%s %s = %s;\n", indent_spaces[indentation_level], var_type, a->assignement_stmt.name->identifier.value, ast_to_c(a->assignement_stmt.value, declared_variables_in_scope, global_scope));
                shput(*(declared_variables_in_scope), a->assignement_stmt.name->identifier.value, 1);
            } else {
                buf = sdscatfmt(buf, "%s%s = %s;\n", indent_spaces[indentation_level], a->assignement_stmt.name->identifier.value, ast_to_c(a->assignement_stmt.value, declared_variables_in_scope, global_scope));
            }
        }
    }
    else if (a->tag == ast_grouped_assignment_stmt) {

        ast **variables = a->grouped_assignement_stmt.names;
        int n = arrlen(variables);

        var_type = "real";

        if(n == 1) {

            char *id_name = a->grouped_assignement_stmt.names[0]->identifier.value;
            ast  *call_expr = a->grouped_assignement_stmt.call_expr;

            int global = shget(global_scope, id_name);

            if(global) {
                buf = sdscatfmt(buf, "%s%s = %s;\n", indent_spaces[indentation_level], id_name, ast_to_c(call_expr, declared_variables_in_scope, global_scope));
            }

            else {
                int declared = shget(*declared_variables_in_scope, id_name);

                if (!declared) {
                    buf = sdscatfmt(buf, "%s%s %s = %s;\n", indent_spaces[indentation_level], var_type, id_name, ast_to_c(call_expr, declared_variables_in_scope, global_scope));
                    shput(*declared_variables_in_scope, id_name, 1);
                } else {
                    buf = sdscatfmt(buf, "%s%s = %s;\n", indent_spaces[indentation_level], id_name, ast_to_c(call_expr, declared_variables_in_scope, global_scope));
                }
            }
        } else {

            char *f_name = a->grouped_assignement_stmt.call_expr->call_expr.function_identifier->identifier.value;
            declared_variable_entry dv = shgets(global_scope, f_name);

            int num_expected_assignements = dv.value;

            if(n != num_expected_assignements) {
                fprintf(stderr, "Error on line %d of file %s. Function %s returns %d values but %d are being assigned!\n", a->token.line_number, a->token.file_name, f_name, num_expected_assignements, n);
            }

            for(int j = 0; j < n; j++) {

                ast *id = variables[j];

                int global = shget(global_scope, id->identifier.value);

                if(!global) {

                    int declared = shget(*(declared_variables_in_scope), id->identifier.value);

                    if (!declared) {
                        buf = sdscatfmt(buf, "%s%s %s;\n", indent_spaces[indentation_level], var_type, id->identifier.value);
                        shput(*declared_variables_in_scope, id->identifier.value, 1);
                    }
                }

            }

            //Converting the function call here
            ast *b = a->grouped_assignement_stmt.call_expr;

            int num_expected_args = dv.num_args;
            int n_real_args = arrlen(b->call_expr.arguments);

            if(n_real_args != num_expected_args) {
                fprintf(stderr, "Error on line %d of file %s. Function %s expects %d parameters but %d are being passed!\n", a->token.line_number, a->token.file_name, f_name, num_expected_args, n_real_args);
            }

            buf = sdscatfmt(buf, "%s%s", indent_spaces[indentation_level], ast_to_c(b->call_expr.function_identifier, declared_variables_in_scope, global_scope));
            buf = sdscat(buf, "(");

            if(n_real_args) {
                buf = sdscat(buf, ast_to_c(b->call_expr.arguments[0], declared_variables_in_scope, global_scope));

                for(int i = 1; i < n_real_args; i++) {
                    buf = sdscatfmt(buf, ", %s", ast_to_c(b->call_expr.arguments[i], declared_variables_in_scope, global_scope));
                }

                for(int i = 0; i < n; i++) {
                    ast *id = variables[i];
                    buf = sdscatfmt(buf, ", &%s", id->identifier.value);
                }

            } else {

                buf = sdscatfmt(buf, "&%s", variables[0]->identifier.value);

                for(int i = 1; i < n; i++) {
                    ast *id = variables[i];
                    buf = sdscatfmt(buf, ", &%s", id->identifier.value);
                }
            }

            buf = sdscat(buf, ");\n");

        }
    }

    return buf;
}

static char* number_literal_to_c(ast *a) {
    char buf[128];
    sprintf(buf, "%e", a->num_literal.value);
    return strdup(buf);
}

static sds identifier_to_c(ast *a, declared_variable_hash *declared_variables_in_scope, declared_variable_hash global_scope) {

    int declared = shget(*declared_variables_in_scope, a->identifier.value) || shget(global_scope, a->identifier.value);

    if(!declared) {
        fprintf(stderr, "Error on line %d of file %s. Identifier %s not declared, assign a value to it before using!\n", a->token.line_number, a->token.file_name, a->identifier.value);
    }

    sds buf = sdsempty();
    buf = sdscat(buf, a->identifier.value);
    return buf;
}

static sds boolean_literal_to_c(ast *a) {
    sds buf = sdsempty();
    buf = sdscatprintf(buf, "%s", a->token.literal);
    return buf;
}

static sds string_literal_to_c(ast *a) {
    sds buf = sdsempty();
    buf = sdscatprintf(buf, "\"%s\"", a->token.literal);
    return buf;
}

static sds prefix_expr_to_c(ast *a, declared_variable_hash *declared_variables_in_scope, declared_variable_hash global_scope) {

    sds buf = sdsempty();

    buf = sdscat(buf, "(");
    buf = sdscatfmt(buf, "%s", a->prefix_expr.op);

    buf = sdscatfmt(buf, "%s", ast_to_c(a->prefix_expr.right, declared_variables_in_scope, global_scope));
    buf = sdscat(buf, ")");

    return buf;

}

static sds infix_expr_to_c(ast *a,  declared_variable_hash *declared_variables_in_scope, declared_variable_hash global_scope) {

    sds buf = sdsempty();

    buf = sdscat(buf, "(");
    buf = sdscatfmt(buf, "%s", ast_to_c(a->infix_expr.left,  declared_variables_in_scope, global_scope));

    if(strcmp(a->infix_expr.op, "and") == 0) {
        buf = sdscat(buf, "&&");
    }else if(strcmp(a->infix_expr.op, "or") == 0) {
        buf = sdscat(buf, "||");
    } else {
        buf = sdscatfmt(buf, "%s", a->infix_expr.op);
    }

    buf = sdscatfmt(buf, "%s", ast_to_c(a->infix_expr.right, declared_variables_in_scope, global_scope));
    buf = sdscat(buf, ")");

    return buf;

}

static sds if_expr_to_c(ast *a, declared_variable_hash *declared_variables_in_scope, declared_variable_hash global_scope) {

    sds buf = sdsempty();

    buf = sdscatfmt(buf, "%sif", indent_spaces[indentation_level]);
    buf = sdscatfmt(buf, "%s {\n", ast_to_c(a->if_expr.condition, declared_variables_in_scope, global_scope));

    indentation_level++;
    int n = arrlen(a->if_expr.consequence);
    for(int i = 0; i < n; i++) {
        buf = sdscatfmt(buf, "%s\n", ast_to_c(a->if_expr.consequence[i], declared_variables_in_scope, global_scope));
    }
    indentation_level--;

    buf = sdscatfmt(buf, "%s}", indent_spaces[indentation_level]);

    n = arrlen(a->if_expr.alternative);

    if(n) {
        buf = sdscat(buf, " else {\n");
        indentation_level++;
        for(int i = 0; i < n; i++) {
            buf = sdscatfmt(buf, "%s\n",  ast_to_c(a->if_expr.alternative[i], declared_variables_in_scope, global_scope));

        }
        indentation_level--;

        buf = sdscatfmt(buf, "%s}\n", indent_spaces[indentation_level]);

    }
    else if(a->if_expr.elif_alternative) {
        buf = sdscatfmt(buf, " else %s", ast_to_c(a->if_expr.elif_alternative, declared_variables_in_scope, global_scope));
    }

    return buf;

}

static sds while_stmt_to_c(ast *a, declared_variable_hash *declared_variables_in_scope, declared_variable_hash global_scope) {

    sds buf = sdsempty();

    buf = sdscat(buf, "while");
    buf = sdscatfmt(buf, "%s ", ast_to_c(a->while_stmt.condition, declared_variables_in_scope, global_scope));

    buf = sdscat(buf, "{");

    int n = arrlen(a->while_stmt.body);
    for(int i = 0; i < n; i++) {
        buf = sdscatfmt(buf, "%s\n", ast_to_c(a->while_stmt.body[i],  declared_variables_in_scope, global_scope));
    }

    buf = sdscat(buf, "}");
    return buf;

}

static sds call_expr_to_c(ast *a, declared_variable_hash *declared_variables_in_scope, declared_variable_hash global_scope) {

    char *f_name = a->call_expr.function_identifier->identifier.value;

    declared_variable_entry dv = shgets(global_scope, f_name);
    int num_return_values = dv.value;

    if (num_return_values != 1) {
        fprintf(stderr, "Error on line %d of file %s. Function %s returns %d values but 1 is being assigned!\n", a->token.line_number, a->token.file_name, f_name, num_return_values);
    }

    int num_expected_args = dv.num_args;
    int n_real_args = arrlen(a->call_expr.arguments);

    if(n_real_args != num_expected_args) {
        fprintf(stderr, "Error on line %d of file %s. Function %s expects %d parameters but %d are being passed!\n", a->token.line_number, a->token.file_name, f_name, num_expected_args, n_real_args);
    }

    sds buf = sdsempty();

    buf = sdscat(buf, ast_to_c(a->call_expr.function_identifier, declared_variables_in_scope, global_scope));
    buf = sdscat(buf, "(");

    int n = arrlen(a->call_expr.arguments);

    if(n) {
        buf = sdscat(buf, ast_to_c(a->call_expr.arguments[0], declared_variables_in_scope, global_scope));

        for(int i = 1; i < n; i++) {
            buf = sdscatfmt(buf, ", %s", ast_to_c(a->call_expr.arguments[i], declared_variables_in_scope, global_scope));
        }
    }

    buf = sdscat(buf, ")");

    return buf;
}

static sds global_variable_to_c(ast *a, declared_variable_hash global_scope) {

    sds buf = sdsempty();
    buf = sdscatfmt(buf, "real %s = %s;\n", a->assignement_stmt.name->identifier.value, ast_to_c(a->assignement_stmt.value, NULL, global_scope));

    return buf;

}

static sds ast_to_c(ast *a, declared_variable_hash *declared_variables_in_scope, declared_variable_hash global_scope) {

    if(a->tag == ast_assignment_stmt || a->tag == ast_grouped_assignment_stmt | a->tag == ast_ode_stmt) {
        return assignement_stmt_to_c(a, declared_variables_in_scope, global_scope);
    }

    if(a->tag == ast_global_stmt) {
        return global_variable_to_c(a, global_scope);
    }

    if(a->tag == ast_return_stmt) {
        return return_stmt_to_c(a, declared_variables_in_scope, global_scope);
    }

    if(a->tag == ast_expression_stmt) {
        return expression_stmt_to_c(a, declared_variables_in_scope, global_scope);
    }

    if(a->tag == ast_number_literal) {
        return number_literal_to_c(a);
    }

    if(a->tag == ast_boolean_literal) {
        return boolean_literal_to_c(a);
    }
    if( a->tag == ast_string_literal) {
        return string_literal_to_c(a);
    }

    if(a->tag == ast_identifier) {
        return identifier_to_c(a, declared_variables_in_scope, global_scope);
    }

    if(a->tag == ast_prefix_expression) {
        return prefix_expr_to_c(a, declared_variables_in_scope, global_scope);
    }

    if(a->tag == ast_infix_expression) {
        return infix_expr_to_c(a,  declared_variables_in_scope, global_scope);
    }

    if(a->tag == ast_if_expr) {
        return if_expr_to_c(a, declared_variables_in_scope, global_scope);
    }

    if(a->tag == ast_while_stmt) {
        return while_stmt_to_c(a, declared_variables_in_scope, global_scope);
    }

    if(a->tag == ast_call_expression) {
        return call_expr_to_c(a, declared_variables_in_scope, global_scope);
    }

    printf("[WARN] Line %d of file %s - to_c not implemented to operator %d\n", a->token.line_number, a->token.file_name, a->tag);

    return NULL;

}

void write_initial_conditions(program p, FILE *file, declared_variable_hash *declared_variables_in_scope) {

    int n_stmt = arrlen(p);
    for(int i = 0; i < n_stmt; i++) {
        ast *a = p[i];

        int position = shget(*declared_variables_in_scope, a->assignement_stmt.name->identifier.value);

        if(solver == CVODE_SOLVER) {
            fprintf(file, "    NV_Ith_S(x0, %d) = values[%d]; //%s\n", position-1, position-1, a->assignement_stmt.name->identifier.value);
        }
        else if(solver == EULER_ADPT_SOLVER) {
            fprintf(file, "    x0[%d] = values[%d]; //%s\n", position-1, position-1, a->assignement_stmt.name->identifier.value);
        }
    }

}

void generate_initial_conditions_values(program p, program body, FILE *file, declared_variable_hash *declared_variables_in_scope, declared_variable_hash global_scope) {

    ode_initialized_hash_entry *result = NULL;
    shdefault(result, false);
    sh_new_strdup(result);

    int n_stmt = arrlen(p);

    fprintf(file, "    real values[%d];\n", n_stmt);

    for(int i = 0; i < n_stmt; i++) {
        ast *a = p[i];

        if(!shget(result, a->assignement_stmt.name->identifier.value)) {
            shput(result, a->assignement_stmt.name->identifier.value, true);
        } else {
            printf("Warning on line %d of file %s. Duplicate initialization ode variable %s\n", a->token.line_number, a->token.file_name, a->assignement_stmt.name->identifier.value);
        }

        int position = shget(*declared_variables_in_scope, a->assignement_stmt.name->identifier.value);
        fprintf(file, "    values[%d] = %s; //%s\n", position-1, ast_to_c(a->assignement_stmt.value, declared_variables_in_scope, global_scope), a->assignement_stmt.name->identifier.value);
    }

    int n_odes = arrlen(body);

    string_array non_initialized_edos = NULL;

    for(int i = 0; i < n_odes; i++) {
        ast *a = body[i];

        if(a->tag != ast_ode_stmt) continue;


        char *tmp = strndup(a->assignement_stmt.name->identifier.value, (int)strlen(a->assignement_stmt.name->identifier.value)-1);
        if(shget(result, tmp)) {
            shdel(result, tmp);
        }
        else {
            arrput(non_initialized_edos, tmp);
        }
    }

    int wrong_initialized = shlen(result);
    for(int i = 0; i < wrong_initialized; i++) {
        printf("Error - initialization of a non ode variable (%s)\n", result[i].key);
    }

    int non_initialized = arrlen(non_initialized_edos);
    for(int i = 0; i < non_initialized; i++) {
        fprintf(stderr, "Warning - No initial condition provided for %s'!\n", non_initialized_edos[i]);
        free(non_initialized_edos[i]);
    }

    arrfree(non_initialized_edos);
    shfree(result);

}

void write_odes_old_values(program p, declared_variable_hash declared_variables_in_scope, FILE *file) {

    int n_stmt = arrlen(p);
    for(int i = 0; i < n_stmt; i++) {

        ast *a = p[i];

        if(a->tag != ast_ode_stmt) continue;

        sds name = sdscatprintf(sdsempty(), "%.*s", (int)strlen(a->assignement_stmt.name->identifier.value)-1, a->assignement_stmt.name->identifier.value);
        int position = shget(declared_variables_in_scope, name);
        sdsfree(name);

        if (solver == CVODE_SOLVER) {
            fprintf(file, "    const real %.*s =  NV_Ith_S(sv, %d);\n", (int) strlen(a->assignement_stmt.name->identifier.value) - 1, a->assignement_stmt.name->identifier.value, position-1);
        } else if (solver == EULER_ADPT_SOLVER) {
            fprintf(file, "    const real %.*s =  sv[%d];\n", (int) strlen(a->assignement_stmt.name->identifier.value) - 1, a->assignement_stmt.name->identifier.value, position-1);
        }
    }
}

sds out_file_header(program p) {

    sds ret = sdsempty();

    int n_stmt = arrlen(p);

    ret = sdscatprintf(ret, "\"#t");

    ast *a ;
    for(int i = 0; i < n_stmt; i++) {
        a = p[i];
        if(a->tag != ast_ode_stmt) continue;

        ret = sdscatprintf(ret, ", %.*s", (int)strlen(a->assignement_stmt.name->identifier.value)-1, a->assignement_stmt.name->identifier.value);
    }

    ret = sdscat(ret, "\\n\"");

    return ret;
}

void write_variables_or_body(program p, FILE *file, declared_variable_hash *declared_variables, declared_variable_hash global_scope) {
    int n_stmt = arrlen(p);
    for(int i = 0; i < n_stmt; i++) {
        ast *a = p[i];
        if(a->tag == ast_ode_stmt) {
            sds name = sdscatprintf(sdsempty(), "%.*s", (int)strlen(a->assignement_stmt.name->identifier.value)-1, a->assignement_stmt.name->identifier.value);

            int position = shget(*declared_variables, name);

            sdsfree(name);
            if(solver == CVODE_SOLVER) {
                fprintf(file, "    NV_Ith_S(rDY, %d) = %s;\n", position-1, ast_to_c(a->assignement_stmt.value, declared_variables, global_scope));
            }
            else if(solver == EULER_ADPT_SOLVER) {
                fprintf(file, "    rDY[%d] = %s;\n", position-1, ast_to_c(a->assignement_stmt.value, declared_variables, global_scope));
            }
        }
        else {
            fprintf(file, "%s", ast_to_c(a, declared_variables, global_scope));
        }
    }
}

void write_odes(program p, FILE *file, declared_variable_hash *declared_variables_in_scope, declared_variable_hash global_scope) {

    int n_stmt = arrlen(p);
    for(int i = 0; i < n_stmt; i++) {
        ast *a = p[i];
        if(solver == CVODE_SOLVER) {
            fprintf(file, "    NV_Ith_S(rDY, %d) = %s;\n", i, ast_to_c(a->assignement_stmt.value, declared_variables_in_scope, global_scope));
        }
        else if(solver == EULER_ADPT_SOLVER) {
            fprintf(file, "    rDY[%d] = %s;\n", i, ast_to_c(a->assignement_stmt.value, declared_variables_in_scope, global_scope));
        }

    }

}

void write_functions(program p, FILE *file, declared_variable_hash global_scope) {

    int n_stmt = arrlen(p);

    for(int i = 0; i < n_stmt; i++) {

        ast *a = p[i];

        if(a->function_stmt.num_return_values == 1) {
            fprintf(file, "real %s", a->function_stmt.name->identifier.value);
        }

        else {
            fprintf(file, "void %s", a->function_stmt.name->identifier.value);
        }

        fprintf(file, "(");

        int n = arrlen(a->function_stmt.parameters);

        declared_variable_hash declared_variables_in_parameters_list = NULL;
        shdefault(declared_variables_in_parameters_list, 0);
        sh_new_arena(declared_variables_in_parameters_list);

        if(n) {
            shput(declared_variables_in_parameters_list, a->function_stmt.parameters[0]->identifier.value, 1);
            fprintf(file, "real %s", ast_to_c(a->function_stmt.parameters[0], &declared_variables_in_parameters_list, global_scope));
            for(int j = 1; j < n; j++) {
                shput(declared_variables_in_parameters_list, a->function_stmt.parameters[j]->identifier.value, 1);
                fprintf(file, ", real %s", ast_to_c(a->function_stmt.parameters[j], &declared_variables_in_parameters_list, global_scope));
            }

            if(a->function_stmt.num_return_values > 1) {
                for(int j = 0; j < a->function_stmt.num_return_values; j++) {
                    fprintf(file, ", real *ret_val_%d", j);
                }
            }

        }
        else {
            if(a->function_stmt.num_return_values > 1) {
                fprintf(file, "real *ret_val_%d", 0);
                for(int k = 1; k < a->function_stmt.num_return_values; k++) {
                    fprintf(file, ", real *ret_val_%d", k);
                }
            }
        }

        fprintf(file, ") {\n");

        n = arrlen(a->function_stmt.body);
        indentation_level++;
        for(int j = 0; j < n; j++) {
            fprintf(file, "%s\n", ast_to_c(a->function_stmt.body[j],  &declared_variables_in_parameters_list, global_scope));
        }
        indentation_level--;
        fprintf(file, "}\n\n");
    }
}

void process_imports(ast **imports, ast ***functions) {
    int n = arrlen(imports);

    for(int i = 0; i < n; i++) {
        ast *a = imports[i];
        const char *import_file_name = a->import_stmt.filename->identifier.value;
        size_t file_size;

        char *source = read_entire_file_with_mmap(import_file_name, &file_size);

        if(!source) {
            fprintf(stderr, "Error importing file %s\n", import_file_name);
            exit(0);
        }

        lexer *l = new_lexer(source, import_file_name);
        parser *p = new_parser(l);
        program program = parse_program(p);

        check_parser_errors(p, true);

        int n_stmt = arrlen(program);
        for(int s = 0; s < n_stmt; s++) {
            //we only import functions for now
            if(program[s]->tag == ast_function_statement) {
                arrput(*functions, program[s]);
            }
            else {
                fprintf(stderr, "[WARN] - Importing from file %s in line %d. Currently, we only import functions. Nested imports or global variables will not be imported!\n", import_file_name, program[s]->token.line_number);
            }
        }

        munmap(source, file_size);

    }
}

declared_variable_hash create_variables_scope(ast **variables) {
    declared_variable_hash declared_variables = NULL;
    shdefault(declared_variables, 0);
    sh_new_arena(declared_variables);

    int count = 1;

    //Adding VARS to scope.
    for(int i = 0; i < arrlen(variables); i++) {
        ast *a = variables[i];
        if(a->tag == ast_assignment_stmt) {
            shput(declared_variables, a->assignement_stmt.name->identifier.value, 0);
        }
        else if(a->tag == ast_ode_stmt) {
            char *tmp = strndup(a->assignement_stmt.name->identifier.value, (int)strlen(a->assignement_stmt.name->identifier.value)-1);
            //The key in this hash is the order of appearance of the ODE. This is important to define the order of the initial conditions
            shput(declared_variables, tmp, count);
            count++;
            free(tmp);

        }
    }

    return declared_variables;
}

declared_variable_hash create_functions_and_global_scope(ast **functions, ast **globals) {

    declared_variable_hash declared_variables = NULL;
    shdefault(declared_variables, 0);
    sh_new_arena(declared_variables);

    //variable time is auto declared in the scope
    shput(declared_variables, "time", 1);

    int nb = sizeof(builtin_functions)/sizeof(builtin_functions[0]);

    for(int i = 0; i < nb; i++) {
        declared_variable_entry tmp;
        tmp.key = (char*) builtin_functions[i];
        tmp.value = 1;
        tmp.num_args = 1;

        if(strcmp(builtin_functions[i], "pow") == 0       ||
                strcmp(builtin_functions[i], "modf") == 0      ||
                strcmp(builtin_functions[i], "copysign") == 0  ||
                strcmp(builtin_functions[i], "exp2") == 0      ||
                strcmp(builtin_functions[i], "expm1") == 0     ||
                strcmp(builtin_functions[i], "fdim") == 0      ||
                strcmp(builtin_functions[i], "fmax") == 0      ||
                strcmp(builtin_functions[i], "fmin") == 0      ||
                strcmp(builtin_functions[i], "hypot") == 0     ||
                strcmp(builtin_functions[i], "nextafter") == 0 ||
                strcmp(builtin_functions[i], "nexttoward") == 0||
                strcmp(builtin_functions[i], "scalbln") == 0   ||
                strcmp(builtin_functions[i], "scalbn") == 0)   {
            tmp.num_args = 2;
        }

        else if(strcmp(builtin_functions[i], "fma") == 0 || strcmp(builtin_functions[i], "remquo") == 0) {
            tmp.num_args = 3;
        }

        shputs(declared_variables, tmp);
    }

    //Ading function identifiers to scope.
    for(int i = 0; i < arrlen(functions); i++) {
        declared_variable_entry tmp;
        tmp.key = functions[i]->function_stmt.name->identifier.value;
        tmp.value = functions[i]->function_stmt.num_return_values;
        tmp.num_args = arrlen(functions[i]->function_stmt.parameters);
        shputs(declared_variables, tmp);
    }

    for(int i = 0; i < arrlen(globals); i++) {
        shput(declared_variables, globals[i]->assignement_stmt.name->identifier.value, 1);
    }

    return declared_variables;

}

void write_cvode_solver(FILE *file, program initial, program globals, program functions, program main_body, sds out_header, declared_variable_hash variables_and_odes_scope, declared_variable_hash global_scope) {

    fprintf(file,"#include <cvode/cvode.h>\n"
            "#include <math.h>\n"
            "#include <nvector/nvector_serial.h>\n"
            "#include <stdbool.h>\n"
            "#include <stdio.h>\n"
            "#include <stdlib.h>\n"
            "#include <sundials/sundials_dense.h>\n"
            "#include <sundials/sundials_types.h>\n"
            "#include <sunlinsol/sunlinsol_dense.h> \n"
            "#include <sunmatrix/sunmatrix_dense.h>"
            " \n\n");

    fprintf(file, "#define NEQ %d\n", (int)arrlen(initial));
    fprintf(file, "typedef realtype real;\n");

    write_variables_or_body(globals, file, &variables_and_odes_scope, global_scope);
    fprintf(file, "\n");

    write_functions(functions, file, global_scope);

    fprintf(file, "void set_initial_conditions(N_Vector x0, real *values) { \n\n");
    write_initial_conditions(initial, file, &variables_and_odes_scope);
    fprintf(file, "\n}\n\n");

    // RHS CPU
    fprintf(file, "static int solve_model(realtype time, N_Vector sv, N_Vector rDY, void *f_data) {\n\n");

    fprintf(file, "    //State variables\n");
    write_odes_old_values(main_body, variables_and_odes_scope, file);
    fprintf(file, "\n");

    fprintf(file, "    //Parameters\n");

    indentation_level++;
    write_variables_or_body(main_body, file, &variables_and_odes_scope, global_scope);
    indentation_level--;

    //write_odes(odes, file, &variables_and_odes_scope, global_scope, CVODE_SOLVER);

    fprintf(file, "\n\treturn 0;  \n\n}\n\n");

    fprintf(file, "static int check_flag(void *flagvalue, const char *funcname, int opt) {\n"
            "\n"
            "    int *errflag;\n"
            "\n"
            "    /* Check if SUNDIALS function returned NULL pointer - no memory allocated */\n"
            "    if(opt == 0 && flagvalue == NULL) {\n"
            "\n"
            "        fprintf(stderr, \"\\nSUNDIALS_ERROR: %%s() failed - returned NULL pointer\\n\\n\", funcname);\n"
            "        return (1);\n"
            "    }\n"
            "\n"
            "    /* Check if flag < 0 */\n"
            "    else if(opt == 1) {\n"
            "        errflag = (int *)flagvalue;\n"
            "        if(*errflag < 0) {\n"
            "            fprintf(stderr, \"\\nSUNDIALS_ERROR: %%s() failed with flag = %%d\\n\\n\", funcname, *errflag);\n"
            "            return (1);\n"
            "        }\n"
            "    }\n"
            "\n"
            "    /* Check if function returned NULL pointer - no memory allocated */\n"
            "    else if(opt == 2 && flagvalue == NULL) {\n"
            "        fprintf(stderr, \"\\nMEMORY_ERROR: %%s() failed - returned NULL pointer\\n\\n\", funcname);\n"
            "        return (1);\n"
            "    }\n"
            "\n"
            "    return (0);\n"
            "}\n");

    fprintf(file, "void solve_ode(N_Vector y, float final_t, char *file_name) {\n"
            "\n"
            "    void *cvode_mem = NULL;\n"
            "    int flag;\n"
            "\n"
            "    // Set up solver\n"
            "    cvode_mem = CVodeCreate(CV_BDF);\n"
            "\n"
            "    if(cvode_mem == 0) {\n"
            "        fprintf(stderr, \"Error in CVodeMalloc: could not allocate\\n\");\n"
            "        return;\n"
            "    }\n"
            "\n"
            "    flag = CVodeInit(cvode_mem, %s, 0, y);\n"
            "    if(check_flag(&flag, \"CVodeInit\", 1))\n"
            "        return;\n"
            "\n"
            "    flag = CVodeSStolerances(cvode_mem, 1.49012e-6, 1.49012e-6);\n"
            "    if(check_flag(&flag, \"CVodeSStolerances\", 1))\n"
            "        return;\n"
            "\n"
            "    // Create dense SUNMatrix for use in linear solver\n"
            "    SUNMatrix A = SUNDenseMatrix(NEQ, NEQ);\n"
            "    if(check_flag((void *)A, \"SUNDenseMatrix\", 0))\n"
            "        return;\n"
            "\n"
            "    // Create dense linear solver for use by CVode\n"
            "    SUNLinearSolver LS = SUNLinSol_Dense(y, A);\n"
            "    if(check_flag((void *)LS, \"SUNLinSol_Dense\", 0))\n"
            "        return;\n"
            "\n"
            "    // Attach the linear solver and matrix to CVode by calling CVodeSetLinearSolver\n"
            "    flag = CVodeSetLinearSolver(cvode_mem, LS, A);\n"
            "    if(check_flag((void *)&flag, \"CVodeSetLinearSolver\", 1))\n"
            "        return;\n"
            "\n"
            "    realtype dt=0.01;\n"
            "    realtype tout = dt;\n"
            "    int retval;\n"
            "    realtype t;\n"
            "\n"
            "    FILE *f = fopen(file_name, \"w\");\n"
            "    fprintf(f, %s);\n"
            "    while(tout < final_t) {\n"
            "\n"
            "        retval = CVode(cvode_mem, tout, y, &t, CV_NORMAL);\n"
            "\n"
            "        if(retval == CV_SUCCESS) {"
            "            fprintf(f, \"%%lf \", t);\n"
            "            for(int i = 0; i < NEQ; i++) {\n"
            "                fprintf(f, \"%%lf \", NV_Ith_S(y,i));\n"
            "            }\n"
            "\n"
            "            fprintf(f, \"\\n\");\n"
            "\n"
            "            tout+=dt;\n"
            "        }\n"
            "\n"
            "    }\n"
            "\n"
            "    // Free the linear solver memory\n"
            "    SUNLinSolFree(LS);\n"
            "    SUNMatDestroy(A);\n"
            "    CVodeFree(&cvode_mem);\n"
            "}\n", "solve_model", out_header);


    fprintf(file, "\nint main(int argc, char **argv) {\n"
            "\n"
            "\tN_Vector x0 = N_VNew_Serial(NEQ);\n"
            "\n");
    generate_initial_conditions_values(initial, main_body, file, &variables_and_odes_scope, global_scope);
    fprintf(file, "\tset_initial_conditions(x0, values);\n"
            "\n"
            "\tsolve_ode(x0, strtod(argv[1], NULL), argv[2]);\n"
            "\n"
            "\n"
            "\treturn (0);\n"
            "}");
}

void write_adpt_euler_solver(FILE *file, program initial, program globals, program functions, program main_body, sds out_header, declared_variable_hash variables_and_odes_scope, declared_variable_hash global_scope) {

    fprintf(file,"#include <math.h>\n"
            "#include <stdbool.h>\n"
            "#include <stdio.h>\n"
            "#include <stdlib.h>\n"
            " \n\n");

    fprintf(file, "#define NEQ %d\n", (int)arrlen(initial));
    fprintf(file, "typedef double real;\n");

    write_variables_or_body(globals, file, &variables_and_odes_scope, global_scope);
    fprintf(file, "\n");

    write_functions(functions, file, global_scope);

    fprintf(file, "void set_initial_conditions(real *x0, real *values) { \n\n");
    write_initial_conditions(initial, file, &variables_and_odes_scope);
    fprintf(file, "\n}\n\n");

    // RHS CPU
    fprintf(file, "static int solve_model(real time, real *sv, real *rDY) {\n\n");

    fprintf(file, "    //State variables\n");
    write_odes_old_values(main_body, variables_and_odes_scope, file);
    fprintf(file, "\n");

    fprintf(file, "    //Parameters\n");

    indentation_level++;
    write_variables_or_body(main_body, file, &variables_and_odes_scope, global_scope);
    indentation_level--;

    //write_odes(odes, file, &variables_and_odes_scope, global_scope, EULER_ADPT_SOLVER);

    fprintf(file, "\n\treturn 0;  \n\n}\n\n");

    fprintf(file, "void solve_ode(real *sv, float final_time, char *file_name) {\n"
            "\n"
            "    real rDY[NEQ];\n"
            "\n"
            "    real reltol = 1e-5;\n"
            "    real abstol = 1e-5;\n"
            "    real _tolerances_[NEQ];\n"
            "    real _aux_tol = 0.0;\n"
            "    //initializes the variables\n"
            "    real dt = 0.000001;\n"
            "    real time_new = 0.0;\n"
            "    real previous_dt = dt;\n"
            "\n"
            "    real edos_old_aux_[NEQ];\n"
            "    real edos_new_euler_[NEQ];\n"
            "    real *_k1__ = (real*) malloc(sizeof(real)*NEQ);\n"
            "    real *_k2__ = (real*) malloc(sizeof(real)*NEQ);\n"
            "    real *_k_aux__;\n"
            "\n"
            "    const real _beta_safety_ = 0.8;\n"
            "\n"
            "    const real __tiny_ = pow(abstol, 2.0f);\n"
            "\n"
            "    if(time_new + dt > final_time) {\n"
            "       dt = final_time - time_new;\n"
            "    }\n"
            "\n"
            "    solve_model(time_new, sv, rDY);\n"
            "    time_new += dt;\n"
            "\n"
            "    for(int i = 0; i < NEQ; i++){\n"
            "        _k1__[i] = rDY[i];\n"
            "    }\n"
            "\n"
            "    FILE *f = fopen(file_name, \"w\");\n"
            "    fprintf(f, %s);\n"
            "    while(1) {\n"
            "\n"
            "        for(int i = 0; i < NEQ; i++) {\n"
            "            //stores the old variables in a vector\n"
            "            edos_old_aux_[i] = sv[i];\n"
            "            //computes euler method\n"
            "            edos_new_euler_[i] = _k1__[i] * dt + edos_old_aux_[i];\n"
            "            //steps ahead to compute the rk2 method\n"
            "            sv[i] = edos_new_euler_[i];\n"
            "        }\n"
            "\n"
            "        time_new += dt;\n"
            "        solve_model(time_new, sv, rDY);\n"
            "        time_new -= dt;//step back\n"
            "\n"
            "        double greatestError = 0.0, auxError = 0.0;\n"
            "        for(int i = 0; i < NEQ; i++) {\n"
            "            // stores the new evaluation\n"
            "            _k2__[i] = rDY[i];\n"
            "            _aux_tol = fabs(edos_new_euler_[i]) * reltol;\n"
            "            _tolerances_[i] = (abstol > _aux_tol) ? abstol : _aux_tol;\n"
            "\n"
            "            // finds the greatest error between  the steps\n"
            "            auxError = fabs(((dt / 2.0) * (_k1__[i] - _k2__[i])) / _tolerances_[i]);\n"
            "\n"
            "            greatestError = (auxError > greatestError) ? auxError : greatestError;\n"
            "        }\n"
            "        ///adapt the time step\n"
            "        greatestError += __tiny_;\n"
            "        previous_dt = dt;\n"
            "        ///adapt the time step\n"
            "        dt = _beta_safety_ * dt * sqrt(1.0f/greatestError);\n"
            "\n"
            "        if (time_new + dt > final_time) {\n"
            "            dt = final_time - time_new;\n"
            "        }\n"
            "\n"
            "        //it doesn't accept the solution\n"
            "        if ((greatestError >= 1.0f) && dt > 0.00000001) {\n"
            "            //restore the old values to do it again\n"
            "            for(int i = 0;  i < NEQ; i++) {\n"
            "                sv[i] = edos_old_aux_[i];\n"
            "            }\n"
            "            //throw the results away and compute again\n"
            "        } else{//it accepts the solutions\n"
            "\n"
            "            if (time_new + dt > final_time) {\n"
            "                dt = final_time - time_new;\n"
            "            }\n"
            "\n"
            "            _k_aux__ = _k2__;\n"
            "            _k2__\t= _k1__;\n"
            "            _k1__\t= _k_aux__;\n"
            "\n"
            "            //it steps the method ahead, with euler solution\n"
            "            for(int i = 0; i < NEQ; i++){\n"
            "                sv[i] = edos_new_euler_[i];\n"
            "            }\n"

            "            fprintf(f, \"%%lf \", time_new);\n"
            "            for(int i = 0; i < NEQ; i++) {\n"
            "                fprintf(f, \"%%lf \", sv[i]);\n"
            "            }\n"
            "\n"
            "            fprintf(f, \"\\n\");\n"

            "\n"
            "            if(time_new + previous_dt >= final_time) {\n"
            "                if(final_time == time_new) {\n"
            "                    break;\n"
            "                } else if(time_new < final_time) {\n"
            "                    dt = previous_dt = final_time - time_new;\n"
            "                    time_new += previous_dt;\n"
            "                    break;\n"
            "                }\n"
            "            } else {\n"
            "                time_new += previous_dt;\n"
            "            }\n"
            "\n"
            "        }\n"
            "    }\n"
            "\n"
            "    free(_k1__);\n"
            "    free(_k2__);\n"
            "}",  out_header);


    fprintf(file, "\nint main(int argc, char **argv) {\n"
            "\n"
            "\treal *x0 = (real*) malloc(sizeof(real)*NEQ);\n"
            "\n");
    generate_initial_conditions_values(initial, main_body, file, &variables_and_odes_scope, global_scope);
    fprintf(file,"\tset_initial_conditions(x0, values);\n"
            "\n"
            "\tsolve_ode(x0, strtod(argv[1], NULL), argv[2]);\n"
            "\n"
            "\n"
            "\treturn (0);\n"
            "}");
}
void convert_to_c(program prog, FILE *file, solver_type solver) {

    solver = solver;

    program main_body = NULL;
    program functions = NULL;
    program initial = NULL;
    program globals = NULL;
    program imports = NULL;

    int n_stmt = arrlen(prog);

    int ode_count = 0;

    for(int i = 0; i < n_stmt; i++) {
        ast *a = prog[i];
        if(a->tag == ast_function_statement) {
            arrput(functions, a);
        } else if(a->tag == ast_initial_stmt) {
            arrput(initial, a);
        } else if(a->tag == ast_global_stmt) {
            arrput(globals, a);
        } else if(a->tag == ast_import_stmt) {
            arrput(imports, a);
        }
        else {
            if(a->tag == ast_ode_stmt) {
                ode_count++;
            }
            arrput(main_body, a);
        }
    }

    if(ode_count == 0) {
        fprintf(stderr, "Warning - no odes defined\n");
    }

    sds out_header = out_file_header(main_body);

    process_imports(imports, &functions);

    declared_variable_hash variables_and_odes_scope = create_variables_scope(main_body);
    declared_variable_hash global_scope = create_functions_and_global_scope(functions, globals);

    switch (solver) {
        case CVODE_SOLVER:
            write_cvode_solver(file, initial, globals, functions, main_body, out_header, variables_and_odes_scope, global_scope);
            break;
        case EULER_ADPT_SOLVER:
            write_adpt_euler_solver(file, initial, globals, functions, main_body, out_header, variables_and_odes_scope, global_scope);
            break;
        default:
            fprintf(stderr, "Error: invalid solver type!\n");
    }

}
