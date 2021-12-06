#include "file_utils/file_utils.h"
#include "code_converter.h"
#include "stb/stb_ds.h"
#include <assert.h>

static solver_type solver = EULER_ADPT_SOLVER;

static int indentation_level = 0;

struct var_declared_entry_t {
    char *key;
    int value;
};

struct var_declared_entry_t *var_declared = NULL;

static sds ast_to_c(ast *a);

static sds expression_stmt_to_c(ast *a) {

    if (a->expr_stmt != NULL) {
        return ast_to_c(a->expr_stmt);
    }
    return sdsempty();

}

static sds return_stmt_to_c(ast *a) {

    sds buf = sdsempty();

    if(a->return_stmt.return_values != NULL) {
        int n = arrlen(a->return_stmt.return_values);

        if(n == 1) {
            buf = sdscatfmt(buf, "%sreturn ", indent_spaces[indentation_level]);
            sds tmp = ast_to_c(a->return_stmt.return_values[0]);
            buf = sdscat(buf, tmp);
            buf = sdscat(buf, ";");
            sdsfree(tmp);
        } else {
            for(int i = 0; i < n; i++) {
                sds tmp = ast_to_c(a->return_stmt.return_values[i]);
                buf = sdscatfmt(buf, "%s*ret_val_%i = %s;\n", indent_spaces[indentation_level], i, tmp);
                sdsfree(tmp);
            }
        }

    }

    return buf;
}

