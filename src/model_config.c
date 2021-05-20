#include "model_config.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "stb/stb_ds.h"
#include "file_utils/file_utils.h"

bool generate_model_program(struct model_config *model) {

    char *file_name = model->model_file;

    if(!file_exists(file_name)) {
        printf("Error: file %s does not exist!\n", file_name);
        return true;
    }

    size_t file_size;
    char *source = read_entire_file_with_mmap(file_name, &file_size);

	//TODO: maybe we can try to avoid this
	model->hash = MeowHash(MeowDefaultSeed, file_size, source);

    lexer *l = new_lexer(source, file_name);
    parser *p = new_parser(l);
    program program = parse_program(p);

    bool error = check_parser_errors(p, false);

    if(!error) {
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
                ode_count++;

            }
        }
        model->program = program;
    }

	munmap(source, file_size);

    return error;

}
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

char *get_var_name(struct model_config *model_config, int index) {

    struct var_index_hash_entry *var_indexes = model_config->var_indexes;

    int len = shlen(var_indexes);

    for(int i = 0; i < len; i++) {
        if(var_indexes[i].value == index)
            return var_indexes[i].key;
    }

    return NULL;

}
