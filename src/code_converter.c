#include "code_converter.h"
#include "stb/stb_ds.h"
#include <assert.h>
#include <sys/types.h>

#define COMMON_INCLUDES "#include <math.h>\n"    \
                        "#include <stdbool.h>\n" \
                        "#include <stdint.h>\n"  \
                        "#include <stdio.h>\n"   \
                        "#include <stdlib.h>\n"  \
                        "#include <float.h>\n"   \
                        "#include <string.h>\n"

#define WRITE_NEQ fprintf(file, "#define NEQ %d\n", (int) arrlen(initial));

static solver_type solver                 = EULER_ADPT_SOLVER;

struct var_declared_entry_t *var_declared = NULL;
struct var_declared_entry_t *ode_position = NULL;

static sds ast_to_c(ast *a, unsigned int *indentation_level);

extern char *indent_spaces[];

static sds expression_stmt_to_c(ast *a, unsigned int *indentation_level) {
    if(a->expr_stmt != NULL) {
        return ast_to_c(a->expr_stmt, indentation_level);
    }
    return sdsempty();
}

static sds return_stmt_to_c(ast *a, unsigned int *indentation_level) {
    sds buf = sdsempty();

    if(a->return_stmt.return_values != NULL) {
        int n = arrlen(a->return_stmt.return_values);

        if(n == 1) {
            sds tmp = ast_to_c(a->return_stmt.return_values[0], indentation_level);
            buf     = sdscatprintf(buf, "%sreturn %s;", indent_spaces[*indentation_level], tmp);
            sdsfree(tmp);
        } else {
            for(int i = 0; i < n; i++) {
                sds tmp = ast_to_c(a->return_stmt.return_values[i], indentation_level);
                buf     = sdscatfmt(buf, "%s*ret_val_%i = %s;\n", indent_spaces[*indentation_level], i, tmp);
                sdsfree(tmp);
            }
        }
    }

    return buf;
}

static sds assignment_stmt_to_c(ast *a, unsigned int *indentation_level) {
    sds buf = sdsempty();
    char *var_type;

    if(a->tag == ast_assignment_stmt || a->tag == ast_ode_stmt) {
        if(a->assignment_stmt.value->tag == ast_boolean_literal || a->assignment_stmt.value->tag == ast_if_expr) {
            var_type = "bool";
        } else if(a->assignment_stmt.value->tag == ast_string_literal) {
            var_type = "char *";
        } else {
            var_type = "real";
        }

        int global = a->assignment_stmt.name->identifier.global;

        if(global) {
            sds tmp = ast_to_c(a->assignment_stmt.value, indentation_level);
            buf     = sdscatfmt(buf, "%s%s = %s", indent_spaces[*indentation_level], a->assignment_stmt.name->identifier.value, tmp);
            sdsfree(tmp);
        } else {
            int declared        = shgeti(var_declared, a->assignment_stmt.name->identifier.value) != -1;
            bool has_ode_symbol = (a->assignment_stmt.name->identifier.value[strlen(a->assignment_stmt.name->identifier.value) - 1] == '\'');

            if(has_ode_symbol) {
                uint32_t position = a->assignment_stmt.declaration_position;
                //TODO: remove the global solver dependency for this function
                if(solver == CVODE_SOLVER) {
                    sds tmp = ast_to_c(a->assignment_stmt.value, indentation_level);
                    buf     = sdscatprintf(buf, "%sNV_Ith_S(rDY, %d) = %s;", indent_spaces[*indentation_level], position - 1, tmp);
                    sdsfree(tmp);
                } else if(solver == EULER_ADPT_SOLVER) {
                    sds tmp = ast_to_c(a->assignment_stmt.value, indentation_level);
                    buf     = sdscatprintf(buf, "%srDY[%d] = %s;", indent_spaces[*indentation_level], position - 1, tmp);
                    sdsfree(tmp);
                }
            } else {
                if(!declared) {
                    sds tmp = ast_to_c(a->assignment_stmt.value, indentation_level);
                    buf     = sdscatfmt(buf, "%s%s %s = %s;", indent_spaces[*indentation_level], var_type, a->assignment_stmt.name->identifier.value, tmp);
                    if(a->assignment_stmt.unit != NULL) {
                        buf = sdscatfmt(buf, " //%s", a->assignment_stmt.unit);
                    }
                    shput(var_declared, a->assignment_stmt.name->identifier.value, 1);
                    sdsfree(tmp);
                } else {
                    sds tmp = ast_to_c(a->assignment_stmt.value, indentation_level);
                    buf     = sdscatfmt(buf, "%s%s = %s;", indent_spaces[*indentation_level], a->assignment_stmt.name->identifier.value, tmp);
                    sdsfree(tmp);
                }
            }
        }
    } else if(a->tag == ast_grouped_assignment_stmt) {

        ast **variables = a->grouped_assignment_stmt.names;
        int n           = arrlen(variables);

        var_type        = "real";

        if(n == 1) {

            char *id_name  = variables[0]->identifier.value;
            ast *call_expr = a->grouped_assignment_stmt.call_expr;

            int global     = variables[0]->identifier.global;

            if(global) {
                sds tmp = ast_to_c(call_expr, indentation_level);
                buf     = sdscatfmt(buf, "%s%s = %s;\n", indent_spaces[*indentation_level], id_name, tmp);
                sdsfree(tmp);
            } else {
                int declared = shgeti(var_declared, id_name) != -1;

                if(!declared) {
                    sds tmp = ast_to_c(call_expr, indentation_level);
                    buf     = sdscatfmt(buf, "%s%s %s = %s;\n", indent_spaces[*indentation_level], var_type, id_name, tmp);
                    if(a->assignment_stmt.unit != NULL) {
                        tmp = sdscatfmt(tmp, " //%s", a->assignment_stmt.unit);
                    }

                    shput(var_declared, id_name, 1);
                    sdsfree(tmp);
                } else {
                    sds tmp = ast_to_c(call_expr, indentation_level);
                    buf     = sdscatfmt(buf, "%s%s = %s;\n", indent_spaces[*indentation_level], id_name, tmp);
                    sdsfree(tmp);
                }
            }
        } else {

            for(int j = 0; j < n; j++) {

                ast *id    = variables[j];
                int global = id->identifier.global;

                if(!global) {

                    int declared = shgeti(var_declared, id->identifier.value) != -1;

                    if(!declared) {
                        buf = sdscatfmt(buf, "%s%s %s;\n", indent_spaces[*indentation_level], var_type, id->identifier.value);
                        shput(var_declared, id->identifier.value, 1);
                    }
                }
            }

            //Converting the function call here
            ast *b          = a->grouped_assignment_stmt.call_expr;

            int n_real_args = arrlen(b->call_expr.arguments);

            sds tmp         = ast_to_c(b->call_expr.function_identifier, indentation_level);
            buf             = sdscatfmt(buf, "%s%s", indent_spaces[*indentation_level], tmp);
            sdsfree(tmp);
            buf = sdscat(buf, "(");

            if(n_real_args) {
                tmp = ast_to_c(b->call_expr.arguments[0], indentation_level);
                buf = sdscat(buf, tmp);
                sdsfree(tmp);

                for(int i = 1; i < n_real_args; i++) {
                    tmp = ast_to_c(b->call_expr.arguments[i], indentation_level);
                    buf = sdscatfmt(buf, ", %s", tmp);
                    sdsfree(tmp);
                }

                for(int i = 0; i < n; i++) {
                    ast *id = variables[i];
                    buf     = sdscatfmt(buf, ", &%s", id->identifier.value);
                }

            } else {

                buf = sdscatfmt(buf, "&%s", variables[0]->identifier.value);

                for(int i = 1; i < n; i++) {
                    ast *id = variables[i];
                    buf     = sdscatfmt(buf, ", &%s", id->identifier.value);
                }
            }

            buf = sdscat(buf, ");");
        }
    }

    return buf;
}