static sds assignement_stmt_to_c(ast *a) {

    sds buf = sdsempty();
    char *var_type;

    if(a->tag == ast_ode_stmt) {
        sds name = sdscatprintf(sdsempty(), "%.*s", (int)strlen(a->assignement_stmt.name->identifier.value)-1, a->assignement_stmt.name->identifier.value);

        int position = a->assignement_stmt.declaration_position;

        sdsfree(name);
        if(solver == CVODE_SOLVER) {
            sds tmp = ast_to_c(a->assignement_stmt.value);
            buf = sdscatprintf(buf, "%sNV_Ith_S(rDY, %d) = %s;\n", indent_spaces[indentation_level], position-1, tmp);
            sdsfree(tmp);
        }
        else if(solver == EULER_ADPT_SOLVER) {
            sds tmp = ast_to_c(a->assignement_stmt.value);
            buf = sdscatprintf(buf, "%srDY[%d] = %s;\n", indent_spaces[indentation_level], position-1, tmp);
            sdsfree(tmp);
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

        int global = a->assignement_stmt.name->identifier.global;

        if(global) {
            sds tmp = ast_to_c(a->assignement_stmt.value);
            buf = sdscatfmt(buf, "%s%s = %s;\n", indent_spaces[indentation_level], a->assignement_stmt.name->identifier.value, tmp);
            sdsfree(tmp);
        }

        else {
            int declared = shgeti(var_declared, a->assignement_stmt.name->identifier.value) != -1;

            if (!declared) {
                sds tmp = ast_to_c(a->assignement_stmt.value);
                buf = sdscatfmt(buf, "%s%s %s = %s;\n", indent_spaces[indentation_level], var_type, a->assignement_stmt.name->identifier.value, tmp);
                shput(var_declared, a->assignement_stmt.name->identifier.value, 1);
                sdsfree(tmp);
            } else {
                sds tmp = ast_to_c(a->assignement_stmt.value);
                buf = sdscatfmt(buf, "%s%s = %s;\n", indent_spaces[indentation_level], a->assignement_stmt.name->identifier.value, tmp);
                sdsfree(tmp);
            }
        }
    }
    else if (a->tag == ast_grouped_assignment_stmt) {

        ast **variables = a->grouped_assignement_stmt.names;
        int n = arrlen(variables);

        var_type = "real";

        if(n == 1) {

            char *id_name = variables[0]->identifier.value;
            ast  *call_expr = a->grouped_assignement_stmt.call_expr;

            int global = variables[0]->identifier.global;

            if(global) {
                sds tmp = ast_to_c(call_expr);
                buf = sdscatfmt(buf, "%s%s = %s;\n", indent_spaces[indentation_level], id_name, tmp);
                sdsfree(tmp);
            }

            else {
                int declared = shgeti(var_declared, id_name) != -1;

                if (!declared) {
                    sds tmp = ast_to_c(call_expr);
                    buf = sdscatfmt(buf, "%s%s %s = %s;\n", indent_spaces[indentation_level], var_type, id_name, tmp);
                    shput(var_declared, id_name, 1);
                    sdsfree(tmp);
                } else {
                    sds tmp = ast_to_c(call_expr);
                    buf = sdscatfmt(buf, "%s%s = %s;\n", indent_spaces[indentation_level], id_name, tmp);
                    sdsfree(tmp);
                }
            }
        } else {

            for(int j = 0; j < n; j++) {

                ast *id = variables[j];
                int global = id->identifier.global;

                if(!global) {

                    int declared = shgeti(var_declared, id->identifier.value) != -1;

                    if (!declared) {
                        buf = sdscatfmt(buf, "%s%s %s;\n", indent_spaces[indentation_level], var_type, id->identifier.value);
                        shput(var_declared, id->identifier.value, 1);
                    }
                }

            }

            //Converting the function call here
            ast *b = a->grouped_assignement_stmt.call_expr;

            int n_real_args = arrlen(b->call_expr.arguments);

            sds tmp = ast_to_c(b->call_expr.function_identifier);
            buf = sdscatfmt(buf, "%s%s", indent_spaces[indentation_level], tmp);
            sdsfree(tmp);
            buf = sdscat(buf, "(");

            if(n_real_args) {
                tmp = ast_to_c(b->call_expr.arguments[0]);
                buf = sdscat(buf, tmp);
                sdsfree(tmp);

                for(int i = 1; i < n_real_args; i++) {
                    tmp =ast_to_c(b->call_expr.arguments[i]);
                    buf = sdscatfmt(buf, ", %s", tmp);
                    sdsfree(tmp);
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
    return sdsnew(buf);
}

static sds identifier_to_c(ast *a) {
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
    buf = sdscatprintf(buf, "\"%s\"", a->str_literal.value);
    return buf;
}

static sds prefix_expr_to_c(ast *a) {

    sds buf = sdsempty();

    buf = sdscat(buf, "(");
    buf = sdscatfmt(buf, "%s", a->prefix_expr.op);

    sds tmp = ast_to_c(a->prefix_expr.right);
    buf = sdscatfmt(buf, "%s", tmp);
    sdsfree(tmp);
    buf = sdscat(buf, ")");

    return buf;

}

static sds infix_expr_to_c(ast *a) {

    sds buf = sdsempty();
    sds tmp;

    buf = sdscat(buf, "(");
    tmp = ast_to_c(a->infix_expr.left);
    buf = sdscatfmt(buf, "%s", tmp);
    sdsfree(tmp);

    if(strcmp(a->infix_expr.op, "and") == 0) {
        buf = sdscat(buf, "&&");
    }else if(strcmp(a->infix_expr.op, "or") == 0) {
        buf = sdscat(buf, "||");
    } else {
        buf = sdscatfmt(buf, "%s", a->infix_expr.op);
    }

    tmp = ast_to_c(a->infix_expr.right);
    buf = sdscatfmt(buf, "%s", tmp);
    buf = sdscat(buf, ")");
    sdsfree(tmp);

    return buf;

}

static sds if_expr_to_c(ast *a) {

    sds buf = sdsempty();

    sds tmp;

    buf = sdscatfmt(buf, "%sif", indent_spaces[indentation_level]);

    tmp = ast_to_c(a->if_expr.condition);
    buf = sdscatfmt(buf, "%s {\n", tmp);
    sdsfree(tmp);

    indentation_level++;
    int n = arrlen(a->if_expr.consequence);
    for(int i = 0; i < n; i++) {
        tmp = ast_to_c(a->if_expr.consequence[i]);
        buf = sdscatfmt(buf, "%s\n", tmp);
        sdsfree(tmp);
    }
    indentation_level--;

    buf = sdscatfmt(buf, "%s}", indent_spaces[indentation_level]);

    n = arrlen(a->if_expr.alternative);

    if(n) {
        buf = sdscat(buf, " else {\n");
        indentation_level++;
        for(int i = 0; i < n; i++) {
            tmp = ast_to_c(a->if_expr.alternative[i]);
            buf = sdscatfmt(buf, "%s\n", tmp);
            sdsfree(tmp);

        }
        indentation_level--;

        buf = sdscatfmt(buf, "%s}\n", indent_spaces[indentation_level]);

    }
    else if(a->if_expr.elif_alternative) {
        tmp = ast_to_c(a->if_expr.elif_alternative);
        buf = sdscatfmt(buf, " else %s", tmp);
        sdsfree(tmp);
    }

    return buf;

}

static sds while_stmt_to_c(ast *a) {

    sds buf = sdsempty();

    sds tmp;

    buf = sdscat(buf, "while");
    tmp = ast_to_c(a->while_stmt.condition);
    buf = sdscatfmt(buf, "%s ", tmp);
    sdsfree(tmp);

    buf = sdscat(buf, "{");

    int n = arrlen(a->while_stmt.body);
    for(int i = 0; i < n; i++) {
        tmp = ast_to_c(a->while_stmt.body[i]);
        buf = sdscatfmt(buf, "%s\n", tmp);
        sdsfree(tmp);
    }

    buf = sdscat(buf, "}");
    return buf;

}

static sds call_expr_to_c(ast *a) {

    sds buf = sdsempty();
    sds tmp;

    tmp = ast_to_c(a->call_expr.function_identifier);
    buf = sdscat(buf, tmp);
    buf = sdscat(buf, "(");
    sdsfree(tmp);

    int n = arrlen(a->call_expr.arguments);

    if(n) {
        tmp = ast_to_c(a->call_expr.arguments[0]);
        buf = sdscat(buf, tmp);
        sdsfree(tmp);

        for(int i = 1; i < n; i++) {
            tmp = ast_to_c(a->call_expr.arguments[i]);
            buf = sdscatfmt(buf, ", %s", tmp);
            sdsfree(tmp);
        }
    }

    buf = sdscat(buf, ")");

    return buf;
}

static sds global_variable_to_c(ast *a) {

    sds buf = sdsempty();
    sds tmp = ast_to_c(a->assignement_stmt.value);
    buf = sdscatfmt(buf, "real %s = %s;\n", a->assignement_stmt.name->identifier.value, tmp);
    sdsfree(tmp);

    return buf;

}

static sds ast_to_c(ast *a) {

    if(a->tag == ast_assignment_stmt || a->tag == ast_grouped_assignment_stmt || a->tag == ast_ode_stmt) {
        return assignement_stmt_to_c(a);
    }

    if(a->tag == ast_global_stmt) {
        return global_variable_to_c(a);
    }

    if(a->tag == ast_return_stmt) {
        return return_stmt_to_c(a);
    }

    if(a->tag == ast_expression_stmt) {
        return expression_stmt_to_c(a);
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
        return identifier_to_c(a);
    }

    if(a->tag == ast_prefix_expression) {
        return prefix_expr_to_c(a);
    }

    if(a->tag == ast_infix_expression) {
        return infix_expr_to_c(a);
    }

    if(a->tag == ast_if_expr) {
        return if_expr_to_c(a);
    }

    if(a->tag == ast_while_stmt) {
        return while_stmt_to_c(a);
    }

    if(a->tag == ast_call_expression) {
        return call_expr_to_c(a);
    }

    printf("[WARN] Line %d of file %s - to_c not implemented to operator %d\n", a->token.line_number, a->token.file_name, a->tag);

    return NULL;

}

void write_initial_conditions(program p, FILE *file) {

    int n_stmt = arrlen(p);
    for(int i = 0; i < n_stmt; i++) {
        assert(p[i]);

        ast *a = p[i];

        int position = a->assignement_stmt.declaration_position;

        if(solver == CVODE_SOLVER) {
            fprintf(file, "    NV_Ith_S(x0, %d) = values[%d]; //%s\n", position-1, position-1, a->assignement_stmt.name->identifier.value);
        }
        else if(solver == EULER_ADPT_SOLVER) {
            fprintf(file, "    x0[%d] = values[%d]; //%s\n", position-1, position-1, a->assignement_stmt.name->identifier.value);
        }
    }

}

bool generate_initial_conditions_values(program p, program body, FILE *file) {

    int n_stmt = arrlen(p);

    fprintf(file, "    real values[%d];\n", n_stmt);
    int error = false;

    for(int i = 0; i < n_stmt; i++) {
        ast *a = p[i];

        int position = a->assignement_stmt.declaration_position;
        sds tmp = ast_to_c(a->assignement_stmt.value);
        fprintf(file, "    values[%d] = %s; //%s\n", position-1, tmp, a->assignement_stmt.name->identifier.value);
        sdsfree(tmp);
    }

    return error;

}

void write_odes_old_values(program p, FILE *file) {

    int n_stmt = arrlen(p);
    for(int i = 0; i < n_stmt; i++) {

        ast *a = p[i];

        if(a->tag != ast_ode_stmt) continue;

        int position = a->assignement_stmt.declaration_position;

        if (solver == CVODE_SOLVER) {
            fprintf(file, "    const real %.*s =  NV_Ith_S(sv, %d);\n", (int) strlen(a->assignement_stmt.name->identifier.value) - 1,
                          a->assignement_stmt.name->identifier.value, position-1);
        } else if (solver == EULER_ADPT_SOLVER) {
            fprintf(file, "    const real %.*s =  sv[%d];\n", (int) strlen(a->assignement_stmt.name->identifier.value) - 1,
                          a->assignement_stmt.name->identifier.value, position-1);
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

void write_variables_or_body(program p, FILE *file) {
    int n_stmt = arrlen(p);
    for(int i = 0; i < n_stmt; i++) {
        ast *a = p[i];
        if(a->tag == ast_ode_stmt) {
            sds name = sdscatprintf(sdsempty(), "%.*s", (int)strlen(a->assignement_stmt.name->identifier.value)-1, a->assignement_stmt.name->identifier.value);
            int position = a->assignement_stmt.declaration_position;
            sdsfree(name);

            if(solver == CVODE_SOLVER) {
                sds tmp = ast_to_c(a->assignement_stmt.value);
                fprintf(file, "    NV_Ith_S(rDY, %d) = %s;\n", position-1, tmp);
                sdsfree(tmp);
            }
            else if(solver == EULER_ADPT_SOLVER) {
                sds tmp = ast_to_c(a->assignement_stmt.value);
                fprintf(file, "    rDY[%d] = %s;\n", position-1, tmp);
                sdsfree(tmp);
            }
        }
        else {
            sds buf = ast_to_c(a);
            fprintf(file, "%s", buf);
            sdsfree(buf);
        }
    }
}

void write_functions(program p, FILE *file) {

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

        //declared_variable_hash declared_variables_in_parameters_list = NULL;
        //sh_new_arena(declared_variables_in_parameters_list);
        //shdefault(declared_variables_in_parameters_list, 0);

        if(n) {
          //  shput(declared_variables_in_parameters_list, a->function_stmt.parameters[0]->identifier.value, 1);
            sds tmp = ast_to_c(a->function_stmt.parameters[0]);
            fprintf(file, "real %s", tmp);
            sdsfree(tmp);
            for(int j = 1; j < n; j++) {
            //    shput(declared_variables_in_parameters_list, a->function_stmt.parameters[j]->identifier.value, 1);
                tmp = ast_to_c(a->function_stmt.parameters[j]);
                fprintf(file, ", real %s", tmp);
                sdsfree(tmp);
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
            sds tmp = ast_to_c(a->function_stmt.body[j]);
            fprintf(file, "%s\n", tmp);
            sdsfree(tmp);
        }
        indentation_level--;
        fprintf(file, "}\n\n");

        //shfree(declared_variables_in_parameters_list);
    }
}

bool write_cvode_solver(FILE *file, program initial, program globals, program functions, program main_body, sds out_header) {

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

    write_variables_or_body(globals, file);
    fprintf(file, "\n");

    write_functions(functions, file);

    fprintf(file, "void set_initial_conditions(N_Vector x0, real *values) { \n\n");
    write_initial_conditions(initial, file);
    fprintf(file, "\n}\n\n");

    // RHS CPU
    fprintf(file, "static int solve_model(realtype time, N_Vector sv, N_Vector rDY, void *f_data) {\n\n");

    fprintf(file, "    //State variables\n");
    write_odes_old_values(main_body, file);
    fprintf(file, "\n");

    fprintf(file, "    //Parameters\n");

    indentation_level++;
    write_variables_or_body(main_body, file);
    indentation_level--;

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


    bool error;

    fprintf(file, "\nint main(int argc, char **argv) {\n"
            "\n"
            "\tN_Vector x0 = N_VNew_Serial(NEQ);\n"
            "\n");
    error = generate_initial_conditions_values(initial, main_body, file);
    fprintf(file, "\tset_initial_conditions(x0, values);\n"
            "\n"
            "\tsolve_ode(x0, strtod(argv[1], NULL), argv[2]);\n"
            "\n"
            "\n"
            "\treturn (0);\n"
            "}");

    return error;
}

bool write_adpt_euler_solver(FILE *file, program initial, program globals, program functions, program main_body, sds out_header) {

    fprintf(file,"#include <math.h>\n"
            "#include <stdbool.h>\n"
            "#include <stdio.h>\n"
            "#include <stdlib.h>\n"
            "#include <float.h>\n"
            "#include <string.h>\n"
            " \n\n");

    fprintf(file, "#define NEQ %d\n", (int)arrlen(initial));
    fprintf(file, "typedef double real;\n");

    write_variables_or_body(globals, file);
    fprintf(file, "\n");

    write_functions(functions, file);

    fprintf(file, "void set_initial_conditions(real *x0, real *values) { \n\n");
    write_initial_conditions(initial, file);
    fprintf(file, "\n}\n\n");

    // RHS CPU
    fprintf(file, "static int solve_model(real time, real *sv, real *rDY) {\n\n");

    fprintf(file, "    //State variables\n");
    write_odes_old_values(main_body, file);
    fprintf(file, "\n");

    fprintf(file, "    //Parameters\n");

    indentation_level++;
    write_variables_or_body(main_body, file);
    indentation_level--;

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
            "    fprintf(f, %s);\n\n"
            "    real min[NEQ];\n"
            "    real max[NEQ];\n\n"
            "    for(int i = 0; i < NEQ; i++) {\n"
            "       min[i] = DBL_MAX;\n"
            "       max[i] = DBL_MIN;\n"
            "    }\n\n"
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
            "                if(sv[i] < min[i]) min[i] = sv[i];\n"
            "                if(sv[i] > max[i]) max[i] = sv[i];\n"
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
            "    char *min_max = malloc(strlen(file_name) + 9);\n"

            "    sprintf(min_max, \"%%s_min_max\", file_name);\n"
            "    FILE* min_max_file = fopen(min_max, \"w\");\n"
            "    for(int i = 0; i < NEQ; i++) {\n"
            "        fprintf(min_max_file, \"%%lf;%%lf\\n\", min[i], max[i]);\n"
            "    }\n"
            "    fclose(min_max_file);\n"
            "    free(min_max);\n"
            "    \n"
            "    free(_k1__);\n"
            "    free(_k2__);\n"
            "}",  out_header);


    fprintf(file, "\nint main(int argc, char **argv) {\n"
            "\n"
            "\treal *x0 = (real*) malloc(sizeof(real)*NEQ);\n"
            "\n");

    bool error = generate_initial_conditions_values(initial, main_body, file);
    fprintf(file,"\tset_initial_conditions(x0, values);\n"
            "\n"
            "\tsolve_ode(x0, strtod(argv[1], NULL), argv[2]);\n"
            "\n"
            "\n"
            "\treturn (0);\n"
            "}");

    return error;
}

bool convert_to_c(program prog, FILE *file, solver_type p_solver) {

    solver = p_solver;

    program main_body = NULL;
    program functions = NULL;
    program initial = NULL;
    program globals = NULL;
    program imports = NULL;

    int n_stmt = arrlen(prog);

    int ode_count = 0;

    bool error = false;

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

    sh_new_arena(var_declared);

    sds out_header = out_file_header(main_body);

    switch (solver) {
        case CVODE_SOLVER:
            error = write_cvode_solver(file, initial, globals, functions, main_body, out_header);
            break;
        case EULER_ADPT_SOLVER:
            error = write_adpt_euler_solver(file, initial, globals, functions, main_body, out_header);
            break;
        default:
            fprintf(stderr, "Error: invalid solver type!\n");
    }

    sdsfree(out_header);

    shfree(var_declared);
    arrfree(main_body);
    arrfree(functions);
    arrfree(initial);
    arrfree(globals);
    arrfree(imports);

    return error;

}
