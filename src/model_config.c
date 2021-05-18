#include "model_config.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "stb/stb_ds.h"

struct model_config *new_config_from_parent(struct model_config *parent_model_config) {

    parent_model_config->version++;

    sds new_model_name = sdsnew(parent_model_config->model_name);
    new_model_name     = sdscatfmt(new_model_name, "_v%i", parent_model_config->version);

    struct model_config *model_config = calloc(1, sizeof(struct model_config));
    model_config->model_name = strdup(new_model_name);
    model_config->model_file = parent_model_config->model_file;

    model_config->plot_config.xindex = parent_model_config->plot_config.xindex;
    model_config->plot_config.yindex = parent_model_config->plot_config.yindex;

    model_config->plot_config.xlabel = strdup(parent_model_config->plot_config.xlabel);
    model_config->plot_config.ylabel = strdup(parent_model_config->plot_config.ylabel);
    model_config->plot_config.title = strdup(parent_model_config->plot_config.title);

    model_config->program = copy_program(parent_model_config->program);

    int n = shlen(parent_model_config->var_indexes);

    for(int i = 0; i < n; i++) {
        shput(model_config->var_indexes,parent_model_config->var_indexes[i].key, parent_model_config->var_indexes[i].value);
    }

    return model_config;
}

void free_model_config(struct model_config *model_config) {

    if(model_config->output_file) {
        unlink(model_config->output_file);
    }

    if(model_config->model_command) {
        unlink(model_config->model_command);
    }

    free(model_config->model_name);
    free(model_config->model_file);
    free(model_config->model_command);
    free(model_config->output_file);
    free_program(model_config->program);
    shfree(model_config->var_indexes);
}