static sds number_literal_to_c(ast *a) {
    sds buf = sdsempty();
    buf     = sdscatprintf(buf, "%e", a->num_literal.value);
    return buf;
}

static sds identifier_to_c(ast *a) {
    sds buf = sdsempty();
    buf     = sdscat(buf, a->identifier.value);
    return buf;
}

static sds boolean_literal_to_c(ast *a) {
    sds buf = sdsempty();
    buf     = sdscatprintf(buf, "%s", a->token.literal);
    return buf;
}

static sds string_literal_to_c(ast *a) {
    sds buf = sdsempty();
    buf     = sdscatprintf(buf, "\"%s\"", a->str_literal.value);
    return buf;
}

static sds prefix_expr_to_c(ast *a, unsigned int *indentation_level) {

    sds buf = sdsempty();

    buf     = sdscat(buf, "(");
    buf     = sdscatfmt(buf, "%s", a->prefix_expr.op);

    sds tmp = ast_to_c(a->prefix_expr.right, indentation_level);
    buf     = sdscatfmt(buf, "%s", tmp);
    sdsfree(tmp);
    buf = sdscat(buf, ")");

    return buf;
}

static sds infix_expr_to_c(ast *a, unsigned int *indentation_level) {

    sds buf = sdsempty();
    sds tmp;

    buf = sdscat(buf, "(");
    tmp = ast_to_c(a->infix_expr.left, indentation_level);
    buf = sdscatfmt(buf, "%s", tmp);
    sdsfree(tmp);

    if(strcmp(a->infix_expr.op, "and") == 0) {
        buf = sdscat(buf, "&&");
    } else if(strcmp(a->infix_expr.op, "or") == 0) {
        buf = sdscat(buf, "||");
    } else {
        buf = sdscatfmt(buf, "%s", a->infix_expr.op);
    }

    tmp = ast_to_c(a->infix_expr.right, indentation_level);
    buf = sdscatfmt(buf, "%s", tmp);
    buf = sdscat(buf, ")");
    sdsfree(tmp);

    return buf;
}

