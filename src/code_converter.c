#include "file_utils/file_utils.h"
#include "code_converter.h"
#include "stb/stb_ds.h"

#define NO_SPACES  ""
#define _4SPACES   "    "
#define _8SPACES  _4SPACES _4SPACES
#define _12SPACES _8SPACES _4SPACES
#define _16SPACES _12SPACES _4SPACES
#define _20SPACES _16SPACES _4SPACES
#define _24SPACES _20SPACES _4SPACES
#define _28SPACES _24SPACES _4SPACES

int indentation_level = 0;
char *indent_spaces[] = {NO_SPACES, _4SPACES, _8SPACES, _12SPACES, _16SPACES, _20SPACES, _24SPACES, _28SPACES};

static sds ast_to_c(ast *a, declared_variable_hash declared_variables_in_scope);

static sds expression_stmt_to_c(ast *a, declared_variable_hash declared_variables_in_scope) {

	if (a->expr_stmt != NULL) {
		return ast_to_c(a->expr_stmt, declared_variables_in_scope);
	}
	return sdsempty();
}

static sds return_stmt_to_c(ast *a, declared_variable_hash declared_variables_in_scope) {

	sds buf = sdsempty();

	buf = sdscatfmt(buf, "%s%s ", indent_spaces[indentation_level], token_literal(a));

	if(a->return_stmt.return_value != NULL) {
		buf = sdscat(buf, ast_to_c(a->return_stmt.return_value, declared_variables_in_scope));
	}

	buf = sdscat(buf, ";");
	return buf;
}

static sds assignement_stmt_to_c(ast *a, declared_variable_hash declared_variables_in_scope) {

	sds buf = sdsempty();

	buf = sdscatfmt(buf, "%s ", token_literal(a));
	buf = sdscatfmt(buf, "%s", a->assignement_stmt.name->identifier.value);
	buf = sdscat(buf, " = ");


	if(a->assignement_stmt.value != NULL) {
		buf = sdscat(buf, ast_to_c(a->assignement_stmt.value, declared_variables_in_scope));
	}

	return buf;

}

static char* number_literal_to_c(ast *a) {
	char buf[128];
	sprintf(buf, "%e", a->num_literal.value);
	return strdup(buf);
}

static sds identifier_to_c(ast *a, declared_variable_hash declared_variables_in_scope) {
    int declared = shget(declared_variables_in_scope, a->identifier.value);

    if(!declared) {
        fprintf(stderr, "Error on line %d of file %s. Identifier %s not declared, assign a value to it before using!\n", a->token.line_number, a->token.file_name, a->identifier.value);
    }

    sds buf = sdsempty();
    sdscatprintf(buf, "%s", a->identifier.value);
    return buf;
}

static sds boolean_literal_to_c(ast *a) {
    sds buf = sdsempty();
    sdscatprintf(buf, "%s", a->token.literal);
    return buf;
}

static sds string_literal_to_c(ast *a) {
    sds buf = sdsempty();
    sdscatprintf(buf, "\"%s\"", a->token.literal);
    return buf;
}

static sds prefix_expr_to_c(ast *a, declared_variable_hash declared_variables_in_scope) {

	sds buf = sdsempty();

	buf = sdscat(buf, "(");
    buf = sdscatfmt(buf, "%s", a->prefix_expr.op);

	buf = sdscatfmt(buf, "%s", ast_to_c(a->prefix_expr.right, declared_variables_in_scope));
	buf = sdscat(buf, ")");

	return buf;

}

static sds infix_expr_to_c(ast *a,  declared_variable_hash declared_variables_in_scope) {

	sds buf = sdsempty();

	buf = sdscat(buf, "(");
	buf = sdscatfmt(buf, "%s", ast_to_c(a->infix_expr.left,  declared_variables_in_scope));

    if(strcmp(a->infix_expr.op, "and") == 0) {
        buf = sdscat(buf, "&&");
    }else if(strcmp(a->infix_expr.op, "or") == 0) {
        buf = sdscat(buf, "||");
    } else {
        buf = sdscatfmt(buf, "%s", a->infix_expr.op);
    }

	buf = sdscatfmt(buf, "%s", ast_to_c(a->infix_expr.right, declared_variables_in_scope));
	buf = sdscat(buf, ")");

	return buf;

}

