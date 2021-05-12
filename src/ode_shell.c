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

#include "code_converter.h"
#include "commands.h"
#include "file_utils/file_utils.h"
#include "stb/stb_ds.h"
#include "string/sds.h"
#include "string_utils.h"

#include "ode_shell.h"

extern command *commands;

static bool parse_and_execute_command(sds line, struct shell_variables *shell_state);

static sigjmp_buf env;
static void ctrl_c_handler(int sig) {
    siglongjmp(env, 42);
}

static void gnuplot_cmd(FILE *handle, char const *cmd, ...) {
  va_list ap;

  va_start(ap, cmd);
  vfprintf(handle, cmd, ap);
  va_end(ap);

  fputs("\n", handle);
  fflush(handle);

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

static bool generate_program(struct model_config *model) {

    char *file_name = model->model_file;

    if(!file_exists(file_name)) {
        printf("Error: file %s does not exist!\n", file_name);
        return true;
    }

    size_t file_size;
    const char *source = read_entire_file_with_mmap(file_name, &file_size);

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

    return error;

}

static struct model_config *load_model_config(struct shell_variables *shell_state, sds *tokens, int num_args, int model_name_position) {

    char *command = tokens[0];

    const char *model_name;

    if(num_args == model_name_position) {
        return shell_state->last_loaded_model;
    }
    else {
        model_name = tokens[1];
    }

    if(shlen(shell_state->loaded_models) == 0) {
        printf ("Error executing command %s. No models loaded. Load a model first using load modelname.edo\n", command);
        return NULL;
    }

    struct model_config *model_config = shget(shell_state->loaded_models, model_name);

    if(!model_config) {
        printf ("Error executing command %s. model %s is not loaded. Load a model first using \"load %s.edo\" or list loaded models using \"list\"\n", command, model_name, model_name);
        return NULL;
    }

    return model_config;

}

static char * get_var_name_from_index(struct var_index_hash_entry *var_indexes, int index) {
    int len = shlen(var_indexes);

    for(int i = 0; i < len; i++) {
        if(var_indexes[i].value == index)
            return var_indexes[i].key;
    }
    return NULL;
}

static void run_commands_from_file(char *file_name, struct shell_variables *shell_state) {
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
                command = sdsnew(line);
                command = sdstrim(command, "\n ");
                add_history(command);
                parse_and_execute_command(command, shell_state);
                printf("%s%s\n", PROMPT, command);
                sdsfree(command);
            }

            fclose(f);
            if (line)
                free(line);
        }
    }
}

static void execute_load_command(struct shell_variables *shell_state, const char *model_file, bool reloding) {

    struct model_config *model_config;
    bool error = false;

    if(reloding) {
        model_config = shell_state->last_loaded_model;
    }
    else {
        model_config = calloc(1, sizeof(struct model_config));
        char *model_name = get_filename_without_ext(model_file);
        model_config->model_name = strdup(model_name);
        model_config->model_file = strdup(model_file);
        error = generate_program(model_config);

        if(error) return;

        model_config->xindex = 1;
        model_config->yindex = 2;

        model_config->xlabel = strdup(get_var_name_from_index(model_config->var_indexes, 1));
        model_config->ylabel = strdup(get_var_name_from_index(model_config->var_indexes, 2));

        sds compiled_model_name = sdscatfmt(sdsempty(), "%s_auto_compiled_model_tmp_file", model_config->model_name);
        model_config->model_command = compiled_model_name;

        shput(shell_state->loaded_models, model_config->model_name, model_config);
        shell_state->last_loaded_model = model_config;

    }

    sds compiled_file = sdsnew(model_config->model_name);
    compiled_file = sdscat(compiled_file, "_XXXXXX.c");

    int fd = mkstemps(compiled_file, 2);

    FILE *outfile = fdopen(fd, "w");
    convert_to_c(model_config->program, outfile, EULER_ADPT_SOLVER);
    fclose(outfile);

    sds compiler_command = sdsnew("gcc");
    compiler_command = sdscatfmt(compiler_command, " %s -o %s -lm", compiled_file, model_config->model_command);

    FILE *fp = popen(compiler_command, "r");
    error = check_and_print_execution_errors(fp);

    unlink(compiled_file);

    //Clean
    pclose(fp);
    sdsfree(compiled_file);
    sdsfree(compiler_command);

}

