#include <linux/limits.h>
#include <math.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>
#include<sys/inotify.h>

#include "code_converter.h"
#include "file_utils/file_utils.h"
#include "stb/stb_ds.h"
#include "string/sds.h"
#include "string_utils.h"
#include "ode_shell.h"
#include "commands.h"
#include "inotify_helpers.h"

extern command *commands;
extern string_array commands_sorted;

static bool parse_and_execute_command(sds line, struct shell_variables *shell_state);

static sigjmp_buf env;
static void ctrl_c_handler(int sig) {
    siglongjmp(env, 42);
}

static void clean_and_exit(struct shell_variables *shell_state) {
    
    sds history_path = sdsnew(get_home_dir());
    history_path = sdscatfmt(history_path, "/%s", HISTORY_FILE);
    
    int n_models = shlen(shell_state->loaded_models);
    for(int i = 0; i < n_models; i++) {
        struct model_config *config = shell_state->loaded_models[i].value;
        if(config->output_file) {
            unlink(config->output_file);
        }
        if(config->model_command) {
            unlink(config->model_command);
        }
    }
    
    printf("\n");
    write_history(history_path);
    exit(0);
    
}

static void gnuplot_cmd(FILE *handle, char const *cmd, ...) {
    va_list ap;
    
    va_start(ap, cmd);
    vfprintf(handle, cmd, ap);
    va_end(ap);

    fputs("\n", handle);
    fflush(handle);
}

static void reset_terminal(FILE *handle, const char *default_term) {
    gnuplot_cmd(handle, "set terminal %s", default_term);
}

static bool check_and_print_execution_errors(FILE *fp) {
    
    bool error = false;
    char msg[PATH_MAX];
    
    while (fgets(msg, PATH_MAX, fp) != NULL) {
        printf("%s", msg);
        if(!error) error = true;
    }
    
    return error;
    
}

static struct model_config *load_model_config_or_print_error(struct shell_variables *shell_state, sds *tokens, int num_args, int model_name_position) {
    
    char *command = tokens[0];
    
    if(shlen(shell_state->loaded_models) == 0) {
        printf ("Error executing command %s. No models loaded. Load a model first using load modelname.edo\n", command);
        return NULL;
    }
    
    const char *model_name;
    
    if(num_args == model_name_position) {
        return shell_state->current_model;
    }
    else {
        model_name = tokens[1];
    }
    
    struct model_config *model_config = shget(shell_state->loaded_models, model_name);
    
    if(!model_config) {
        printf ("Error executing command %s. model %s is not loaded. Load a model first using \"load %s.edo\" or list loaded models using \"list\"\n", command, model_name, model_name);
        return NULL;
    }
    
    return model_config;
    
}

static void run_commands_from_file(char *file_name, struct shell_variables *shell_state) {
    
    bool quit = false;
    
    if (!file_exists(file_name)) {
        printf("File %s does not exist!\n", file_name);
    } else {
        FILE *f = fopen(file_name, "r");
        
        if (!f) {
            printf("Error opening file %s for reading!\n", file_name);
        } else {
            char *line = NULL;
            size_t len = 0;
            sds command;
            
            while ((getline(&line, &len, f)) != -1) {
                
                if(!line[0] || line[0] == '#' || line[0] == '\n') continue;
                
                command = sdsnew(line);
                command = sdstrim(command, "\n ");
                add_history(command);
                printf("%s%s\n", PROMPT, command);
                quit = parse_and_execute_command(command, shell_state);
                sdsfree(command);
                if(quit) break;
            }
            
            fclose(f);
            if (line) {
                free(line);
            }
            
            
            if(quit) {
                sleep(1);
                clean_and_exit(shell_state);
            }
            
        }
    }
}

