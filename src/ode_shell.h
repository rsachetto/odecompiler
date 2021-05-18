//
// Created by sachetto on 12/05/2021.
//

#ifndef ODECOMPILER_ODE_SHELL_H
#define ODECOMPILER_ODE_SHELL_H

#include "model_config.h"

#include <stdio.h>

struct shell_variables {
    struct model_hash_entry *loaded_models;
    struct model_config *current_model;
    
    FILE *gnuplot_handle;
    const char *default_gnuplot_term;

};

struct model_hash_entry {
    char *key;
    struct model_config *value;
};

#define PROMPT "ode_shell> "
#define HISTORY_FILE ".ode_history"

#endif//ODECOMPILER_ODE_SHELL_H