static sds if_expr_to_c(ast *a, declared_variable_hash declared_variables_in_scope) {

    sds buf = sdsempty();

    buf = sdscatfmt(buf, "%sif", indent_spaces[indentation_level]);
    buf = sdscatfmt(buf, "%s {\n", ast_to_c(a->if_expr.condition, declared_variables_in_scope));

    indentation_level++;
    int n = arrlen(a->if_expr.consequence);
    for(int i = 0; i < n; i++) {
        buf = sdscatfmt(buf, "%s\n", ast_to_c(a->if_expr.consequence[i], declared_variables_in_scope));
    }
    indentation_level--;

    buf = sdscatfmt(buf, "%s}", indent_spaces[indentation_level]);

    n = arrlen(a->if_expr.alternative);

    if(n) {
        buf = sdscat(buf, " else {\n");
        indentation_level++;
        for(int i = 0; i < n; i++) {
            buf = sdscatfmt(buf, "%s\n",  ast_to_c(a->if_expr.alternative[i], declared_variables_in_scope));

        }
        indentation_level--;

        buf = sdscatfmt(buf, "%s}", indent_spaces[indentation_level]);

    }

    return buf;

}

static sds while_stmt_to_c(ast *a, declared_variable_hash declared_variables_in_scope) {

    sds buf = sdsempty();

    buf = sdscat(buf, "while");
    buf = sdscatfmt(buf, "%s ", ast_to_c(a->while_stmt.condition, declared_variables_in_scope));

    buf = sdscat(buf, "{");

    int n = arrlen(a->while_stmt.body);
    for(int i = 0; i < n; i++) {
        buf = sdscatfmt(buf, "%s\n", ast_to_c(a->while_stmt.body[i],  declared_variables_in_scope));
    }

    buf = sdscat(buf, "}");
    return buf;

}

static sds call_expr_to_c(ast *a, declared_variable_hash declared_variables_in_scope) {

    sds buf = sdsempty();

    buf = sdscat(buf, ast_to_c(a->call_expr.function, declared_variables_in_scope));
    buf = sdscat(buf, "(");

    int n = arrlen(a->call_expr.arguments);

    if(n) {
        buf = sdscat(buf, ast_to_c(a->call_expr.arguments[0], declared_variables_in_scope));

        for(int i = 1; i < n; i++) {
            buf = sdscatfmt(buf, ", %s", ast_to_c(a->call_expr.arguments[i], declared_variables_in_scope));
        }
    }

    buf = sdscat(buf, ")");

    return buf;
}

static sds ast_to_c(ast *a, declared_variable_hash declared_variables_in_scope) {

	if(a->tag == ast_ode_stmt || a->tag == ast_initial_stmt || a->tag == ast_global_stmt) {
		return assignement_stmt_to_c(a, declared_variables_in_scope);
	}

	if(a->tag == ast_return_stmt) {
		return return_stmt_to_c(a, declared_variables_in_scope);
	}

	if(a->tag == ast_expression_stmt) {
		return expression_stmt_to_c(a, declared_variables_in_scope);
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
        return identifier_to_c(a, declared_variables_in_scope);
    }

	if(a->tag == ast_prefix_expression) {
		return prefix_expr_to_c(a, declared_variables_in_scope);
	}

	if(a->tag == ast_infix_expression) {
		return infix_expr_to_c(a,  declared_variables_in_scope);
	}

    if(a->tag == ast_if_expr) {
        return if_expr_to_c(a, declared_variables_in_scope);
    }

    if(a->tag == ast_while_stmt) {
        return while_stmt_to_c(a, declared_variables_in_scope);
    }

    if(a->tag == ast_call_expression) {
        return call_expr_to_c(a, declared_variables_in_scope);
    }

    printf("[WARN] - to_c not implemented to operator %d\n", a->token.type);

	return NULL;

}

void write_initial_conditions(program p, FILE *file, declared_variable_hash declared_variables_in_scope) {

	int n_stmt = arrlen(p);
	for(int i = 0; i < n_stmt; i++) {
		ast *a = p[i];
		fprintf(file, "    NV_Ith_S(x0, %d) = %s; //%s\n", i, ast_to_c(a->assignement_stmt.value, declared_variables_in_scope), a->assignement_stmt.name->identifier.value);
	}
}

void write_odes_old_values(program p, FILE *file) {

	int n_stmt = arrlen(p);
	for(int i = 0; i < n_stmt; i++) {
		ast *a = p[i];
		fprintf(file, "    const real %.*s =  NV_Ith_S(sv, %d);\n", (int)strlen(a->assignement_stmt.name->identifier.value)-1, a->assignement_stmt.name->identifier.value, i);
	}

}