static void execute_run_command(struct shell_variables *shell_state, sds *tokens, int num_args) {
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
        printf ("Error parsing command run. Invalid number: %s\n", simulation_steps_str);
        return;
    }

    struct model_config *model_config = load_model_config(shell_state, tokens, num_args, 1);

    if(!model_config) return;

    sds model_out_file = sdscatfmt(sdsempty(), "%s_out.txt", model_config->model_command);
    model_config->output_file = model_out_file;

    sds model_command = sdsnew("./");
    model_command = sdscat(model_command, model_config->model_command);

    model_command = sdscatprintf(model_command, " %lf %s", simulation_steps, model_config->output_file);

    FILE *fp = popen(model_command, "r");
    check_and_print_execution_errors(fp);
    pclose(fp);

    sdsfree(model_command);
}

static void execute_plot_command(struct shell_variables *shell_state, sds *tokens, command_type c_type, int num_args) {

    const char *command = tokens[0];
    struct model_config *model_config = load_model_config(shell_state, tokens, num_args, 0);

    if(!model_config) return;

    if(!model_config->output_file) {
        printf ("Error executing command %s. Model %s was not executed. Run then model first using \"run %s\" or list loaded models using \"list\"\n", command, model_config->model_name, model_config->model_name);
        return;
    }

    if(shell_state->gnuplot_handle == NULL) {
        if(c_type == CMD_REPLOT) {
            printf ("Error executing command %s. No previous plot. plot the model first using \"plot modelname\" or list loaded models using \"list\"\n", command);
            return;
        }

        shell_state->gnuplot_handle = popen("gnuplot", "w");
    }

    gnuplot_cmd(shell_state->gnuplot_handle, "set xlabel \"%s\"", model_config->xlabel);
    gnuplot_cmd(shell_state->gnuplot_handle, "set ylabel \"%s\"", model_config->ylabel);
    gnuplot_cmd(shell_state->gnuplot_handle, "%s '%s' u %d:%d title \"%s\" w lines lw 2", command, model_config->output_file, model_config->xindex, model_config->yindex, model_config->ylabel);
}

static void execute_setplot_command(struct shell_variables *shell_state, sds *tokens, command_type c_type, int num_args) {

    const char *command = tokens[0];

    struct model_config *model_config = load_model_config(shell_state, tokens, num_args, 1);

    if(!model_config) return;

    char *index_str;

    if(num_args == 1) {
        index_str = tokens[1];
    }
    else {
        index_str = tokens[2];
    }

    bool index_as_str;
    int index = (int)string_to_long(index_str, &index_as_str);

    if(index_as_str) {
        //string was passed, try to get the index from the var_indexes on model_config
        index = shget(model_config->var_indexes, index_str);

        if(index == -1) {
            printf ("Error parsing command %s. Invalid variable name: %s. You can list model variable name using vars %s\n", command, index_str, model_config->model_name);
            return;
        }
    }

    char *var_name = NULL;

    if(!index_as_str) {

        var_name = get_var_name_from_index(model_config->var_indexes, index);

        if(!var_name) {
            printf ("Error parsing command %s. Invalid index: %d. You can list model variable name using vars %s\n", command, index, model_config->model_name);
            return;

        }
    }

    if(c_type == CMD_PLOT_SET_X) {
        model_config->xindex = index;
        if(index_as_str) {
            model_config->xlabel = strdup(index_str);
        }
        else {
            model_config->xlabel = strdup(var_name);
        }
    }
    else {
        model_config->yindex = index;
        if(index_as_str) {
            model_config->ylabel = strdup(index_str);
        }
        else {
            model_config->ylabel = strdup(var_name);
        }
    }
}

static void execute_list_command(struct shell_variables *shell_state) {

    int len = shlen(shell_state->loaded_models);

    if(len == 0) {
        printf ("No models loaded\n");
        return;
    }

    printf("Last loaded model: %s\n", shell_state->last_loaded_model->model_name);
    printf("Loaded models:\n");
    for(int i = 0; i < len; i++) {
        printf("%s\n", shell_state->loaded_models[i].key);
    }

}

static void execute_vars_command(struct shell_variables *shell_state, sds *tokens, int num_args) {

    struct model_config *model_config = load_model_config(shell_state, tokens, num_args, 0);

    if(!model_config) return;

    int len = shlen(model_config->var_indexes);

    printf("Model vars for model %s:\n", model_config->model_name);
    for(int i = 0; i < len; i++) {
        printf("%s, %d\n", model_config->var_indexes[i].key, model_config->var_indexes[i].value);
    }

    return;

}