static bool compile_model(struct model_config *model_config) {
    sds modified_model_name = sdsnew(model_config->model_name);
    modified_model_name = sdsmapchars(modified_model_name, "/", ".", 1);

    sds compiled_model_name = sdscatfmt(sdsempty(), "/tmp/%s_auto_compiled_model_tmp_file", modified_model_name);
    model_config->model_command = strdup(compiled_model_name);

    sds compiled_file;
    compiled_file = sdscatfmt(sdsempty(), "/tmp/%s_XXXXXX.c", modified_model_name);

    int fd = mkstemps(compiled_file, 2);

    FILE *outfile = fdopen(fd, "w");
    convert_to_c(model_config->program, outfile, EULER_ADPT_SOLVER);
    fclose(outfile);

    sds compiler_command = sdsnew("gcc");
    compiler_command = sdscatfmt(compiler_command, " %s -o %s -lm", compiled_file, model_config->model_command);

    FILE *fp = popen(compiler_command, "r");
    bool error = check_and_print_execution_errors(fp);

    pclose(fp);
    unlink(compiled_file);

    //Clean
    sdsfree(compiled_file);
    sdsfree(compiled_model_name);
    sdsfree(compiler_command);
    sdsfree(modified_model_name);

    return error;
}

void maybe_reload_from_file_change(struct shell_variables *shell_state, int wd) {

    if(shell_state->never_reload) return;

    pthread_mutex_lock(&shell_state->lock);
    struct model_config *model_config = hmget(shell_state->notify_entries, wd);

    if(!model_config->should_reload) {
        pthread_mutex_unlock(&shell_state->lock);
        return;
    }

    if(model_config && !model_config->is_derived) {

        char answer = 0;

        if(!model_config->auto_reload) {
            printf("\nModel %s was modified externally. Would you like to reload it? [y]: ", model_config->model_name);
            answer = getchar();
        }

        if(model_config->auto_reload || answer == 'Y' || answer == 'y' || answer == '\r') {
            printf("\nReloading model %s", model_config->model_name);
            fflush(stdout);
            free_program(model_config->program);
            bool error = generate_model_program(model_config);

            if(!error) {
                error = compile_model(model_config);
            }
            else {
                printf("Error compiling model %s", model_config->model_name);
            }
        }

    }

    printf("\n%s", PROMPT);
    fflush(stdout);

    pthread_mutex_unlock(&shell_state->lock);

}

static void execute_load_command(struct shell_variables *shell_state, const char *model_file, struct model_config *model_config) {

    bool error = false;
    bool new_model = (model_config == NULL);

    if(new_model) {
        model_config = calloc(1, sizeof(struct model_config));

        model_config->should_reload = true;
        model_config->auto_reload   = false;


        char *model_name = get_filename_without_ext(model_file);
        model_config->model_name = strdup(model_name);

        sds full_model_file_path;

        if(model_file[0] != '/') {
            full_model_file_path = sdsnew(shell_state->current_dir);
        }
        else {
            full_model_file_path = sdsempty();
        }

        full_model_file_path = sdscatfmt(full_model_file_path, "%s", model_file);
        model_config->model_file = strdup(full_model_file_path);
        sdsfree(full_model_file_path);

        error = generate_model_program(model_config);
        
        if(error) return;
        
        model_config->plot_config.xindex = 1;
        model_config->plot_config.yindex = 2;
        
        if(!model_config->plot_config.xlabel) {
            model_config->plot_config.xlabel = strdup(get_var_name(model_config, 1));
        }

        if(!model_config->plot_config.ylabel) {
            model_config->plot_config.ylabel = strdup(get_var_name(model_config, 2));
        }

        if(!model_config->plot_config.title) {
            model_config->plot_config.title = strdup(model_config->plot_config.ylabel);
        }
    }

    model_config->is_derived = !new_model;

    //TODO: handle errors
    error = compile_model(model_config);

    shput(shell_state->loaded_models, model_config->model_name, model_config);
    shell_state->current_model = model_config;

    if(new_model) {
        add_dir_watch(shell_state, shell_state->current_dir);
    }

}