sds out_file_header(program p) {


    sds ret = sdsempty();

	int n_stmt = arrlen(p);

	ret = sdscatprintf(ret, "\"#t");

    ast *a ;
	for(int i = 0; i < n_stmt; i++) {
		a = p[i];
	    ret = sdscatprintf(ret, ", %.*s", (int)strlen(a->assignement_stmt.name->identifier.value)-1, a->assignement_stmt.name->identifier.value);
	}

    ret = sdscat(ret, "\\n\"");

    return ret;
}


void write_variables(program p, FILE *file, declared_variable_hash *declared_variables) {

	int n_stmt = arrlen(p);
	for(int i = 0; i < n_stmt; i++) {
		char *var_type;
		ast *a = p[i];

		if(a->assignement_stmt.value->tag == ast_boolean_literal || a->assignement_stmt.value->tag == ast_if_expr ) {
			var_type = "bool";
		}
		else if (a->assignement_stmt.value->tag == ast_string_literal) {
			var_type = "char *";
		}
		else {
			var_type = "real";
		}

        int declared = shget(*declared_variables, a->assignement_stmt.name->identifier.value);

        if(!declared) {
		    fprintf(file, "%s%s %s = %s;\n", indent_spaces[indentation_level], var_type, a->assignement_stmt.name->identifier.value,  ast_to_c(a->assignement_stmt.value, *declared_variables));
            shput(*declared_variables, a->assignement_stmt.name->identifier.value, 1);
        }
        else {
            fprintf(file, "%s%s = %s;\n", indent_spaces[indentation_level], a->assignement_stmt.name->identifier.value,  ast_to_c(a->assignement_stmt.value, *declared_variables));
        }

	}

}

void write_odes(program p, FILE *file, declared_variable_hash declared_variables_in_scope) {

	int n_stmt = arrlen(p);
	for(int i = 0; i < n_stmt; i++) {
		ast *a = p[i];
		fprintf(file, "    NV_Ith_S(rDY, %d) = %s;\n",  i, ast_to_c(a->assignement_stmt.value,  declared_variables_in_scope));

	}

}