static void execute_cd_command(const char *path) {

    int ret = chdir(path);
    if(ret == -1) {
        printf("Error changing working directory to %s\n", path);
    }
    else {
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
        int nc = shlen(commands);
        for(int i = 0; i < nc; i++) {
            printf("%s\n", commands[i].key);
        }
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

    struct model_config *model_config = load_model_config(shell_state, tokens, num_args, 0);

    if(!model_config) return;

    char *xname = get_var_name_from_index(model_config->var_indexes, model_config->xindex);
    char *yname = get_var_name_from_index(model_config->var_indexes, model_config->yindex);

    printf("Plot configuration for model %s\n\n", model_config->model_name);

    printf("Graph X: %s (%d)\n", xname, model_config->xindex);
    printf("Graph Y: %s (%d)\n", yname, model_config->yindex);
    //TODO: set gnuplot labels
    return;

}

static void execute_set_or_get_value_command(struct shell_variables *shell_state, sds *tokens, int num_args, ast_tag tag, bool set) {

    const char *command = tokens[0];
    char *var_name;
    char *new_value;

    struct model_config *model_config;

    if(set) {
        if(num_args == 2) {
            var_name = tokens[1];
            new_value = tokens[2];
        }
        else {
            var_name = tokens[2];
            new_value = tokens[3];
        }

        model_config = load_model_config(shell_state, tokens, num_args, 2);
    } else {
        if(num_args == 1) {
            var_name = tokens[1];
        }
        else {
            var_name = tokens[2];
        }

        model_config = load_model_config(shell_state, tokens, num_args, 1);

    }

    if(!model_config) return;

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
                            ast_to_string(program[0]), model_config->model_name);

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
            printf("Reloading model %s\n", model_config->model_name);
            execute_load_command(shell_state, NULL, true);
        }
    }

}