static bool execute_solve_command(struct shell_variables *shell_state, sds *tokens, int num_args) {
    char *simulation_steps_str;
    double simulation_steps = 0;
    
    if(num_args == 1) {
        simulation_steps_str = tokens[1];
    }
    else {
        simulation_steps_str = tokens[2];
    }
    
    simulation_steps = string_to_double(simulation_steps_str);
    
    if(isnan(simulation_steps) || simulation_steps == 0) {
        printf ("Error parsing command %s. Invalid number: %s\n", tokens[0], simulation_steps_str);
        return false;
    }
    
    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 1);
    
    if(!model_config) return false;

    sds modified_model_name = sdsnew(model_config->model_name);
    modified_model_name = sdsmapchars(modified_model_name, "/", ".", 1);

    sds model_out_file = sdscatfmt(sdsempty(), "/tmp/%s_out.txt", modified_model_name);
    model_config->output_file = strdup(model_out_file);
    
    sds model_command = sdscat(sdsempty(), model_config->model_command);
    
    model_command = sdscatprintf(model_command, " %lf %s", simulation_steps, model_config->output_file);
    
    FILE *fp = popen(model_command, "r");
    check_and_print_execution_errors(fp);
    pclose(fp);
    
    sdsfree(model_command);
    sdsfree(model_out_file);
    
    return true;
    
}


//TODO: refactor this function. maybe break it in to two separate functions
static void execute_plot_command(struct shell_variables *shell_state, sds *tokens, command_type c_type, int num_args) {
    
    const char *command = tokens[0];
    struct model_config *model_config = NULL;
    
    if(c_type == CMD_PLOT || c_type == CMD_REPLOT || c_type == CMD_PLOT_TERM || c_type == CMD_REPLOT_TERM) {
        model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 0);
    }
    else if(c_type == CMD_PLOT_FILE || c_type == CMD_REPLOT_FILE) {
        model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 1);
    }
    
    if(!model_config) return;
    
    if(!model_config->output_file) {
        printf ("Error executing command %s. Model %s was not executed. Run then model first using \"solve %s\" or list loaded models using \"list\"\n", command, model_config->model_name, model_config->model_name);
        return;
    }
    
    if(shell_state->gnuplot_handle == NULL) {
        if(c_type == CMD_REPLOT || c_type == CMD_REPLOT_FILE) {
            printf ("Error executing command %s. No previous plot. plot the model first using \"plot modelname\" or list loaded models using \"list\"\n", command);
            return;
        }
        
        shell_state->gnuplot_handle = popen("gnuplot", "w");
    }
    
    if(c_type == CMD_PLOT_FILE || c_type == CMD_REPLOT_FILE) {
        if(c_type == CMD_PLOT_FILE) {
            command = "plot";
        }
        else {
            command = "replot";
        }

        const char *file_name = tokens[num_args];

        const char *ext = get_filename_ext(file_name);

        if(!FILE_HAS_EXTENSION(ext, "pdf") && !FILE_HAS_EXTENSION(ext, "png")) {
            printf("Error executing command %s. Only .pdf and .png outputs are supported\n", command);
            return;
        }

        gnuplot_cmd(shell_state->gnuplot_handle, "set terminal %s", ext);
        gnuplot_cmd(shell_state->gnuplot_handle, "set output \"%s", file_name);
    }

    if(c_type == CMD_PLOT_TERM || c_type == CMD_REPLOT_TERM) {
        if(c_type == CMD_PLOT_TERM) {
            command = "plot";
        }
        else {
            command = "replot";
        }
        gnuplot_cmd(shell_state->gnuplot_handle, "set term dumb");
    }

    gnuplot_cmd(shell_state->gnuplot_handle, "set xlabel \"%s\"", model_config->plot_config.xlabel);
    gnuplot_cmd(shell_state->gnuplot_handle, "set ylabel \"%s\"", model_config->plot_config.ylabel);
    gnuplot_cmd(shell_state->gnuplot_handle, "%s '%s' u %d:%d title \"%s\" w lines lw 2", command, model_config->output_file, model_config->plot_config.xindex, model_config->plot_config.yindex, model_config->plot_config.title);


    if(c_type == CMD_PLOT_FILE || c_type == CMD_REPLOT_FILE || c_type == CMD_PLOT_TERM || c_type == CMD_REPLOT_TERM) {
        reset_terminal(shell_state->gnuplot_handle, shell_state->default_gnuplot_term);
    }

}

