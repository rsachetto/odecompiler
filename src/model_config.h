#ifndef __MODEL_CONFIG_H
#define __MODEL_CONFIG_H

#include "compiler/parser.h"
#include <stdint.h>

struct var_index_hash_entry {
    char *key;
    int value;
};

struct plot_config {
    char *xlabel;
    char *ylabel;
    char *title;
    int xindex;
    int yindex;
};

struct run_info {
    char *filename;
    double *vars_max_value;
    double *vars_min_value;
    bool saved;
    double time;
};

struct model_config {
    char *model_name;
    char *model_file;
    sds   model_command;
    uint version;
    uint num_runs;
    struct run_info *runs;
    program program;
    struct var_index_hash_entry *var_indexes;
    struct plot_config plot_config;
    bool is_derived;
    bool should_reload;
    bool auto_reload;
    int notify_code;
    uint8_t hash[16];
};


#define PRINT_NO_MODELS_LOADED_ERROR(command__) printf("Error executing command %s. No models loaded. Load a model first using load modelname.edo\n", command__)

#define GET_MODEL_ONE_ARG_OR_RETURN_FALSE(model_config, args)                                     \
    do {                                                                                          \
        if ((num_args) == args) {                                                                 \
            (model_config) = load_model_config_or_print_error(shell_state, tokens[0], NULL);      \
                                                                                                  \
        } else {                                                                                  \
            (model_config) = load_model_config_or_print_error(shell_state, tokens[0], tokens[1]); \
        }                                                                                         \
        if (!(model_config)) return false;                                                        \
    } while (0)


struct model_config *new_config_from_parent(struct model_config *parent_model_config);
char *get_var_name(struct model_config *model_config, int index);
void free_model_config(struct model_config *model_config);
bool generate_model_program(struct model_config *model);
sds get_model_output_file(struct model_config *model_config, uint run_number);
bool compile_model(struct model_config *model_config);
#endif /* __MODEL_CONFIG_H */