void write_functions(program p, FILE *file, declared_variable_hash global_variables) {

	int n_stmt = arrlen(p);
	for(int i = 0; i < n_stmt; i++) {

		ast *a = p[i];

		fprintf(file, "real %s", a->function_stmt.name->identifier.value);
		fprintf(file, "(");

		int n = arrlen(a->function_stmt.parameters);

        declared_variable_hash declared_variables_in_parameters_list = NULL;
        shdefault(declared_variables_in_parameters_list, 0);
        sh_new_arena(declared_variables_in_parameters_list);

		if(n) {
            shput(declared_variables_in_parameters_list, a->function_stmt.parameters[0]->identifier.value, 1);
			fprintf(file, "real %s", ast_to_c(a->function_stmt.parameters[0], declared_variables_in_parameters_list));
			for(int j = 1; j < n; j++) {
                shput(declared_variables_in_parameters_list, a->function_stmt.parameters[0]->identifier.value, 1);
				fprintf(file, ", real %s", ast_to_c(a->function_stmt.parameters[j], declared_variables_in_parameters_list));
			}
		}

		fprintf(file, ") {\n");

        n = arrlen(a->function_stmt.body);
		indentation_level++;
		for(int j = 0; j < n; j++) {
			fprintf(file, "%s\n", ast_to_c(a->function_stmt.body[j],  declared_variables_in_parameters_list));
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

declared_variable_hash create_scope(ast **odes, ast **functions) {
	declared_variable_hash declared_variables = NULL;
    shdefault(declared_variables, 0);
    sh_new_arena(declared_variables);

	//variable time is auto declared in the scope
	shput(declared_variables, "time", 1);

	//built in functions
	shput(declared_variables, "acos", 1);
	shput(declared_variables, "asin", 1);
	shput(declared_variables, "atan", 1);
	shput(declared_variables, "atan2", 1);
	shput(declared_variables, "ceil", 1);
	shput(declared_variables, "cos", 1);
	shput(declared_variables, "cosh", 1);
	shput(declared_variables, "exp", 1);
	shput(declared_variables, "fabs", 1);
	shput(declared_variables, "floor", 1);
	shput(declared_variables, "fmod", 1);
	shput(declared_variables, "frexp", 1);
	shput(declared_variables, "ldexp", 1);
	shput(declared_variables, "log   ", 1);
	shput(declared_variables, "log10", 1);
	shput(declared_variables, "modf", 1);
	shput(declared_variables, "pow", 1);
	shput(declared_variables, "sin", 1);
	shput(declared_variables, "sinh", 1);
	shput(declared_variables, "sqrt", 1);
	shput(declared_variables, "tan", 1);
	shput(declared_variables, "tanh", 1);
	shput(declared_variables, "cosh", 1);
	shput(declared_variables, "asinh", 1);
	shput(declared_variables, "atanh", 1);
	shput(declared_variables, "cbrt", 1);
	shput(declared_variables, "copysign", 1);
	shput(declared_variables, "erf	   ", 1);
	shput(declared_variables, "erfc", 1);
	shput(declared_variables, "exp2", 1);
	shput(declared_variables, "expm1", 1);
	shput(declared_variables, "fdim", 1);
	shput(declared_variables, "fma", 1);
	shput(declared_variables, "fmax", 1);
	shput(declared_variables, "fmin", 1);
	shput(declared_variables, "hypot", 1);
	shput(declared_variables, "ilogb", 1);
	shput(declared_variables, "lgamma", 1);
	shput(declared_variables, "llrint", 1);
	shput(declared_variables, "lrint	", 1);
	shput(declared_variables, "llround", 1);
	shput(declared_variables, "lround", 1);
	shput(declared_variables, "log1p", 1);
	shput(declared_variables, "log2	", 1);
	shput(declared_variables, "logb	", 1);
	shput(declared_variables, "nan", 1);
	shput(declared_variables, "nearbyint", 1);
	shput(declared_variables, "nextafter", 1);
	shput(declared_variables, "nexttoward", 1);
	shput(declared_variables, "remainder", 1);
	shput(declared_variables, "remquo", 1);
	shput(declared_variables, "rint", 1);
	shput(declared_variables, "round", 1);
	shput(declared_variables, "scalbln", 1);
	shput(declared_variables, "scalbn", 1);
	shput(declared_variables, "tgamma", 1);
	shput(declared_variables, "trunc", 1);

	//Adding ODES to scope.
	for(int i = 0; i < arrlen(odes); i++) {
		char *tmp = strndup(odes[i]->assignement_stmt.name->identifier.value, (int)strlen(odes[i]->assignement_stmt.name->identifier.value)-1);
		shput(declared_variables, tmp, 1);
	}

	//Ading function identifiers to scope.
	for(int i = 0; i < arrlen(functions); i++) {
		shput(declared_variables, functions[i]->function_stmt.name->identifier.value, 1);
	}

	return declared_variables;
}


void convert_to_c(program p, FILE *file) {

	ast **odes = NULL;
   	ast	**variables = NULL;
	ast **functions = NULL;
	ast **initial = NULL;
    ast **globals = NULL;
    ast **imports = NULL;

	int n_stmt = arrlen(p);

	for(int i = 0; i < n_stmt; i++) {
        ast *a = p[i];
		if(a->tag == ast_assignment_stmt) {
			arrput(variables, a);
		} else if(a->tag == ast_function_statement) {
			arrput(functions, a);
		} else if(a->tag == ast_ode_stmt) {
			arrput(odes, a);
		} else if(a->tag == ast_initial_stmt) {
			arrput(initial, a);
		} else if(a->tag == ast_global_stmt) {
            arrput(globals, a);
        } else if(a->tag == ast_import_stmt) {
            arrput(imports, a);
        }
	}

    if(arrlen(odes) == 0) {
        fprintf(stderr, "Error - no odes defined\n");
    }

    sds out_header = out_file_header(odes);

    process_imports(imports, &functions);

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


	declared_variable_hash declared_variables = create_scope(odes, functions);

    write_variables(globals, file, &declared_variables);
    fprintf(file, "\n");

	write_functions(functions, file, declared_variables);

	fprintf(file, "void set_initial_conditions(N_Vector x0) { \n\n");
	write_initial_conditions(initial, file, declared_variables);
	fprintf(file, "\n}\n\n");

	// RHS CPU
	fprintf(file, "static int solve_model(realtype time, N_Vector sv, N_Vector rDY, void *f_data) {\n\n");

	fprintf(file, "    //State variables\n");
	write_odes_old_values(odes, file);
	fprintf(file, "\n");

	fprintf(file, "    //Parameters\n");

    indentation_level++;
    write_variables(variables, file, &declared_variables);
    indentation_level--;

    write_odes(odes, file, declared_variables);

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
			"\n"
			"\tset_initial_conditions(x0);\n"
			"\n"
			"\tsolve_ode(x0, strtod(argv[1], NULL), argv[2]);\n"
			"\n"
			"\n"
			"\treturn (0);\n"
			"}");
}