static void execute_setplot_command(struct shell_variables *shell_state, sds *tokens, command_type c_type, int num_args) {
    
    const char *command = tokens[0];
    
    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 1);
    
    if(!model_config) return;
    
    char *cmd_param = tokens[num_args];

    if(c_type == CMD_PLOT_SET_X || c_type == CMD_PLOT_SET_Y) {
        
        bool index_as_str;
        int index = (int)string_to_long(cmd_param, &index_as_str);
        char *var_name = NULL;

        if(index_as_str) {
            //string was passed, try to get the index from the var_indexes on model_config
            index = shget(model_config->var_indexes, cmd_param);

            if(index == -1) {
                printf ("Error parsing command %s. Invalid variable name: %s. You can list model variable name using vars %s\n", command, cmd_param, model_config->model_name);
                return;
            }
        } else {
            var_name = get_var_name(model_config, index);

            if(!var_name) {
                printf ("Error parsing command %s. Invalid index: %d. You can list model variable name using vars %s\n", command, index, model_config->model_name);
                return;
                
            }
        }
        
        if(c_type == CMD_PLOT_SET_X) {
            model_config->plot_config.xindex = index;
        }
        else {
            model_config->plot_config.yindex = index;
            
            if(index_as_str) {
                model_config->plot_config.title = strdup(cmd_param);
            }
            else {
                model_config->plot_config.title = strdup(var_name);
            }
        }
    }
    else if(c_type == CMD_PLOT_SET_X_LABEL || c_type == CMD_PLOT_SET_Y_LABEL) {
        
        if(c_type == CMD_PLOT_SET_X_LABEL) {
            model_config->plot_config.xlabel = strdup(cmd_param);
        }
        else {
            model_config->plot_config.ylabel = strdup(cmd_param);
        }
    }
    else {
        model_config->plot_config.title = strdup(cmd_param);
    }
}

static void execute_list_command(struct shell_variables *shell_state) {
    
    int len = shlen(shell_state->loaded_models);
    
    if(len == 0) {
        printf ("No models loaded\n");
        return;
    }
    
    printf("Current model: %s\n", shell_state->current_model->model_name);
    printf("Loaded models:\n");
    for(int i = 0; i < len; i++) {
        printf("%s\n", shell_state->loaded_models[i].key);
    }
    
}

static void execute_vars_command(struct shell_variables *shell_state, sds *tokens, int num_args) {
    
    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 0);
    
    if(!model_config) return;
    
    int len = shlen(model_config->var_indexes);
    
    printf("Model vars for model %s:\n", model_config->model_name);
    for(int i = 0; i < len; i++) {
        printf("%s, %d\n", model_config->var_indexes[i].key, model_config->var_indexes[i].value);
    }
    
    return;
    
}

static void execute_cd_command(struct shell_variables *shell_state,  const char *path) {

    int ret = chdir(path);
    if(ret == -1) {
        printf("Error changing working directory to %s\n", path);
    }
    else {
        free(shell_state->current_dir);
        shell_state->current_dir = get_current_directory();
        print_current_dir();
    }
}

static void execute_ls_command(char *p, int num_args) {
    
    char *path_name;
    
    if(num_args == 0) {
        path_name = ".";
    }
    else {
        path_name = p;
    }
    
    print_path_contents(path_name);
    
}