static sds if_expr_to_c(ast *a, unsigned int *indentation_level) {

    sds buf = sdsempty();

    sds tmp;

    buf = sdscatfmt(buf, "%sif", indent_spaces[*indentation_level]);

    tmp = ast_to_c(a->if_expr.condition, indentation_level);
    buf = sdscatfmt(buf, "%s {\n", tmp);
    sdsfree(tmp);

    (*indentation_level)++;
    int n = arrlen(a->if_expr.consequence);
    for(int i = 0; i < n; i++) {
        tmp = ast_to_c(a->if_expr.consequence[i], indentation_level);
        buf = sdscatfmt(buf, "%s\n", tmp);
        sdsfree(tmp);
    }
    (*indentation_level)--;

    buf = sdscatfmt(buf, "%s}", indent_spaces[*indentation_level]);

    n   = arrlen(a->if_expr.alternative);

    if(n) {
        buf = sdscat(buf, " else {\n");
        (*indentation_level)++;
        for(int i = 0; i < n; i++) {
            tmp = ast_to_c(a->if_expr.alternative[i], indentation_level);
            buf = sdscatfmt(buf, "%s\n", tmp);
            sdsfree(tmp);
        }
        (*indentation_level)--;

        buf = sdscatfmt(buf, "%s}\n", indent_spaces[*indentation_level]);

    } else if(a->if_expr.elif_alternative) {
        tmp = ast_to_c(a->if_expr.elif_alternative, indentation_level);
        buf = sdscatfmt(buf, " else %s", tmp);
        sdsfree(tmp);
    } else {
        buf = sdscatfmt(buf, "\n");
    }

    return buf;
}

static sds while_stmt_to_c(ast *a, unsigned int *indentation_level) {

    sds buf = sdsempty();

    sds tmp;

    buf = sdscatfmt(buf, "%swhile", indent_spaces[*indentation_level]);
    tmp = ast_to_c(a->while_stmt.condition, indentation_level);
    buf = sdscatfmt(buf, "(%s) {\n", tmp);
    sdsfree(tmp);

    int n = arrlen(a->while_stmt.body);
    (*indentation_level)++;
    for(int i = 0; i < n; i++) {
        tmp = ast_to_c(a->while_stmt.body[i], indentation_level);
        buf = sdscatfmt(buf, "%s%s\n", tmp, indent_spaces[*indentation_level]);
        sdsfree(tmp);
    }

    (*indentation_level)--;
    buf = sdscatfmt(buf, "%s}", indent_spaces[*indentation_level]);
    return buf;
}

static sds call_expr_to_c(ast *a, unsigned int *indentation_level) {

    sds buf     = sdsempty();


    sds fn_name = ast_to_c(a->call_expr.function_identifier, indentation_level);

    buf         = sdscat(buf, fn_name);
    buf         = sdscat(buf, "(");

    int n       = arrlen(a->call_expr.arguments);

    if(n) {

        bool is_export_fn = false;

        if(STRING_EQUALS(fn_name, ODE_GET_VALUE) || STRING_EQUALS(fn_name, ODE_GET_TIME)) {
            is_export_fn = true;
        }

        sds tmp = ast_to_c(a->call_expr.arguments[0], indentation_level);
        //TODO: this should only be allowed inside a endfn function
        if(is_export_fn) {
            sds ode_name = sdsdup(tmp);
            ode_name     = sdscat(ode_name, "'");
            int position = shget(ode_position, ode_name);
            sdsfree(ode_name);
            buf = sdscatfmt(buf, "%i", position - 1);
        } else {
            buf = sdscat(buf, tmp);
        }
        sdsfree(tmp);

        for(int i = 1; i < n; i++) {
            tmp = ast_to_c(a->call_expr.arguments[i], indentation_level);
            buf = sdscatfmt(buf, ", %s", tmp);
            sdsfree(tmp);
        }
    }

    sdsfree(fn_name);
    buf = sdscat(buf, ")");

    return buf;
}

static sds global_variable_to_c(ast *a, unsigned int *indentation_level) {

    sds buf = sdsempty();
    sds tmp = ast_to_c(a->assignment_stmt.value, indentation_level);
    buf     = sdscatfmt(buf, "const real %s = %s;", a->assignment_stmt.name->identifier.value, tmp);

    if(a->assignment_stmt.unit != NULL) {
        buf = sdscatfmt(buf, " //%s", a->assignment_stmt.unit);
    }

    sdsfree(tmp);

    return buf;
}

