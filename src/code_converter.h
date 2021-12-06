#ifndef __C_CONVERTER_H
#define __C_CONVERTER_H 

#include <stdio.h>
#include "compiler/parser.h"

typedef enum solver_type_t{
    CVODE_SOLVER,
    EULER_ADPT_SOLVER
} solver_type;

bool convert_to_c(program p, FILE *out, solver_type solver);

#endif /* __C_CONVERTER_H */