static void execute_get_values_command(struct shell_variables *shell_state, sds *tokens, int num_args, ast_tag tag) {

    struct model_config *model_config = load_model_config(shell_state, tokens, num_args, 0);

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

void execute_command_save_plot(const char *command, struct shell_variables *shell_state, char *file_name) {

    if(shell_state->gnuplot_handle == NULL) {
        printf ("Error executing command %s. No previous plot. plot the model first using \"plot modelname\" or list loaded models using \"list\"\n", command);
        return;
    }

    //TODO: check if the name ends with pdf
    gnuplot_cmd(shell_state->gnuplot_handle, "set terminal pdf");
    gnuplot_cmd(shell_state->gnuplot_handle, "set output \"%s", file_name);
    gnuplot_cmd(shell_state->gnuplot_handle, "replot");

    //TODO: get default terminal using "show t"
    gnuplot_cmd(shell_state->gnuplot_handle, "set terminal qt");

}

static bool parse_and_execute_command(sds line, struct shell_variables *shell_state) {

    int  num_args;
    int token_count;

    sds *tokens = sdssplit(line, " ", &token_count);

    for(int i = 0; i < token_count; i++) {
        tokens[i] = sdstrim(tokens[i], " ");
    }

    num_args = token_count - 1;

    command command = shgets(commands, tokens[0]);
    command_type c_type = command.value;

    if(c_type == CMD_INVALID) {
        printf("Invalid command: %s\n", tokens[0]);
        goto dealloc_vars;
    }

    CHECK_2_ARGS(command.key, command.accept[0], command.accept[1], num_args);

    if(c_type == CMD_QUIT) {
        //Exit is handled by the main loop
        return true;
    }
    else if(c_type == CMD_LOAD) {
        execute_load_command(shell_state, tokens[1], false);
    }
    else if(c_type == CMD_RUN) {
        execute_run_command(shell_state, tokens, num_args);
    }
    else if(c_type == CMD_PLOT || c_type == CMD_REPLOT) {
        execute_plot_command(shell_state, tokens, c_type, num_args);
    } else if(c_type == CMD_LIST) {
        execute_list_command(shell_state);
    } else if(c_type == CMD_VARS) {
        execute_vars_command(shell_state, tokens, num_args);
    } else if(c_type == CMD_PLOT_SET_X || c_type == CMD_PLOT_SET_Y) {
        execute_setplot_command(shell_state,  tokens, c_type, num_args);
    } else if(c_type == CMD_CD) {
        execute_cd_command(tokens[1]);
    } else if(c_type == CMD_PWD) {
        print_current_dir();
    } else if(c_type == CMD_LOAD_CMDS) {
        run_commands_from_file(tokens[1], shell_state);
    } else if(c_type == CMD_LS) {
        execute_ls_command(tokens[1], num_args);
    } else if(c_type == CMD_HELP) {
        execute_help_command(tokens, num_args);
    } else if(c_type == CMD_GET_PLOT_CONFIG) {
        execute_getplotconfig_command(shell_state, tokens, num_args);
    } else if(c_type == CMD_SET_INITIAL_VALUE) {
        execute_set_or_get_value_command(shell_state, tokens, num_args, ast_initial_stmt, true);
    } else if(c_type == CMD_GET_INITIAL_VALUE) {
        execute_set_or_get_value_command(shell_state, tokens, num_args, ast_initial_stmt, false);
    } else if(c_type == CMD_SET_PARAM_VALUE) {
        execute_set_or_get_value_command(shell_state, tokens, num_args, ast_assignment_stmt, true);
    } else if(c_type == CMD_GET_PARAM_VALUE) {
        execute_set_or_get_value_command(shell_state, tokens, num_args, ast_assignment_stmt, false);
    } else if(c_type == CMD_SET_GLOBAL_VALUE) {
        execute_set_or_get_value_command(shell_state, tokens, num_args, ast_global_stmt, true);
    } else if(c_type == CMD_GET_GLOBAL_VALUE) {
        execute_set_or_get_value_command(shell_state, tokens, num_args, ast_global_stmt, false);
    } else if(c_type == CMD_GET_INITIAL_VALUES) {
        execute_get_values_command(shell_state, tokens, num_args, ast_initial_stmt);
    } else if(c_type == CMD_GET_PARAM_VALUES) {
        execute_get_values_command(shell_state, tokens, num_args, ast_assignment_stmt);
    } else if(c_type == CMD_GET_GLOBAL_VALUES) {
        execute_get_values_command(shell_state, tokens, num_args, ast_global_stmt);
    } else if(c_type == CMD_SET_ODE_VALUE) {
        execute_set_or_get_value_command(shell_state, tokens, num_args, ast_ode_stmt, true);
    } else if(c_type == CMD_GET_ODE_VALUE) {
        execute_set_or_get_value_command(shell_state, tokens, num_args, ast_ode_stmt, false);
    } else if(c_type == CMD_GET_ODE_VALUES) {
        execute_get_values_command(shell_state, tokens, num_args, ast_ode_stmt);
    }
    else if(c_type == CMD_SAVEPLOT) {
        execute_command_save_plot(tokens[0], shell_state, tokens[1]);
    }

    dealloc_vars:
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

void strip_extra_spaces(char* str) {
    int i, x;
    for(i=x=0; str[i]; ++i)
        if(!isspace(str[i]) || (i > 0 && !isspace(str[i-1])))
            str[x++] = str[i];
    str[x] = '\0';
}

int main(int argc, char **argv) {

    initialize_commands();

    print_current_dir();

    rl_attempted_completion_function = command_completion;

    struct shell_variables shell_state = {0};

    shdefault(shell_state.loaded_models, NULL);
    sh_new_strdup(shell_state.loaded_models);

    if(argc == 2) {
        run_commands_from_file(argv[1], &shell_state);
    }

    sds history_path = sdsnew(get_home_dir());
    history_path = sdscatfmt(history_path, "/%s", HISTORY_FILE);

    read_history(history_path);

    setup_ctrl_c_handler();

    sds command;
    char *line;

    //if CTRL+C is pressed, we print a new line
    if (sigsetjmp(env, 1) == 42) {
        printf("\n");
    }

    bool quit;

    while ((line = readline(PROMPT)) != 0) {
        if(*line) {
            strip_extra_spaces(line);
            command = sdsnew(line);
            command = sdstrim(command, "\n ");
            add_history(command);
            quit = parse_and_execute_command(command, &shell_state);
            sdsfree(command);
            if(quit) break;
        }
    }

    int n_models = shlen(shell_state.loaded_models);
    for(int i = 0; i < n_models; i++) {
        struct model_config *config = shell_state.loaded_models[i].value;
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

