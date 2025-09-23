#ifndef __C_CONVERTER_H
#define __C_CONVERTER_H

#include "compiler/parser.h"
#include <stdio.h>

#define EXPOSED_ODE_VALUES_NAME "__exposed_odes_values__"

struct var_declared_entry_t {
    char *key;
    int value;
};

typedef enum solver_type_t {
    CVODE_SOLVER,
    EULER_ADPT_SOLVER
} solver_type;

typedef struct solver_config_t {
    unsigned int indentation_level;
    solver_type solver_type;
} solver_config;

bool convert_to_c(program p, FILE *out, solver_type solver);

#endif /* __C_CONVERTER_H */