static void execute_help_command(sds *tokens, int num_args) {
    if(num_args == 0) {
        printf("Available commands:\n");
        int nc = arrlen(commands_sorted);
        for(int i = 0; i < nc; i++) {
            printf("%s\n", commands_sorted[i]);
        }

        printf("type 'help command' for more information about a specific command\n");
    } else {
        command command = shgets(commands, tokens[1]);
        if(command.help) {
            printf("%s\n", command.help);
        }
        else {
            printf("No help available for command %s yet\n", tokens[1]);
        }
        
    }
}

static void execute_getplotconfig_command(struct shell_variables *shell_state, sds *tokens, int num_args) {
    
    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 0);
    
    if(!model_config) return;
    
    char *xname = get_var_name(model_config, model_config->plot_config.xindex);
    char *yname = get_var_name(model_config, model_config->plot_config.yindex);
    
    printf("Plot configuration for model %s\n\n", model_config->model_name);
    
    printf("Var on X: %s (%d)\n", xname, model_config->plot_config.xindex);
    printf("Var on Y: %s (%d)\n", yname, model_config->plot_config.yindex);
    
    printf("Label X: %s\n", model_config->plot_config.xlabel);
    printf("Label Y: %s\n", model_config->plot_config.ylabel);
    
    
    return;
    
}

static void execute_set_or_get_value_command(struct shell_variables *shell_state, sds *tokens, int num_args, ast_tag tag, bool set) {
    
    const char *command = tokens[0];
    char *var_name;
    char *new_value;
    
    struct model_config *parent_model_config;
    
    if(set) {
        var_name = tokens[num_args - 1];
        new_value = tokens[num_args];
        parent_model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 2);
        
    } else {
        var_name = tokens[num_args];
        parent_model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 1);
    }
    
    if(!parent_model_config) return;

    struct model_config *model_config;


    if(set) {
        model_config = new_config_from_parent(parent_model_config);
    }
    else {
        model_config = parent_model_config;
    }

    int n = arrlen(model_config->program);

    int i;
    for(i = 0; i < n; i++) {
        ast *a = model_config->program[i];
        if(a->tag == tag) {
                if(STR_EQUALS(a->assignement_stmt.name->identifier.value, var_name)) {

                    if(set) {
                        lexer *l = new_lexer(new_value, model_config->model_name);
                        parser *p = new_parser(l);
                        program program = parse_program(p);
                    
                    check_parser_errors(p, true);
                    
                    printf("Changing value of variable %s from %s to %s for model %s\n",
                           var_name, ast_to_string(a->assignement_stmt.value),
                           ast_to_string(program[0]), parent_model_config->model_name);
                    
                    free(a->assignement_stmt.value);
                    a->assignement_stmt.value = program[0];
                } else {
                    printf("%s = %s for model %s\n", var_name, ast_to_string(a->assignement_stmt.value), model_config->model_name);
                }
                break;
            }
        }
    }
    
    if(i == n ) {
        printf ("Error parsing command %s. Invalid variable name: %s. You can list model variable name using vars %s\n",
                command, var_name, model_config->model_name);
    }
    else {
        if(set) {
            printf("Reloading model %s as %s\n", parent_model_config->model_name, model_config->model_name);
            execute_load_command(shell_state, NULL, model_config);
        }
    }
}

static void execute_get_values_command(struct shell_variables *shell_state, sds *tokens, int num_args, ast_tag tag) {
    
    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 0);
    
    if(!model_config) return;
    
    int n = arrlen(model_config->program);
    
    printf("Model %s: ", model_config->model_name);
    
    bool empty = true;
    
    for(int i = 0; i < n; i++) {
        ast *a = model_config->program[i];
        if(a->tag == tag) {
            printf("\n%s = %s", a->assignement_stmt.name->identifier.value, ast_to_string(a->assignement_stmt.value));
            empty = false;
        }
    }
    
    if(empty) {
        printf("No values to show");
    }
    
    printf("\n");
}

