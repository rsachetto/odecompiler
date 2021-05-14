//
// Created by sachetto on 12/05/2021.
//

#ifndef ODECOMPILER_ODE_SHELL_H
#define ODECOMPILER_ODE_SHELL_H

#include "compiler/parser.h"
#include <stdio.h>

struct var_index_hash_entry {
    char *key;
    int value;
};

struct plot_config {
    const char *xlabel;
    const char *ylabel;
    const char *title;
    int xindex;
    int yindex;
};

struct model_config {
    char *model_name;
    char *model_file;
    char *model_command;
    char *output_file;
    int version;
    program program;
    struct var_index_hash_entry *var_indexes;
    struct model_config *parent_model;
    struct plot_config plot_config;
};

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