static sds ast_to_c(ast *a, unsigned int *indentation_level) {

    if(a->tag == ast_assignment_stmt || a->tag == ast_grouped_assignment_stmt || a->tag == ast_ode_stmt) {
        return assignment_stmt_to_c(a, indentation_level);
    }

    if(a->tag == ast_global_stmt) {
        return global_variable_to_c(a, indentation_level);
    }

    if(a->tag == ast_return_stmt) {
        return return_stmt_to_c(a, indentation_level);
    }

    if(a->tag == ast_expression_stmt) {
        return expression_stmt_to_c(a, indentation_level);
    }

    if(a->tag == ast_number_literal) {
        return number_literal_to_c(a);
    }

    if(a->tag == ast_boolean_literal) {
        return boolean_literal_to_c(a);
    }
    if(a->tag == ast_string_literal) {
        return string_literal_to_c(a);
    }

    if(a->tag == ast_identifier) {
        return identifier_to_c(a);
    }

    if(a->tag == ast_prefix_expression) {
        return prefix_expr_to_c(a, indentation_level);
    }

    if(a->tag == ast_infix_expression) {
        return infix_expr_to_c(a, indentation_level);
    }

    if(a->tag == ast_if_expr) {
        return if_expr_to_c(a, indentation_level);
    }

    if(a->tag == ast_while_stmt) {
        return while_stmt_to_c(a, indentation_level);
    }

    if(a->tag == ast_call_expression) {
        return call_expr_to_c(a, indentation_level);
    }

    printf("[WARN] Line %d of file %s - to_c not implemented to operator %d\n", a->token.line_number, a->token.file_name, a->tag);

    return NULL;
}

void write_initial_conditions(program p, FILE *file) {

    int n_stmt = arrlen(p);
    for(int i = 0; i < n_stmt; i++) {
        assert(p[i]);

        ast *a            = p[i];

        uint32_t position = a->assignment_stmt.declaration_position;

        if(solver == CVODE_SOLVER) {
            if(a->assignment_stmt.unit != NULL) {
                fprintf(file, "    NV_Ith_S(x0, %d) = values[%d]; //%s %s\n", position - 1, position - 1, a->assignment_stmt.name->identifier.value, a->assignment_stmt.unit);
            } else {
                fprintf(file, "    NV_Ith_S(x0, %d) = values[%d]; //%s\n", position - 1, position - 1, a->assignment_stmt.name->identifier.value);
            }
        } else if(solver == EULER_ADPT_SOLVER) {
            if(a->assignment_stmt.unit != NULL) {
                fprintf(file, "    x0[%d] = values[%d]; //%s %s\n", position - 1, position - 1, a->assignment_stmt.name->identifier.value, a->assignment_stmt.unit);
            } else {
                fprintf(file, "    x0[%d] = values[%d]; //%s\n", position - 1, position - 1, a->assignment_stmt.name->identifier.value);
            }
        }
    }
}

static bool create_dynamic_array_headers(FILE *f) {

    fprintf(f, "//------------------ Support functions and data ---------------\n\n");

    fprintf(f, "#define print(X) \\\n");
    fprintf(f, "_Generic((X), \\\n");
    fprintf(f, "real: print_real, \\\n");
    fprintf(f, "char*: print_string,  \\\n");
    fprintf(f, "bool: print_boolean)(X)\n\n");

    fprintf(f, "void print_string(char *c) {\n");
    fprintf(f, "    printf(\"%%s\",c);\n");
    fprintf(f, "}\n\n");

    fprintf(f, "void print_real(real f) {\n");
    fprintf(f, "    printf(\"%%lf\",f);\n");
    fprintf(f, "}\n\n");

    fprintf(f, "void print_boolean(bool b) {\n");
    fprintf(f, "    printf(\"%%d\",b);\n");
    fprintf(f, "}\n\n");

    fprintf(f, "typedef struct __exposed_ode_value__t {\n");
    fprintf(f, "    real value;\n");
    fprintf(f, "    real time;\n");
    fprintf(f, "} __exposed_ode_value__;\n\n");

    fprintf(f, "typedef uint64_t u64;\n");
    fprintf(f, "typedef uint32_t u32;\n");
    fprintf(f, "\n");
    fprintf(f, "struct header {    \n");
    fprintf(f, "    u64 size;\n");
    fprintf(f, "    u64 capacity;\n");
    fprintf(f, "};\n");
    fprintf(f, "\n");
    fprintf(f, "\n");

    fprintf(f, "//INTERNALS MACROS\n");
    fprintf(f, "#define __original_address__(__l) ( (char*)(__l) - sizeof(struct header) )\n");
    fprintf(f, "#define __len__(__l) ( ((struct header *)(__original_address__(__l)))->size )\n");
    fprintf(f, "#define __cap__(__l) ( ((struct header* )(__original_address__(__l)))->capacity )\n");
    fprintf(f, "#define __internal_len__(__l) ( ((struct header *)(__l))->size )\n");
    fprintf(f, "#define __internal_cap__(__l) ( ((struct header *)(__l))->capacity )\n");
    fprintf(f, "\n");
    fprintf(f, "\n");
    fprintf(f, "//API\n");
    fprintf(f, "#define append(__l, __item) ( (__l) = maybe_grow( (__l), sizeof(*(__l)) ), __l[__len__(__l)++] = (__item) )\n");
    fprintf(f, "#define arrlength(__l) ( (__l) ? __len__(__l) : 0 )\n");
    fprintf(f, "#define arrcapacity(__l) ( (__l) ? __cap__(__l) : 0 )\n");
    fprintf(f, "#define arrfree(__l) free(__original_address__(__l))\n\n");

    fprintf(f, "void * maybe_grow(void *list, u64 data_size_in_bytes) {\n");
    fprintf(f, "\n");
    fprintf(f, "    u64 m = 2;\n");
    fprintf(f, "\n");
    fprintf(f, "    if(list == NULL) {\n");
    fprintf(f, "        list = malloc(data_size_in_bytes * m + sizeof(struct header));\n");
    fprintf(f, "        if(!list) return NULL;\n");
    fprintf(f, "        __internal_cap__(list) = m;\n");
    fprintf(f, "        __internal_len__(list) = 0;\n");
    fprintf(f, "    }\n");
    fprintf(f, "    else {\n");
    fprintf(f, "\n");
    fprintf(f, "        u64 list_size = __len__(list);\n");
    fprintf(f, "        m = __cap__(list);\n");
    fprintf(f, "\n");
    fprintf(f, "        if ((list_size + 1) > m) {\n");
    fprintf(f, "            m = m * 2;\n");
    fprintf(f, "            list = realloc(__original_address__(list), data_size_in_bytes * m + sizeof(struct header));\n");
    fprintf(f, "            if(!list) return NULL;\n");
    fprintf(f, "            __internal_cap__(list) = m;\n");
    fprintf(f, "        }\n");
    fprintf(f, "        else {\n");
    fprintf(f, "            return list;\n");
    fprintf(f, "        }\n");
    fprintf(f, "    }\n");
    fprintf(f, "\n");
    fprintf(f, "    return (char*) list + sizeof(struct header);\n");
    fprintf(f, "}\n\n");
    fprintf(f, "__exposed_ode_value__ **%s = NULL;\n", EXPOSED_ODE_VALUES_NAME);
    fprintf(f, "static int __ode_last_iteration__ = 1;\n\n");

    return false;
}