void execute_save_plot_command(const char *command, struct shell_variables *shell_state, char *file_name) {
    
    if(shell_state->gnuplot_handle == NULL) {
        printf ("Error executing command %s. No previous plot. plot the model first using \"plot modelname\" or list loaded models using \"list\"\n", command);
        return;
    }
    
    const char *ext = get_filename_ext(file_name);
    
    if(!FILE_HAS_EXTENSION(ext, "pdf") && !FILE_HAS_EXTENSION(ext, "png")) {
        printf("Error executing command %s. Only .pdf and .png outputs are supported\n", command);
        return;
    }
    
    gnuplot_cmd(shell_state->gnuplot_handle, "set terminal %s", ext);
    gnuplot_cmd(shell_state->gnuplot_handle, "set output \"%s", file_name);
    gnuplot_cmd(shell_state->gnuplot_handle, "replot");
    
    reset_terminal(shell_state->gnuplot_handle, shell_state->default_gnuplot_term);
}

void execute_set_current_model_command(struct shell_variables *shell_state, sds *tokens, int num_args) {
    
    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 0);
    
    if(!model_config) return;
    
    
    shell_state->current_model = model_config;
    
    printf("Current model set to %s\n", shell_state->current_model->model_name);
    
}

void execute_solve_plot_command(struct shell_variables *shell_state, sds *tokens, int num_args, command_type c_type) {
    
    bool success = execute_solve_command(shell_state, tokens, num_args);
    
    if(!success) return;
    
    char *new_tokens[2];
    new_tokens[0] = "plot";
    
    if(num_args == 2) {
        new_tokens[1] = tokens[1];
    }
    
    execute_plot_command(shell_state, new_tokens, c_type, num_args-1);
    
}

void execute_print_model_command(struct shell_variables *shell_state, sds *tokens, int num_args) {
    
    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 0);
    
    if(!model_config) return;
    
    printf("\n");
    print_program(model_config->program);
    printf("\n");
    
}

void execute_edit_model_command(struct shell_variables *shell_state, sds *tokens, int num_args) {

    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 0);

    if(!model_config) return;

    sds cmd = sdscatfmt(sdsempty(), "xdg-open %s.ode", model_config->model_name);

    FILE *f = popen(cmd, "r");
    if(!f) {
        printf("Error - xdg-open is not in your path or is not installed\n");
    }
    pclose(f);

}

void execute_unload_model_command(struct shell_variables *shell_state, sds *tokens, int num_args) {
    
    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 0);
    if(!model_config) return;
    
    bool is_current = (model_config == shell_state->current_model);
    
    free_model_config(model_config);
    shdel(shell_state->loaded_models, tokens[1]);
    
    if(shell_state->loaded_models != 0 && is_current) {
        //TODO: do we want to know the real current model??
        shell_state->current_model = shell_state->loaded_models[0].value;
    }
}

void execute_set_reload_command(struct shell_variables *shell_state, sds *tokens, int num_args, command_type cmd_type) {

    char *command = tokens[0];
    char *arg = tokens[1];
    bool is_zero;

    if(STR_EQUALS(tokens[1], "0")) {
        is_zero = true;
    }
    else if(STR_EQUALS(tokens[1], "1")) {
        is_zero = false;
    }
    else {
        printf("Error - Invalid value %s for command %s. Valid values are 0 or 1\n", tokens[1], tokens[0]);
        return;
    }

    struct model_config *model_config;

    if(cmd_type == CMD_SET_GLOBAL_RELOAD) {
        shell_state->never_reload = is_zero;
    }
    else {
        model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 1);

        if(cmd_type == CMD_SET_RELOAD) {
            model_config->should_reload = !is_zero;
        }
        else if(cmd_type == CMD_SET_AUTO_RELOAD) {
            model_config->auto_reload = !is_zero;

            if(!is_zero) {
               model_config->should_reload = true;
            }
        }
    }

}

