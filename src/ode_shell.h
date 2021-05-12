//
// Created by sachetto on 12/05/2021.
//

#ifndef ODECOMPILER_ODE_SHELL_H
#define ODECOMPILER_ODE_SHELL_H

extern command *commands;

struct var_index_hash_entry {
    char *key;
    int value;
};

struct model_config {
    char *model_name;
    char *model_file;
    char *model_command;
    char *output_file;
    char *xlabel;
    char *ylabel;
    program program;
    struct var_index_hash_entry *var_indexes;
    int xindex;
    int yindex;
};

struct shell_variables {
    struct model_hash_entry *loaded_models;
    struct model_config *last_loaded_model;
    FILE *gnuplot_handle;
};

struct model_hash_entry {
    char *key;
    struct model_config *value;
};

#define PROMPT "ode_shell> "
#define HISTORY_FILE ".ode_history"

#endif//ODECOMPILER_ODE_SHELL_H