static void create_export_functions(FILE *f) {
    fprintf(f, "real %s(int ode_position, int timestep) {\n", ODE_GET_VALUE);
    fprintf(f, "    return __exposed_odes_values__[ode_position][timestep].value;\n");
    fprintf(f, "}\n\n");

    fprintf(f, "real %s(int ode_position, int timestep) {\n", ODE_GET_TIME);
    fprintf(f, "    return __exposed_odes_values__[ode_position][timestep].time;\n");
    fprintf(f, "}\n\n");

    fprintf(f, "int %s() {\n", ODE_GET_N_IT);
    fprintf(f, "    return __ode_last_iteration__;\n");
    fprintf(f, "}\n\n");
}

static sds generate_exposed_ode_values_for_loop() {

    sds code = sdsempty();
    code     = sdscatfmt(code, "__exposed_ode_value__ tmp;\n");
    if(solver == EULER_ADPT_SOLVER) {
        code = sdscatfmt(code, "                tmp.time = time_new;\n");
        code = sdscatfmt(code, "                tmp.value = sv[i];\n");
    } else {
        code = sdscatfmt(code, "                tmp.time = t;\n");
        code = sdscatfmt(code, "                tmp.value = NV_Ith_S(y,i);\n");
    }

    code = sdscatfmt(code, "                append(%s[i], tmp);\n", EXPOSED_ODE_VALUES_NAME);

    return code;
}


static bool generate_initial_conditions_values(program p, FILE *file, unsigned int *indentation_level) {

    int n_stmt = arrlen(p);

    fprintf(file, "    real values[%d];\n", n_stmt);
    int error = false;
    for(int i = 0; i < n_stmt; i++) {
        ast *a            = p[i];

        uint32_t position = a->assignment_stmt.declaration_position;
        sds value         = ast_to_c(a->assignment_stmt.value, indentation_level);
        fprintf(file, "    values[%d] = %s; //%s\n", position - 1, value, a->assignment_stmt.name->identifier.value);
        sdsfree(value);
    }
    fprintf(file, "    \n");
    fprintf(file, "    for(int i = 0; i < %d; i++) {\n", n_stmt);
    fprintf(file, "        __exposed_ode_value__ *__ode_values_array__ = NULL;\n");
    fprintf(file, "        __exposed_ode_value__ __tmp__;\n");
    fprintf(file, "        append(__exposed_odes_values__, NULL);\n");
    fprintf(file, "        __tmp__.value = values[i];\n");
    fprintf(file, "        __tmp__.time  = 0;\n");
    fprintf(file, "        append(__ode_values_array__, __tmp__);\n");
    fprintf(file, "        __exposed_odes_values__[i] = __ode_values_array__;\n");
    fprintf(file, "    }\n");

    return error;
}

