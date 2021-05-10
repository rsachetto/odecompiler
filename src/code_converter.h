#ifndef __C_CONVERTER_H
#define __C_CONVERTER_H 

#include <stdio.h>
#include "compiler/parser.h"

#define NO_SPACES  ""
#define _4SPACES   "    "
#define _8SPACES  _4SPACES _4SPACES
#define _12SPACES _8SPACES _4SPACES
#define _16SPACES _12SPACES _4SPACES
#define _20SPACES _16SPACES _4SPACES
#define _24SPACES _20SPACES _4SPACES
#define _28SPACES _24SPACES _4SPACES

static char *builtin_functions[] = {"acos",
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
                                    "trunc"};

static int indentation_level = 0;
static char *indent_spaces[] = {NO_SPACES, _4SPACES, _8SPACES, _12SPACES, _16SPACES, _20SPACES, _24SPACES, _28SPACES};

typedef struct declared_variable_entry_t {
    char *key;
    int value;
    int num_args;
} declared_variable_entry;

typedef enum solver_type_t{
    CVODE_SOLVER,
    EULER_ADPT_SOLVER
} solver_type;

typedef declared_variable_entry * declared_variable_hash;

typedef struct ode_initialized_hash_entry_t {
    char *key;
    bool value;
} ode_initialized_hash_entry;

void convert_to_c(program p, FILE *out, solver_type solver);

#endif /* __C_CONVERTER_H */