static bool parse_and_execute_command(sds line, struct shell_variables *shell_state) {
    
    int  num_args, token_count;
    
    sds *tokens = sdssplitargs(line, &token_count);
    
    if(!tokens) {
        printf("Error parsing command line!\n");
        goto dealloc_vars;
    }
    
    num_args = token_count - 1;
    
    command command = shgets(commands, tokens[0]);
    command_type c_type = command.value;
    
    if(c_type == CMD_INVALID) {
        printf("Invalid command: %s\n", tokens[0]);
        goto dealloc_vars;
    }
    
    CHECK_2_ARGS(command.key, command.accept[0], command.accept[1], num_args);

    pthread_mutex_lock(&shell_state->lock);

    switch(c_type) {
        case CMD_INVALID:
            //should never happens as we return on invalid command
            break;
        case CMD_QUIT:
            //Exit is handled by the main loop
            return true;
        case CMD_LOAD:
            execute_load_command(shell_state, tokens[1], NULL);
            break;
        case CMD_UNLOAD:
            execute_unload_model_command(shell_state, tokens, num_args);
            break;
        case CMD_SOLVE:
            execute_solve_command(shell_state, tokens, num_args);
            break;
        case CMD_SOLVE_PLOT:
            execute_solve_plot_command(shell_state, tokens, num_args, c_type);
            break;
        case CMD_PLOT:
        case CMD_REPLOT:
        case CMD_PLOT_FILE:
        case CMD_REPLOT_FILE:
        case CMD_PLOT_TERM:
        case CMD_REPLOT_TERM:
            execute_plot_command(shell_state, tokens, c_type, num_args);
            break;
        case CMD_LIST:
            execute_list_command(shell_state);
            break;
        case CMD_VARS:
            execute_vars_command(shell_state, tokens, num_args);
            break;
        case CMD_PLOT_SET_X:
        case CMD_PLOT_SET_Y:
        case CMD_PLOT_SET_X_LABEL:
        case CMD_PLOT_SET_Y_LABEL:
        case CMD_PLOT_SET_TITLE:
            execute_setplot_command(shell_state,  tokens, c_type, num_args);
            break;
        case CMD_CD:
            execute_cd_command(shell_state, tokens[1]);
            break;
        case CMD_PWD:
            print_current_dir();
            break;
        case CMD_LOAD_CMDS:
            run_commands_from_file(tokens[1], shell_state);
            break;
        case CMD_LS:
            execute_ls_command(tokens[1], num_args);
            break;
        case CMD_HELP:
            execute_help_command(tokens, num_args);
            break;
        case CMD_GET_PLOT_CONFIG:
            execute_getplotconfig_command(shell_state, tokens, num_args);
            break;
        case CMD_SET_INITIAL_VALUE:
            execute_set_or_get_value_command(shell_state, tokens, num_args, ast_initial_stmt, true);
            break;
        case CMD_GET_INITIAL_VALUE:
            execute_set_or_get_value_command(shell_state, tokens, num_args, ast_initial_stmt, false);
            break;
        case CMD_SET_PARAM_VALUE:
            execute_set_or_get_value_command(shell_state, tokens, num_args, ast_assignment_stmt, true);
            break;
        case CMD_GET_PARAM_VALUE:
            execute_set_or_get_value_command(shell_state, tokens, num_args, ast_assignment_stmt, false);
            break;
        case CMD_SET_GLOBAL_VALUE:
            execute_set_or_get_value_command(shell_state, tokens, num_args, ast_global_stmt, true);
            break;
        case CMD_GET_GLOBAL_VALUE:
            execute_set_or_get_value_command(shell_state, tokens, num_args, ast_global_stmt, false);
            break;
        case CMD_GET_INITIAL_VALUES:
            execute_get_values_command(shell_state, tokens, num_args, ast_initial_stmt);
            break;
        case CMD_GET_PARAM_VALUES:
            execute_get_values_command(shell_state, tokens, num_args, ast_assignment_stmt);
            break;
        case CMD_GET_GLOBAL_VALUES:
            execute_get_values_command(shell_state, tokens, num_args, ast_global_stmt);
            break;
        case CMD_SET_ODE_VALUE:
            execute_set_or_get_value_command(shell_state, tokens, num_args, ast_ode_stmt, true);
            break;
        case CMD_GET_ODE_VALUE:
            execute_set_or_get_value_command(shell_state, tokens, num_args, ast_ode_stmt, false);
            break;
        case CMD_GET_ODE_VALUES:
            execute_get_values_command(shell_state, tokens, num_args, ast_ode_stmt);
            break;
        case CMD_SAVEPLOT:
            execute_save_plot_command(tokens[0], shell_state, tokens[1]);
            break;
        case CMD_SET_CURRENT_MODEL:
            execute_set_current_model_command(shell_state, tokens, num_args);
            break;
        case CMD_PRINT_MODEL:
            execute_print_model_command(shell_state, tokens, num_args);
            break;
        case CMD_EDIT_MODEL:
            execute_edit_model_command(shell_state, tokens, num_args);
            break;
        case CMD_SET_GLOBAL_RELOAD:
        case CMD_SET_RELOAD:
        case CMD_SET_AUTO_RELOAD:
            execute_set_reload_command(shell_state, tokens, num_args, c_type);
    }

    dealloc_vars:
    {
        if(tokens)
            sdsfreesplitres(tokens, token_count);

        pthread_mutex_unlock(&shell_state->lock);

        return false;
    }

    pthread_mutex_unlock(&shell_state->lock);
    sdsfreesplitres(tokens, token_count);
    return false;
    
}

