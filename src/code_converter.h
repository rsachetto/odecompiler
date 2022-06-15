#ifndef __C_CONVERTER_H
#define __C_CONVERTER_H 

#include <stdio.h>
#include "compiler/parser.h"

#define EXPOSED_ODE_VALUES_NAME "__exposed_odes_values__"

struct var_declared_entry_t {
    char *key;
    int value;
};


typedef enum solver_type_t{
    CVODE_SOLVER,
    EULER_ADPT_SOLVER
} solver_type;

bool convert_to_c(program p, FILE *out, solver_type solver);

#endif /* __C_CONVERTER_H */
