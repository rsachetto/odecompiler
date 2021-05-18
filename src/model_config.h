#ifndef __MODEL_CONFIG_H
#define __MODEL_CONFIG_H 

#include "compiler/parser.h"

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
    struct plot_config plot_config;
};

struct model_config *new_config_from_parent(struct model_config *parent_model_config);
char *get_var_name(struct model_config *model_config, int index);
void free_model_config(struct model_config *model_config);

#endif /* __MODEL_CONFIG_H */
