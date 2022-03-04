#include "model_config.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "stb/stb_ds.h"
#include "file_utils/file_utils.h"

#define MODEL_OUTPUT_TEMPLATE "/tmp/%s_%i_out.txt"

sds get_model_output_file(struct model_config *model_config, uint run_number) {

    sds modified_model_name = sdsnew(model_config->model_name);
    modified_model_name = sdsmapchars(modified_model_name, "/", ".", 1);

    sds model_out_file = sdscatfmt(sdsempty(), MODEL_OUTPUT_TEMPLATE, modified_model_name, run_number);
    sdsfree(modified_model_name);
    return model_out_file;
}

bool generate_model_program(struct model_config *model) {

    char *file_name = model->model_file;

    if(!file_exists(file_name)) {
        printf("Error: file %s does not exist!\n", file_name);
        return true;
    }

    size_t file_size;
    char *source = read_entire_file_with_mmap(file_name, &file_size);

    model->hash = MeowHash(MeowDefaultSeed, file_size, source);

    lexer *l = new_lexer(source, file_name);
    parser *p = new_parser(l);
    program program = parse_program(p, true, true);

    shfree(model->var_indexes);
    model->var_indexes = NULL;
    sh_new_arena(model->var_indexes);
    shdefault(model->var_indexes, -1);

    int n_stmt = arrlen(program);

    shput(model->var_indexes, "t", 1);

    int ode_count = 2;
    for(int i = 0; i < n_stmt; i++) {
        ast *a = program[i];
        if(a->tag == ast_ode_stmt) {
            sds var_name = sdscatprintf(sdsempty(), "%.*s", (int)strlen(a->assignement_stmt.name->identifier.value)-1, a->assignement_stmt.name->identifier.value);
            shput(model->var_indexes, var_name, ode_count);
            sdsfree(var_name);
            ode_count++;

        }
    }
    model->program = program;

    munmap(source, file_size);

    free_parser(p);
    free_lexer(l);

    return false;

}

struct model_config *new_config_from_parent(struct model_config *parent_model_config) {

    parent_model_config->version++;

    sds new_model_name = sdsnew(parent_model_config->model_name);
    new_model_name     = sdscatfmt(new_model_name, "_v%i", parent_model_config->version);

    struct model_config *model_config = calloc(1, sizeof(struct model_config));

    if(model_config == NULL) {
        fprintf(stderr, "%s - Error allocating memory for the new model config!\n", __FUNCTION__);
        return NULL;
    }

    model_config->model_name = strdup(new_model_name);
    model_config->model_file = strdup(parent_model_config->model_file);

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

    sdsfree(new_model_name);

    return model_config;
}

void free_model_config(struct model_config *model_config) {

    if(model_config == NULL) return;

    for(int r = 0; r < model_config->num_runs; r++) {

        free(model_config->runs[r].filename);
        free(model_config->runs[r].vars_max_value);
        free(model_config->runs[r].vars_min_value);

        sds out = get_model_output_file(model_config, r);
        unlink(out);
        sdsfree(out);
    }

    if(model_config->model_command) {
        unlink(model_config->model_command);
    }

    free(model_config->model_name);
    free(model_config->model_file);
    free(model_config->plot_config.title);
    free(model_config->plot_config.xlabel);
    free(model_config->plot_config.ylabel);

    free(model_config->model_command);
    free_program(model_config->program);
    arrfree(model_config->runs);

    shfree(model_config->var_indexes);

    free(model_config);
}

char *get_var_name(struct model_config *model_config, int index) {

    struct var_index_hash_entry *var_indexes = model_config->var_indexes;

    int len = shlen(var_indexes);

    for(int i = 0; i < len; i++) {
        if(var_indexes[i].value == index)
            return var_indexes[i].key;
    }

    return NULL;

}