static void setup_ctrl_c_handler() {
    /* Setup SIGINT */
    struct sigaction s;
    s.sa_handler = ctrl_c_handler;
    sigemptyset(&s.sa_mask);
    s.sa_flags = SA_RESTART;
    sigaction(SIGINT, &s, NULL);
}


void get_default_gnuplot_terminal(struct shell_variables *shell_state) {
    FILE *f = popen("gnuplot -e \"show t\" 2>&1", "r");
    
    char msg[PATH_MAX];
    
    sds tmp;
    
    while (fgets(msg, PATH_MAX, f) != NULL) {
        int n = strlen(msg);
        if(n > 5) {
            tmp = sdsnew(msg);
            tmp = sdstrim(tmp, " \n");
            break;
        }
    }
    
    int c;
    sds *tmp_tokens = sdssplit(tmp, " ", &c);
    shell_state->default_gnuplot_term = strdup(tmp_tokens[3]);
    fclose(f);
    
}

int main(int argc, char **argv) {
    
    initialize_commands();
    
    print_current_dir();
    
    rl_attempted_completion_function = command_completion;
    
    struct shell_variables shell_state = {0};

    shell_state.current_dir = get_current_directory();
    shell_state.never_reload = false;

    shdefault(shell_state.loaded_models, NULL);
    sh_new_strdup(shell_state.loaded_models);

    get_default_gnuplot_terminal(&shell_state);

    //Setting up inotify
    shell_state.fd_notify = inotify_init();

    pthread_t inotify_thread;

    if (pthread_mutex_init(&shell_state.lock, NULL) != 0) {
        printf("\n mutex init has failed\n");
        return EXIT_FAILURE;
    }

    pthread_create(&inotify_thread, NULL, check_for_model_file_changes, (void*)&shell_state);

    if(argc == 2) {
        run_commands_from_file(argv[1], &shell_state);
    }
    
    sds history_path = sdsnew(get_home_dir());
    history_path = sdscatfmt(history_path, "/%s", HISTORY_FILE);
    
    read_history(history_path);
    
    setup_ctrl_c_handler();
    
    char *line;
    
    //if CTRL+C is pressed, we print a new line
    if (sigsetjmp(env, 1) == 42) {
        printf("\n");
    }
    
    bool quit;
    
    while ((line = readline(PROMPT)) != 0) {
        
        if(!line[0] || line[0] == '#' || line[0] == '\n') continue;
        add_history(line);
        quit = parse_and_execute_command(line, &shell_state);
        if(quit) break;
    }
    
    clean_and_exit(&shell_state);
    
}