void write_odes_old_values(program p, FILE *file) {

    int n_stmt = arrlen(p);
    for(int i = 0; i < n_stmt; i++) {

        ast *a = p[i];

        if(a->tag != ast_ode_stmt) continue;

        uint32_t position = a->assignment_stmt.declaration_position;

        if(solver == CVODE_SOLVER) {
            fprintf(file, "    const real %.*s =  NV_Ith_S(sv, %d);\n", (int) strlen(a->assignment_stmt.name->identifier.value) - 1,
                    a->assignment_stmt.name->identifier.value, position - 1);
        } else if(solver == EULER_ADPT_SOLVER) {
            fprintf(file, "    const real %.*s =  sv[%d];\n", (int) strlen(a->assignment_stmt.name->identifier.value) - 1,
                    a->assignment_stmt.name->identifier.value, position - 1);
        }
    }
}

sds out_file_header(program p) {

    sds ret    = sdsempty();

    int n_stmt = arrlen(p);

    ret        = sdscatprintf(ret, "\"#t");

    for(int i = 0; i < n_stmt; i++) {
        ast *a = p[i];
        if(a->tag != ast_ode_stmt) continue;

        ret = sdscatprintf(ret, ", %.*s", (int) strlen(a->assignment_stmt.name->identifier.value) - 1, a->assignment_stmt.name->identifier.value);
    }

    ret = sdscat(ret, "\\n\"");

    return ret;
}

void write_variables_or_body(program p, FILE *file, unsigned int *indentation_level) {
    int n_stmt = arrlen(p);
    for(int i = 0; i < n_stmt; i++) {
        ast *a = p[i];
        if(a->tag == ast_ode_stmt) {
            uint32_t position = a->assignment_stmt.declaration_position;
            sds tmp           = ast_to_c(a->assignment_stmt.value, indentation_level);
            sds name          = ast_to_c(a->assignment_stmt.name, indentation_level);
            shput(ode_position, name, position);
            sdsfree(name);

            if(solver == CVODE_SOLVER) {
                fprintf(file, "    NV_Ith_S(rDY, %d) = %s;\n", position - 1, tmp);
            } else if(solver == EULER_ADPT_SOLVER) {
                fprintf(file, "    rDY[%d] = %s;\n", position - 1, tmp);
            }

            sdsfree(tmp);
        } else {
            sds buf = ast_to_c(a, indentation_level);
            fprintf(file, "%s\n", buf);
            sdsfree(buf);
        }
    }
}

static void write_functions(program p, FILE *file, bool write_end_functions, unsigned int *indentation_level) {

    int n_stmt = arrlen(p);

    for(int i = 0; i < n_stmt; i++) {

        ast *a = p[i];

        if(write_end_functions == false) {
            if(a->function_stmt.is_end_fn) {
                continue;
            }
        } else {
            if(!a->function_stmt.is_end_fn) {
                continue;
            }
        }

        if(a->function_stmt.num_return_values == 1) {
            fprintf(file, "real %s", a->function_stmt.name->identifier.value);
        }

        else {
            fprintf(file, "void %s", a->function_stmt.name->identifier.value);
        }

        fprintf(file, "(");

        int n = arrlen(a->function_stmt.parameters);

        if(n) {
            sds tmp = ast_to_c(a->function_stmt.parameters[0], indentation_level);
            fprintf(file, "real %s", tmp);
            sdsfree(tmp);
            for(int j = 1; j < n; j++) {
                tmp = ast_to_c(a->function_stmt.parameters[j], indentation_level);
                fprintf(file, ", real %s", tmp);
                sdsfree(tmp);
            }

            if(a->function_stmt.num_return_values > 1) {
                for(int j = 0; j < a->function_stmt.num_return_values; j++) {
                    fprintf(file, ", real *ret_val_%d", j);
                }
            }

        } else {
            if(a->function_stmt.num_return_values > 1) {
                fprintf(file, "real *ret_val_%d", 0);
                for(int k = 1; k < a->function_stmt.num_return_values; k++) {
                    fprintf(file, ", real *ret_val_%d", k);
                }
            }
        }

        fprintf(file, ") {\n");

        n = arrlen(a->function_stmt.body);
        (*indentation_level)++;
        for(int j = 0; j < n; j++) {
            ast *ast_a = a->function_stmt.body[j];
            sds tmp    = ast_to_c(ast_a, indentation_level);

            if((ast_a->tag == ast_expression_stmt && ast_a->expr_stmt->tag == ast_if_expr) || ast_a->tag == ast_while_stmt || ast_a->tag == ast_return_stmt) {
                fprintf(file, "%s\n", tmp);
            } else {
                fprintf(file, "%s;\n", tmp);
            }
            sdsfree(tmp);
        }
        (*indentation_level)--;
        fprintf(file, "}\n\n");
    }
}

static sds generate_end_functions(program functions) {

    sds result = sdsempty();
    int len    = arrlen(functions);

    for(int i = 0; i < len; i++) {
        ast *a = functions[i];
        if(a->function_stmt.is_end_fn) {
            result = sdscatfmt(result, "%s();\n", a->function_stmt.name->identifier.value);
        }
    }

    return result;
}

