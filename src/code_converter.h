#ifndef __C_CONVERTER_H
#define __C_CONVERTER_H 

#include <stdio.h>
#include "compiler/parser.h"

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

bool convert_to_c(program p, FILE *out, solver_type solver);

#endif /* __C_CONVERTER_H */