static bool write_cvode_solver(FILE *file, program initial, program globals, program functions, program main_body, sds out_header, unsigned int *indentation_level) {

    fprintf(file, COMMON_INCLUDES
            "#include <cvode/cvode.h>\n"
            "#include <nvector/nvector_serial.h>\n"
            "#include <sundials/sundials_dense.h>\n"
            "#include <sundials/sundials_types.h>\n"
            "#include <sunlinsol/sunlinsol_dense.h> \n"
            "#include <sunmatrix/sunmatrix_dense.h>"
            " \n\n");


    WRITE_NEQ
    fprintf(file, "typedef realtype real;\n");

    create_dynamic_array_headers(file);
    create_export_functions(file);

    write_variables_or_body(globals, file, indentation_level);
    fprintf(file, "\n");

    write_functions(functions, file, false, indentation_level);

    fprintf(file, "void set_initial_conditions(N_Vector x0, real *values) { \n\n");
    write_initial_conditions(initial, file);
    fprintf(file, "\n}\n\n");

    // RHS CPU
    fprintf(file, "static int solve_model(realtype time, N_Vector sv, N_Vector rDY, void *f_data) {\n\n");

    fprintf(file, "    //State variables\n");
    write_odes_old_values(main_body, file);
    fprintf(file, "\n");

    fprintf(file, "    //Parameters\n");

    (*indentation_level)++;
    write_variables_or_body(main_body, file, indentation_level);
    (*indentation_level)--;

    fprintf(file, "\n    return 0;  \n\n}\n\n");

    sds export_code = generate_exposed_ode_values_for_loop();

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

    fprintf(file, "void solve_ode(N_Vector y, float final_t, FILE *f, char *file_name, SUNContext sunctx) {\n"
                  "\n"
                  "    void *cvode_mem = NULL;\n"
                  "    int flag;\n"
                  "\n"
                  "    // Set up solver\n"
                  "    cvode_mem = CVodeCreate(CV_BDF, sunctx);\n"
                  "\n"
                  "    if(cvode_mem == 0) {\n"
                  "        fprintf(stderr, \"Error in CVodeMalloc: could not allocate\\n\");\n"
                  "        return;\n"
                  "    }\n"
                  "\n"
                  "    flag = CVodeInit(cvode_mem, solve_model, 0, y);\n"
                  "    if(check_flag(&flag, \"CVodeInit\", 1))\n"
                  "        return;\n"
                  "\n"
                  "    flag = CVodeSStolerances(cvode_mem, 1.49012e-6, 1.49012e-6);\n"
                  "    if(check_flag(&flag, \"CVodeSStolerances\", 1))\n"
                  "        return;\n"
                  "\n"
                  "    // Create dense SUNMatrix for use in linear solver\n"
                  "    SUNMatrix A = SUNDenseMatrix(NEQ, NEQ, sunctx);\n"
                  "    if(check_flag((void *)A, \"SUNDenseMatrix\", 0))\n"
                  "        return;\n"
                  "\n"
                  "    // Create dense linear solver for use by CVode\n"
                  "    SUNLinearSolver LS = SUNLinSol_Dense(y, A, sunctx);\n"
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
                  "\n");

    fprintf(file, "    while(tout < final_t) {\n"
                  "\n"
                  "        retval = CVode(cvode_mem, tout, y, &t, CV_NORMAL);\n"
                  "\n"
                  "        if(retval == CV_SUCCESS) {\n"
                  "            fprintf(f, \"%%lf \", t);\n"
                  "            for(int i = 0; i < NEQ; i++) {\n"
                  "                fprintf(f, \"%%lf \", NV_Ith_S(y,i));\n"
                  "                %s\n"
                  "            }\n"
                  "\n"
                  "            fprintf(f, \"\\n\");\n"
                  "\n"
                  "            tout+=dt;\n"
                  "            __ode_last_iteration__+=1;\n"
                  "        }\n"
                  "\n"
                  "    }\n"
                  "\n"
                  "    // Free the linear solver memory\n"
                  "    SUNLinSolFree(LS);\n"
                  "    SUNMatDestroy(A);\n"
                  "    CVodeFree(&cvode_mem);\n"
                  "}\n",
            export_code);

    sdsfree(export_code);
    write_functions(functions, file, true, indentation_level);

    bool error;

    fprintf(file, "\nint main(int argc, char **argv) {\n"
                  "    SUNContext sunctx;\n"
                  "    SUNContext_Create(NULL, &sunctx);\n"
                  "    N_Vector x0 = N_VNew_Serial(NEQ, sunctx);\n"
                  "\n");
    error             = generate_initial_conditions_values(initial, file, indentation_level);

    sds end_functions = generate_end_functions(functions);
    fprintf(file, "    set_initial_conditions(x0, values);\n"
                  "    FILE *f = fopen(argv[2], \"w\");\n"
                  "    fprintf(f, %s);\n"
                  "    fprintf(f, \"0.0 \");\n"
                  "    for(int i = 0; i < NEQ; i++) {\n"
                  "        fprintf(f, \"%%lf \", NV_Ith_S(x0, i));\n"
                  "    }\n"
                  "    fprintf(f, \"\\n\");\n"
                  "\n\n",
            out_header);

    fprintf(file, "    solve_ode(x0, strtod(argv[1], NULL), f, argv[2], sunctx);\n"
                  "\n"
                  "    free(x0);\n"
                  "    %s\n"
                  "    return (0);\n"
                  "}",
            end_functions);

    sdsfree(end_functions);
    return error;
}

static bool write_adpt_euler_solver(FILE *file, program initial, program globals, program functions, program main_body, sds out_header, unsigned int *indentation_level) {

    fprintf(file, COMMON_INCLUDES " \n\n");

    WRITE_NEQ
    fprintf(file, "typedef double real;\n");

    create_dynamic_array_headers(file);
    create_export_functions(file);

    write_variables_or_body(globals, file, indentation_level);
    fprintf(file, "\n");

    write_functions(functions, file, false, indentation_level);

    fprintf(file, "void set_initial_conditions(real *x0, real *values) { \n\n");
    write_initial_conditions(initial, file);
    fprintf(file, "\n}\n\n");

    // RHS CPU
    fprintf(file, "static int solve_model(real time, real *sv, real *rDY) {\n\n");

    fprintf(file, "    //State variables\n");
    write_odes_old_values(main_body, file);
    fprintf(file, "\n");

    fprintf(file, "    //Parameters\n");

    (*indentation_level)++;
    write_variables_or_body(main_body, file, indentation_level);
    (*indentation_level)--;

    sds export_code = generate_exposed_ode_values_for_loop();

    fprintf(file, "\n    return 0;  \n\n}\n\n");

    fprintf(file, "void solve_ode(real *sv, float final_time, FILE *f, char *file_name) {\n"
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
                  "\n");

    fprintf(file, "    real min[NEQ];\n"
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
                  "            _k2__    = _k1__;\n"
                  "            _k1__    = _k_aux__;\n"
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
                  "                %s\n"
                  "            }\n"
                  "\n"
                  "            __ode_last_iteration__ += 1;\n"
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
                  "        fprintf(min_max_file, \"%%e;%%e\\n\", min[i], max[i]);\n"
                  "    }\n"
                  "    fclose(min_max_file);\n"
                  "    free(min_max);\n"
                  "    \n"
                  "    free(_k1__);\n"
                  "    free(_k2__);\n"
                  "}\n\n",
            export_code);

    sdsfree(export_code);

    write_functions(functions, file, true, indentation_level);

    fprintf(file, "\nint main(int argc, char **argv) {\n"
                  "\n"
                  "    real *x0 = (real*) malloc(sizeof(real)*NEQ);\n"
                  "\n");

    bool error        = generate_initial_conditions_values(initial, file, indentation_level);

    sds end_functions = generate_end_functions(functions);
    fprintf(file,
            "    set_initial_conditions(x0, values);\n"
            "    FILE *f = fopen(argv[2], \"w\");\n"
            "    fprintf(f, %s);\n"
            "    fprintf(f, \"0.0 \");\n"
            "    for(int i = 0; i < NEQ; i++) {\n"
            "        fprintf(f, \"%%lf \", x0[i]);\n"
            "    }\n"
            "    fprintf(f, \"\\n\");\n"
            "\n\n",
            out_header);
    fprintf(file,
            "    solve_ode(x0, strtod(argv[1], NULL), f, argv[2]);\n"
            "    free(x0);\n"
            "    %s\n"
            "    return (0);\n"
            "}",
            end_functions);

    sdsfree(end_functions);


    return error;
}

bool convert_to_c(program prog, FILE *file, solver_type p_solver) {

    unsigned int indentation_level = 0;

    solver                         = p_solver;

    program main_body              = NULL;
    program functions              = NULL;
    program initial                = NULL;
    program globals                = NULL;
    program imports                = NULL;

    int n_stmt                     = arrlen(prog);

    bool error                     = false;

    for(int i = 0; i < n_stmt; i++) {
        ast *a = prog[i];
        switch(a->tag) {
            case ast_function_statement:
                arrput(functions, a);
                break;
            case ast_initial_stmt:
                arrput(initial, a);
                break;
            case ast_global_stmt:
                arrput(globals, a);
                break;
            case ast_import_stmt:
                arrput(imports, a);
                break;
            default:
                arrput(main_body, a);
        }
    }

    sh_new_arena(var_declared);
    sh_new_arena(ode_position);

    sds out_header = out_file_header(main_body);

    switch(solver) {
        case CVODE_SOLVER:
            error = write_cvode_solver(file, initial, globals, functions, main_body, out_header, &indentation_level);
            break;
        case EULER_ADPT_SOLVER:
            error = write_adpt_euler_solver(file, initial, globals, functions, main_body, out_header, &indentation_level);
            break;
        default:
            fprintf(stderr, "Error: invalid solver type!\n");
    }

    sdsfree(out_header);

    shfree(var_declared);
    shfree(ode_position);
    arrfree(main_body);
    arrfree(functions);
    arrfree(initial);
    arrfree(globals);
    arrfree(imports);

    return error;
}
