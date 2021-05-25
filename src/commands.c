#include "commands.h"
#include "file_utils/file_utils.h"
#include "model_config.h"
#include "stb/stb_ds.h"
#include "code_converter.h"
#include "to_latex.h"

#include <linux/limits.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <math.h>

#include "hash/meow_hash_x64_aesni.h"

static command *commands = NULL;
static string_array commands_sorted = NULL;

#define PRINT_NO_MODELS_LOADED_ERROR(command) printf("Error executing command %s. No models loaded. Load a model first using load modelname.edo\n", command);

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

static bool have_gnuplot(struct shell_variables *shell_state) {
    if (!shell_state->default_gnuplot_term) {
        printf("Error - gnuplot not installed or not in path\n");
        return false;
    }

    return true;
}

static struct model_config *load_model_config_or_print_error(struct shell_variables *shell_state, const char *command, const char *model_name) {

    if (shlen(shell_state->loaded_models) == 0) {
        PRINT_NO_MODELS_LOADED_ERROR(command);
        return NULL;
    }

    struct model_config *model_config = shget(shell_state->loaded_models, model_name);

    if (!model_config) {
        printf("Error executing command %s. model %s is not loaded. Load a model first using \"load %s.edo\" or list loaded models using \"list\"\n", command, model_name, model_name);
        return NULL;
    }

    return model_config;
}


static bool check_and_print_execution_errors(FILE *fp) {

    bool error = false;
    char msg[PATH_MAX];

    while (fgets(msg, PATH_MAX, fp) != NULL) {
        printf("%s", msg);
        if (!error) error = true;
    }

    return error;
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

static char *autocomplete_command(const char *text, int state) {

    static string_array matches = NULL;
    static size_t match_index = 0;

    if (state == 0) {
        arrsetlen(matches, 0);
        match_index = 0;

        int len = shlen(commands);

        sds textstr = sdsnew(text);
        for (int i = 0; i < len; i++) {
            char *word = (char *) commands[i].key;
            size_t wlen = strlen(word);
            size_t tlen = strlen(textstr);

            if (wlen >= sdslen(textstr) && strncmp(word, textstr, tlen) == 0) {
                arrput(matches, word);
            }
        }
    }

    if (match_index >= arrlen(matches)) {
        return NULL;
    } else {
        return strdup(matches[match_index++]);
    }
}

static char **command_completion(const char *text, int start, int end) {
    (void) start;
    (void) end;
    return rl_completion_matches(text, autocomplete_command);
}

static bool check_command_number_argument(const char *command, int expected_args, int num_args) {

    if (expected_args != num_args) {
        printf("Error: command %s accept %d argument(s). %d argument(s) given!\n", command, expected_args, num_args);
        return false;
    }
    return true;

}

static struct model_config * get_model_and_n_runs_for_plot_cmds(struct shell_variables *shell_state, sds *tokens, int num_args, int min_args, uint *run_number) {

    bool error = false;
    struct model_config *model_config = NULL;

    if(num_args == min_args) {
        if(!shell_state->current_model) {
            PRINT_NO_MODELS_LOADED_ERROR(tokens[0]);
            return NULL;
        }
        model_config = shell_state->current_model;
        *run_number = model_config->num_runs;
    }
    else if(num_args == min_args + 1) {
        *run_number = string_to_long(tokens[1], &error);

        if(!error) {
            //the first argument is run number, so the model is the default
            if(!shell_state->current_model) {
                PRINT_NO_MODELS_LOADED_ERROR(tokens[0]);
                return NULL;
            }

            model_config = shell_state->current_model;
        }
        else {
            //the first argument is the model name, so the run number is the last one
            model_config = load_model_config_or_print_error(shell_state, tokens[0], tokens[1]);
            if(!model_config) return NULL;
            *run_number = model_config->num_runs;
        }
    }
    else if(num_args == min_args + 2) {
        model_config = load_model_config_or_print_error(shell_state, tokens[0], tokens[1]);

        if(!model_config) return NULL;
        *run_number = string_to_long(tokens[2], &error);

        if(error) {
            printf("Error parsing command %s. Invalid number: %s\n", tokens[0], tokens[2]);
        }

    }

    if(model_config) {

        if(model_config->num_runs == 0) {
            printf("Error executing command %s. Model %s was not executed. Run then model first using \"solve %s\" or list loaded models using \"list\"\n", tokens[0], model_config->model_name, model_config->model_name);
            return NULL;
        }
        else if(*run_number > model_config->num_runs) {
            printf("Error running command %s. The model was executed %u time(s), but it was requested to plor run %u!\n", tokens[0], model_config->num_runs, *run_number);
            return NULL;
        }
    }

    return model_config;

}

static bool load_model(struct shell_variables *shell_state, const char *model_file, struct model_config *model_config) {

    bool error = false;
    bool new_model = (model_config == NULL);

    if (new_model) {
        model_config = calloc(1, sizeof(struct model_config));

        model_config->should_reload = true;
        model_config->auto_reload = false;

        const char *ext = get_filename_ext(model_file);

        sds new_file = sdsnew(model_file);

        if (ext == NULL) {
            //we try to add .ode and find the file again
            new_file = sdscat(new_file, ".ode");
        }

        char *model_name = get_filename_without_ext(model_file);
        model_config->model_name = strdup(model_name);

        sds full_model_file_path;

        if (model_file[0] != '/') {
            full_model_file_path = sdsnew(shell_state->current_dir);
        } else {
            full_model_file_path = sdsempty();
        }

        full_model_file_path = sdscatfmt(full_model_file_path, "%s", new_file);
        model_config->model_file = strdup(full_model_file_path);
        sdsfree(full_model_file_path);
        sdsfree(new_file);

        error = generate_model_program(model_config);

        if (error) return false;

        model_config->plot_config.xindex = 1;
        model_config->plot_config.yindex = 2;

        if (!model_config->plot_config.xlabel) {
            model_config->plot_config.xlabel = strdup(get_var_name(model_config, 1));
        }

        if (!model_config->plot_config.ylabel) {
            model_config->plot_config.ylabel = strdup(get_var_name(model_config, 2));
        }

        if (!model_config->plot_config.title) {
            model_config->plot_config.title = strdup(model_config->plot_config.ylabel);
        }
    }

    model_config->is_derived = !new_model;

    //TODO: handle errors
    error = compile_model(model_config);

    shput(shell_state->loaded_models, model_config->model_name, model_config);
    shell_state->current_model = model_config;

    if (new_model) {
        add_file_watch(shell_state, get_dir_from_path(model_config->model_file));
    }

    return true;
}

COMMAND_FUNCTION(load) {
    return load_model(shell_state, tokens[1], NULL);
}

COMMAND_FUNCTION(solve) {

    double simulation_steps = 0;

    char *simulation_steps_str = tokens[num_args];

    simulation_steps = string_to_double(simulation_steps_str);

    if (isnan(simulation_steps) || simulation_steps == 0) {
        printf("Error parsing command %s. Invalid number: %s\n", tokens[0], simulation_steps_str);
        return false;
    }

    struct model_config *model_config = NULL;

    if(num_args == 1) {
        model_config = shell_state->current_model;
    }
    else {
        model_config = load_model_config_or_print_error(shell_state, tokens[0], tokens[1]);
    }

    if (!model_config) return false;

    model_config->num_runs++;

    sds output_file = get_model_output_file(model_config, model_config->num_runs);

    sds model_command = sdscat(sdsempty(), model_config->model_command);

    model_command = sdscatprintf(model_command, " %lf %s", simulation_steps, output_file);

    FILE *fp = popen(model_command, "r");
    check_and_print_execution_errors(fp);
    pclose(fp);

    sdsfree(model_command);
    sdsfree(output_file);

    return true;
}

static bool plot_helper(struct shell_variables *shell_state, sds *tokens, command_type c_type, int num_args) {

    if (!have_gnuplot(shell_state)) return false;

    const char *command = tokens[0];
    uint run_number;
    struct model_config *model_config = get_model_and_n_runs_for_plot_cmds(shell_state, tokens, num_args, 0, &run_number);

    if (!model_config) return false;

    if (shell_state->gnuplot_handle == NULL) {

        if (c_type == CMD_REPLOT) {
            printf("Error executing command %s. No previous plot. plot the model first using \"plot modelname\" or list loaded models using \"list\"\n", command);
            return false;
        }

        shell_state->gnuplot_handle = popen("gnuplot -persistent", "w");
    }

    if (c_type == CMD_PLOT_TERM || c_type == CMD_REPLOT_TERM) {
        gnuplot_cmd(shell_state->gnuplot_handle, "set term dumb");
        if (c_type == CMD_PLOT_TERM) {
            command = "plot";
        } else if(c_type == CMD_REPLOT_TERM) {
            command = "replot";
        }
    }

    gnuplot_cmd(shell_state->gnuplot_handle, "set xlabel \"%s\"", model_config->plot_config.xlabel);
    gnuplot_cmd(shell_state->gnuplot_handle, "set ylabel \"%s\"", model_config->plot_config.ylabel);

    sds output_file = get_model_output_file(model_config, run_number);
    gnuplot_cmd(shell_state->gnuplot_handle, "%s '%s' u %d:%d title \"%s\" w lines lw 2", command, output_file, model_config->plot_config.xindex, model_config->plot_config.yindex, model_config->plot_config.title);
    sdsfree(output_file);


    if (c_type == CMD_PLOT_TERM || c_type == CMD_REPLOT_TERM) {
        reset_terminal(shell_state->gnuplot_handle, shell_state->default_gnuplot_term);
    }

    return true;
}

COMMAND_FUNCTION(plot) {
    return plot_helper(shell_state, tokens, CMD_PLOT, num_args);
}

COMMAND_FUNCTION(replot) {
    return plot_helper(shell_state, tokens, CMD_REPLOT, num_args);
}

COMMAND_FUNCTION(plottoterm) {
    return plot_helper(shell_state, tokens, CMD_PLOT_TERM, num_args);
}

COMMAND_FUNCTION(replottoterm) {
    return plot_helper(shell_state, tokens, CMD_REPLOT_TERM, num_args);
}

static bool plot_file_helper(struct shell_variables *shell_state, sds *tokens, command_type c_type, int num_args) {

    if (!have_gnuplot(shell_state)) return false;

    const char *command = tokens[0];
    uint run_number;
    struct model_config *model_config = get_model_and_n_runs_for_plot_cmds(shell_state, tokens, num_args, 1, &run_number);

    if (!model_config) return false;

    if (shell_state->gnuplot_handle == NULL) {
        if (c_type == CMD_REPLOT_FILE) {
            printf("Error executing command %s. No previous plot. plot the model first using \"plot modelname\" or list loaded models using \"list\"\n", command);
            return false;
        }

        shell_state->gnuplot_handle = popen("gnuplot -persistent", "w");
    }

    if (c_type == CMD_PLOT_FILE) {
        command = "plot";
    } else {
        command = "replot";
    }

    const char *file_name = tokens[num_args];

    const char *ext = get_filename_ext(file_name);

    if (!FILE_HAS_EXTENSION(ext, "pdf") && !FILE_HAS_EXTENSION(ext, "png")) {
        printf("Error executing command %s. Only .pdf and .png outputs are supported\n", command);
        return false;
    }

    gnuplot_cmd(shell_state->gnuplot_handle, "set terminal %s", ext);
    gnuplot_cmd(shell_state->gnuplot_handle, "set output \"%s", file_name);
    gnuplot_cmd(shell_state->gnuplot_handle, "set xlabel \"%s\"", model_config->plot_config.xlabel);
    gnuplot_cmd(shell_state->gnuplot_handle, "set ylabel \"%s\"", model_config->plot_config.ylabel);

    sds output_file = get_model_output_file(model_config, run_number);
    gnuplot_cmd(shell_state->gnuplot_handle, "%s '%s' u %d:%d title \"%s\" w lines lw 2", command, output_file, model_config->plot_config.xindex, model_config->plot_config.yindex, model_config->plot_config.title);
    sdsfree(output_file);

    reset_terminal(shell_state->gnuplot_handle, shell_state->default_gnuplot_term);

    return true;

}

COMMAND_FUNCTION(plottofile) {
    return plot_file_helper(shell_state, tokens, CMD_PLOT_FILE, num_args);
}

COMMAND_FUNCTION(replottofile) {
    return plot_file_helper(shell_state, tokens, CMD_REPLOT_FILE, num_args);
}

static bool setplot_helper(struct shell_variables *shell_state, sds *tokens, command_type c_type, int num_args) {

    if (!have_gnuplot(shell_state)) return false;

    const char *command = tokens[0];

    struct model_config *model_config = NULL;

    if(num_args == 1) {
        model_config = shell_state->current_model;
    }
    else {
        model_config = load_model_config_or_print_error(shell_state, command, tokens[1]);
    }

    if (!model_config) return false;

    char *cmd_param = tokens[num_args];

    if (c_type == CMD_SET_PLOT_X || c_type == CMD_SET_PLOT_Y) {

        bool index_as_str;
        int index = (int) string_to_long(cmd_param, &index_as_str);
        char *var_name = NULL;

        if (index_as_str) {
            //string was passed, try to get the index from the var_indexes on model_config
            index = shget(model_config->var_indexes, cmd_param);

            if (index == -1) {
                printf("Error parsing command %s. Invalid variable name: %s. You can list model variable name using vars %s\n", command, cmd_param, model_config->model_name);
                return false;
            }
        } else {
            var_name = get_var_name(model_config, index);

            if (!var_name) {
                printf("Error parsing command %s. Invalid index: %d. You can list model variable name using vars %s\n", command, index, model_config->model_name);
                return false;
            }
        }

        if (c_type == CMD_SET_PLOT_X) {
            model_config->plot_config.xindex = index;
        } else {
            model_config->plot_config.yindex = index;

            if (index_as_str) {
                model_config->plot_config.title = strdup(cmd_param);
            } else {
                model_config->plot_config.title = strdup(var_name);
            }
        }
    } else if (c_type == CMD_SET_PLOT_X_LABEL || c_type == CMD_SET_PLOT_Y_LABEL) {

        if (c_type == CMD_SET_PLOT_X_LABEL) {
            model_config->plot_config.xlabel = strdup(cmd_param);
        } else {
            model_config->plot_config.ylabel = strdup(cmd_param);
        }
    } else if (c_type == CMD_SET_PLOT_TITLE) {
        model_config->plot_config.title = strdup(cmd_param);
    }

    return true;
}

COMMAND_FUNCTION(setplotx) {
    return setplot_helper(shell_state, tokens, CMD_SET_PLOT_X, num_args);
}

COMMAND_FUNCTION(setploty) {
    return setplot_helper(shell_state, tokens, CMD_SET_PLOT_Y, num_args);
}

COMMAND_FUNCTION(setplotxlabel) {
    return setplot_helper(shell_state, tokens, CMD_SET_PLOT_X_LABEL, num_args);
}

COMMAND_FUNCTION(setplotylabel) {
    return setplot_helper(shell_state, tokens, CMD_SET_PLOT_Y_LABEL, num_args);
}

COMMAND_FUNCTION(setplottitle) {
    return setplot_helper(shell_state, tokens, CMD_SET_PLOT_TITLE, num_args);
}

COMMAND_FUNCTION(list) {

    int len = shlen(shell_state->loaded_models);

    if (len == 0) {
        printf("No models loaded\n");
        return false;
    }

    printf("Current model: %s\n", shell_state->current_model->model_name);
    printf("Loaded models:\n");
    for (int i = 0; i < len; i++) {
        printf("%s\n", shell_state->loaded_models[i].key);
    }

    return true;
}

COMMAND_FUNCTION(vars) {

    struct model_config *model_config = NULL;

    if(num_args == 0) {
        model_config = shell_state->current_model;
    }
    else {
        model_config = load_model_config_or_print_error(shell_state, tokens[0], tokens[1]);
    }

    if (!model_config) return false;

    int len = shlen(model_config->var_indexes);

    printf("Model vars for model %s:\n", model_config->model_name);
    for (int i = 0; i < len; i++) {
        printf("%s, %d\n", model_config->var_indexes[i].key, model_config->var_indexes[i].value);
    }

    return true;
}

COMMAND_FUNCTION(cd) {

    const char *path = tokens[1];

    int ret = chdir(path);
    if (ret == -1) {
        printf("Error changing working directory to %s\n", path);
        return false;
    } else {
        free(shell_state->current_dir);
        shell_state->current_dir = get_current_directory();
        print_current_dir();
        return true;
    }
}

COMMAND_FUNCTION(ls) {

    char *p = tokens[1];

    char *path_name;

    if (num_args == 0) {
        path_name = ".";
    } else {
        path_name = p;
    }

    print_path_contents(path_name);
    return true;
}

COMMAND_FUNCTION(help) {
    if (num_args == 0) {
        printf("Available commands:\n");
        int nc = arrlen(commands_sorted);
        for (int i = 0; i < nc; i++) {
            printf("%s\n", commands_sorted[i]);
        }

        printf("type 'help command' for more information about a specific command\n");
    } else {
        command command = shgets(commands, tokens[1]);
        if (command.help) {
            printf("%s\n", command.help);
        } else {
            printf("No help available for command %s yet\n", tokens[1]);
        }
    }
    return true;
}

COMMAND_FUNCTION(getplotconfig) {

    struct model_config *model_config = NULL;

    if(num_args == 0) {
        model_config = shell_state->current_model;
    }
    else {
        model_config = load_model_config_or_print_error(shell_state, tokens[0], tokens[1]);
    }

    if (!model_config) return false;

    char *xname = get_var_name(model_config, model_config->plot_config.xindex);
    char *yname = get_var_name(model_config, model_config->plot_config.yindex);

    printf("\nPlot configuration for model %s\n\n", model_config->model_name);

    printf("Var on X: %s (%d)\n", xname, model_config->plot_config.xindex);
    printf("Var on Y: %s (%d)\n", yname, model_config->plot_config.yindex);

    printf("Label X: %s\n", model_config->plot_config.xlabel);
    printf("Label Y: %s\n", model_config->plot_config.ylabel);
    printf("Title (appears on plot legend): %s\n\n", model_config->plot_config.title);

    return true;
}

static bool set_or_get_value_helper(struct shell_variables *shell_state, sds *tokens, int num_args, ast_tag tag, bool set) {

    const char *command = tokens[0];
    sds var_name;
    char *new_value;

    struct model_config *parent_model_config = NULL;

    if (set) {
        var_name = sdsnew(tokens[num_args - 1]);
        new_value = tokens[num_args];

        if(num_args == 2) {
            parent_model_config = shell_state->current_model;
        }
        else if(num_args == 3) {
            parent_model_config = load_model_config_or_print_error(shell_state, command, tokens[1]);
        }

    } else {
        var_name = sdsnew(tokens[num_args]);

        if(tag == ast_ode_stmt) {
            var_name = sdscat(var_name, "'");
        }

        if(num_args == 1) {
            parent_model_config = shell_state->current_model;
        }
        else if(num_args == 2) {
            parent_model_config = load_model_config_or_print_error(shell_state, command, tokens[1]);
        }
    }

    if (!parent_model_config) return false;

    struct model_config *model_config;

    if (set) {
        model_config = new_config_from_parent(parent_model_config);
    } else {
        model_config = parent_model_config;
    }

    int n = arrlen(model_config->program);

    int i;
    for (i = 0; i < n; i++) {
        ast *a = model_config->program[i];
        if (a->tag == tag) {
            if (STR_EQUALS(a->assignement_stmt.name->identifier.value, var_name)) {

                if (set) {
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

    if (i == n) {
        printf("Error parsing command %s. Invalid variable name: %s. You can list model variable name using vars %s\n",
                command, var_name, model_config->model_name);
    } else {
        if (set) {
            printf("Reloading model %s as %s\n", parent_model_config->model_name, model_config->model_name);
            load_model(shell_state, NULL, model_config);
        }
    }

    sdsfree(var_name);

    return true;

}

COMMAND_FUNCTION(setinitialvalue) {
    return set_or_get_value_helper(shell_state, tokens, num_args, ast_initial_stmt, true);
}

COMMAND_FUNCTION(getinitialvalue) {
    return set_or_get_value_helper(shell_state, tokens, num_args, ast_initial_stmt, false);
}

COMMAND_FUNCTION(setparamvalue) {
    return set_or_get_value_helper(shell_state, tokens, num_args, ast_assignment_stmt, true);
}

COMMAND_FUNCTION(getparamvalue) {
    return set_or_get_value_helper(shell_state, tokens, num_args, ast_assignment_stmt, false);
}

COMMAND_FUNCTION(setglobalvalue) {
    return set_or_get_value_helper(shell_state, tokens, num_args, ast_global_stmt, true);
}

COMMAND_FUNCTION(getglobalvalue) {
    return set_or_get_value_helper(shell_state, tokens, num_args, ast_global_stmt, false);
}

COMMAND_FUNCTION(setodevalue) {
    return set_or_get_value_helper(shell_state, tokens, num_args, ast_ode_stmt, true);
}

COMMAND_FUNCTION(getodevalue) {
    return set_or_get_value_helper(shell_state, tokens, num_args, ast_ode_stmt, false);
}

static bool get_values_helper(struct shell_variables *shell_state, sds *tokens, int num_args, ast_tag tag) {

    struct model_config *model_config = NULL;

    if(num_args == 0) {
        model_config = shell_state->current_model;
    }
    else {
        model_config = load_model_config_or_print_error(shell_state, tokens[0], tokens[1]);
    }

    if (!model_config) return false;

    int n = arrlen(model_config->program);

    printf("Model %s: ", model_config->model_name);

    bool empty = true;

    for (int i = 0; i < n; i++) {
        ast *a = model_config->program[i];
        if (a->tag == tag) {
            printf("\n%s = %s", a->assignement_stmt.name->identifier.value, ast_to_string(a->assignement_stmt.value));
            empty = false;
        }
    }

    if (empty) {
        printf("No values to show");
    }

    printf("\n");

    return true;
}

COMMAND_FUNCTION(getinitialvalues) {
    return get_values_helper(shell_state, tokens, num_args, ast_initial_stmt);
}

COMMAND_FUNCTION(getparamvalues) {
    return get_values_helper(shell_state, tokens, num_args, ast_assignment_stmt);
}

COMMAND_FUNCTION(getglobalvalues) {
    return get_values_helper(shell_state, tokens, num_args, ast_global_stmt);
}

COMMAND_FUNCTION(getodevalues) {
    return get_values_helper(shell_state, tokens, num_args, ast_ode_stmt);
}

COMMAND_FUNCTION(saveplot) {

    const char *command = tokens[0];

    const char *file_name = tokens[1];

    if (!have_gnuplot(shell_state)) return false;

    if (shell_state->gnuplot_handle == NULL) {
        printf("Error executing command %s. No previous plot. plot the model first using \"plot modelname\" or list loaded models using \"list\"\n", command);
        return false;
    }

    const char *ext = get_filename_ext(file_name);

    if (!FILE_HAS_EXTENSION(ext, "pdf") && !FILE_HAS_EXTENSION(ext, "png")) {
        printf("Error executing command %s. Only .pdf and .png outputs are supported\n", command);
        return false;
    }

    gnuplot_cmd(shell_state->gnuplot_handle, "set terminal %s", ext);
    gnuplot_cmd(shell_state->gnuplot_handle, "set output \"%s", file_name);
    gnuplot_cmd(shell_state->gnuplot_handle, "replot");

    reset_terminal(shell_state->gnuplot_handle, shell_state->default_gnuplot_term);

    return true;
}

COMMAND_FUNCTION(setcurrentmodel) {

    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens[0], tokens[1]);

    if (!model_config) return false;

    shell_state->current_model = model_config;

    printf("Current model set to %s\n", shell_state->current_model->model_name);

    return true;
}

COMMAND_FUNCTION(solveplot) {

    bool success = solve(shell_state, tokens, num_args);

    if (!success) return false;

    char *new_tokens[2];
    new_tokens[0] = "plot";

    if (num_args == 2) {
        new_tokens[1] = tokens[1];
    }

    plot_helper(shell_state, new_tokens, CMD_PLOT, num_args - 1);

    return true;
}

COMMAND_FUNCTION(printmodel) {

    struct model_config *model_config = NULL;

    if(num_args == 0) {
        model_config = shell_state->current_model;
    }
    else {
        model_config = load_model_config_or_print_error(shell_state, tokens[0], tokens[1]);
    }

    if (!model_config) return false;

    printf("\n");
    print_program(model_config->program);
    printf("\n");

    return true;
}

COMMAND_FUNCTION(editmodel) {

    struct model_config *model_config = NULL;

    if(num_args == 0) {
        model_config = shell_state->current_model;
    }
    else {
        model_config = load_model_config_or_print_error(shell_state, tokens[0], tokens[1]);
    }

    if (!model_config) return false;

    if (!can_run_command("xdg-open")) {
        printf("Error - xdg-open is not in your path or is not installed\n");
        return false;
    }

    sds cmd = sdscatfmt(sdsempty(), "xdg-open %s.ode", model_config->model_name);

    FILE *f = popen(cmd, "r");
    pclose(f);

    return true;
}

COMMAND_FUNCTION(unload) {

    struct model_config *model_config = NULL;

    if(num_args == 0) {
        model_config = shell_state->current_model;
    }
    else {
        model_config = load_model_config_or_print_error(shell_state, tokens[0], tokens[1]);
    }

    if (!model_config) return false;

    bool is_current = (model_config == shell_state->current_model);

    free_model_config(model_config);
    shdel(shell_state->loaded_models, tokens[1]);

    if (shell_state->loaded_models != 0 && is_current) {
        //TODO: do we want to know the last loaded model??
        shell_state->current_model = shell_state->loaded_models[0].value;
    }

    return true;
}

static bool set_reload_helper(struct shell_variables *shell_state, sds *tokens, int num_args, command_type cmd_type) {

    char *command = tokens[0];
    char *arg = tokens[1];
    bool is_zero;

    if (STR_EQUALS(arg, "0")) {
        is_zero = true;
    } else if (STR_EQUALS(arg, "1")) {
        is_zero = false;
    } else {
        printf("Error - Invalid value %s for command %s. Valid values are 0 or 1\n", arg, command);
        return false;
    }

    struct model_config *model_config;

    if (cmd_type == CMD_SET_GLOBAL_RELOAD) {
        shell_state->never_reload = is_zero;
    } else {

        if(num_args == 1) {
            model_config = shell_state->current_model;
        }
        else {
            model_config = load_model_config_or_print_error(shell_state, command, arg);
        }

        if (cmd_type == CMD_SET_RELOAD) {
            model_config->should_reload = !is_zero;
        } else if (cmd_type == CMD_SET_AUTO_RELOAD) {
            model_config->auto_reload = !is_zero;

            if (!is_zero) {
                model_config->should_reload = true;
            }
        }
    }

    return true;
}

COMMAND_FUNCTION(setglobalreload) {
    return set_reload_helper(shell_state, tokens, num_args, CMD_SET_GLOBAL_RELOAD);
}

COMMAND_FUNCTION(setautolreload) {
    return set_reload_helper(shell_state, tokens, num_args, CMD_SET_AUTO_RELOAD);
}

COMMAND_FUNCTION(setshouldreload) {
    return set_reload_helper(shell_state, tokens, num_args, CMD_SET_RELOAD);
}

COMMAND_FUNCTION(savemodeloutput) {

    const char *command = tokens[0];
    uint run_number;
    struct model_config *model_config = get_model_and_n_runs_for_plot_cmds(shell_state, tokens, num_args, 1, &run_number);

    if (!model_config) return false;

    const char *file_name = tokens[num_args];

    sds output_file = get_model_output_file(model_config, run_number);

    if(cp_file(file_name, output_file, true) == -1) {
        printf("Error executing command %s. Could not copy %s to %s\n", command, output_file, file_name);
        sdsfree(output_file);
        return false;
    }

    sdsfree(output_file);
    return true;

}

void run_commands_from_file(struct shell_variables *shell_state, char *file_name) {

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

                if (!line[0] || line[0] == '#' || line[0] == '\n') continue;

                command = sdsnew(line);
                command = sdstrim(command, "\n ");
                add_history(command);
                printf("%s%s\n", PROMPT, command);
                quit = parse_and_execute_command(command, shell_state);
                sdsfree(command);
                if (quit) break;
            }

            fclose(f);
            if (line) {
                free(line);
            }

            if (quit) {
                sleep(1);
                clean_and_exit(shell_state);
            }
        }
    }
}

COMMAND_FUNCTION(loadcmds) {
    pthread_mutex_unlock(&shell_state->lock);
    run_commands_from_file(shell_state, tokens[1]);
    return true;
}

COMMAND_FUNCTION(quit) {
    printf("Bye!\n");
    return true;
}

COMMAND_FUNCTION(pwd) {
    print_current_dir();
    return true;
}

COMMAND_FUNCTION(odestolatex) {

    struct model_config *model_config = NULL;

    if(num_args == 0) {
        model_config = shell_state->current_model;
    }
    else {
        model_config = load_model_config_or_print_error(shell_state, tokens[0], tokens[1]);
    }

    if (!model_config) return false;

    sds *odes = odes_to_latex(model_config->program);

    for(int i = 0; i < arrlen(odes); i++) {
        printf("%s\n", odes[i]);
    }

    return true;

}

void clean_and_exit(struct shell_variables *shell_state) {

    sds history_path = sdsnew(get_home_dir());
    history_path = sdscatfmt(history_path, "/%s", HISTORY_FILE);

    int n_models = shlen(shell_state->loaded_models);
    for (int i = 0; i < n_models; i++) {
        struct model_config *config = shell_state->loaded_models[i].value;
        if (config->num_runs > 0) {
            //TODO: remove all output files
            //unlink(config->output_file);
        }
        if (config->model_command) {
            unlink(config->model_command);
        }
    }

    printf("\n");
    write_history(history_path);
    exit(0);
}

bool parse_and_execute_command(sds line, struct shell_variables *shell_state) {

    int num_args, token_count;

    sds *tokens = sdssplitargs(line, &token_count);

    if (!tokens) {
        printf("Error parsing command line!\n");
        goto dealloc_vars;
    }

    num_args = token_count - 1;

    command command = shgets(commands, tokens[0]);

    if (!command.command_function) {
        printf("Invalid command: %s\n", tokens[0]);
        goto dealloc_vars;
    }

    CHECK_N_ARGS(command.key, command.accept[0], command.accept[1], num_args);

    if (STR_EQUALS(command.key, "quit")) {
        return true;
    }

    pthread_mutex_lock(&shell_state->lock);

    command.command_function(shell_state, tokens, num_args);

    pthread_mutex_unlock(&shell_state->lock);
    sdsfreesplitres(tokens, token_count);
    return false;

dealloc_vars :
    if (tokens)
        sdsfreesplitres(tokens, token_count);

    pthread_mutex_unlock(&shell_state->lock);
    return false;

}

void maybe_reload_from_file_change(struct shell_variables *shell_state, struct inotify_event *event) {

    if (shell_state->never_reload) return;

    pthread_mutex_lock(&shell_state->lock);

    struct model_config **model_configs = hmget(shell_state->notify_entries, event->wd);
    struct model_config *model_config;

    for (int i = 0; i < arrlen(model_configs); i++) {
        char *file_name = get_file_from_path(model_configs[i]->model_file);
        if (STR_EQUALS(file_name, event->name)) {
            model_config = model_configs[i];
            break;
        }
    }

    if (!model_config->should_reload) {
        pthread_mutex_unlock(&shell_state->lock);
        return;
    }

    size_t file_size;
    char *source = read_entire_file_with_mmap(model_config->model_file, &file_size);
    meow_u128 hash = MeowHash(MeowDefaultSeed, file_size, source);
    munmap(source, file_size);

    int file_equals = MeowHashesAreEqual(hash, model_config->hash);

    if (file_equals) {
        pthread_mutex_unlock(&shell_state->lock);
        return;
    }

    if (model_config && !model_config->is_derived) {

        char answer = 0;

        if (!model_config->auto_reload) {
            printf("\nModel %s was modified externally. Would you like to reload it? [y]: ", model_config->model_name);
            answer = getchar();
        }

        if (model_config->auto_reload || answer == 'Y' || answer == 'y' || answer == '\r') {
            printf("\nReloading model %s", model_config->model_name);
            fflush(stdout);
            free_program(model_config->program);
            bool error = generate_model_program(model_config);

            if (!error) {
                error = compile_model(model_config);
            } else {
                printf("Error compiling model %s", model_config->model_name);
            }
        }
    }

    printf("\n%s", PROMPT);
    fflush(stdout);

    pthread_mutex_unlock(&shell_state->lock);
}

static void add_cmd(command_fn *function, char *cmd,  int accept_min, int accept_max, char *help) {

    command c;

    c.key = strdup(cmd);

    c.accept[0] = accept_min;
    c.accept[1] = accept_max;

    if (help) {
        c.help = strdup(help);
    }

    c.command_function = function;

    shputs(commands, c);
    arrput(commands_sorted, strdup(cmd));
}

void initialize_commands() {

#define DEFAULT_MSG "the command is executed using the last loaded model"
#define NO_ARGS     "If no arguments are provided, " DEFAULT_MSG
#define ONE_ARG     "If only one argurment is provided, " DEFAULT_MSG
#define TWO_ARGS    "If only two argurments are provided, " DEFAULT_MSG

    rl_attempted_completion_function = command_completion;

    command def;
    def.key = strdup("invalid");

    shdefaults(commands, def);
    sh_new_arena(commands);

    arrsetcap(commands_sorted, 64);


    //TODO: fix the help for the plot functions
    ADD_CMD(cd,               1, 1, "Changes the current directory, e.g., cd examples");
    ADD_CMD(quit,             0, 0, "Quits the shell (CTRL+d also quits).");
    ADD_CMD(help,             0, 1, "Prints all available commands or the help for a specific command, e.g., help run");
    ADD_CMD(list,             0, 0, "Lists all loaded models");
    ADD_CMD(loadcmds,         1, 1, "Loads a list of command from a file and execute them, e.g., loadcmds file.os");
    ADD_CMD(load,             1, 1, "Loads a model from a ode file, e.g, load sir.ode");
    ADD_CMD(unload,           1, 1, "Unloads previously loaded model, e.g, unload sir.ode");
    ADD_CMD(ls,               0, 1, "Lists the content of a given directory.");
    ADD_CMD(plot,             0, 2, "Plots the output of a model execution (one variable). "NO_ARGS". E.g., plot sir");
    ADD_CMD(replot,           0, 2, "Adds the output of a model execution (one variable) in to an existing plot. "NO_ARGS". E.g., plot sir");
    ADD_CMD(plottofile,       1, 3, "Plots the output of a model execution (one variable) in the specified file (pdf or png). "ONE_ARG". E.g., plot sir");
    ADD_CMD(replottofile,     1, 3, "Adds the output of a model execution (one variable) in to an existing plot in the specified file (pdf or png). "ONE_ARG". E.g., replottofile sir");
    ADD_CMD(plottoterm,       0, 2, "Plots the output of a model execution (one variable) using the terminal (text). "NO_ARGS". E.g., plottoterm sir");
    ADD_CMD(replottoterm,     0, 2, "Adds the output of a model execution (one variable) in to an existing plot using the terminal (text). "NO_ARGS". E.g., replototerm sir");
    ADD_CMD(setplotx,         1, 2, "Sets the variable to be plotted along the x axis. "ONE_ARG". E.g., setplotx sir t or setplotx t");
    ADD_CMD(setploty,         1, 2, "Sets the variable to be plotted along the y axis. "ONE_ARG". E.g., setplotx sir R or setplotx R");
    ADD_CMD(setplotxlabel,    1, 2, "Sets x axis label. "ONE_ARG". E.g., setplotxlabel sir Pop or setplotxlabel Pop");
    ADD_CMD(setplotylabel,    1, 2, "Sets y axis label. "ONE_ARG". E.g., setplotylabel sir days or setplotylabel days");
    ADD_CMD(setplottitle,     1, 2, "Sets the current plot title. "ONE_ARG". E.g., setplottitle sir title1 or setplottitle title1");
    ADD_CMD(pwd,              0, 0, "Shows the current directory");
    ADD_CMD(solve,            1, 2, "Solves the ODE(s) of a loaded model for x steps. "ONE_ARG". E.g., run sir 100");
    ADD_CMD(solveplot,        1, 2, "Solves the ODE(s) of a loaded model for x steps and plot it. "ONE_ARG". E.g., solveplot sir 100");
    ADD_CMD(vars,             0, 1, "List all variables available for plotting in a loaded model. "NO_ARGS". E.g vars sir");
    ADD_CMD(getplotconfig,    0, 1, "Prints the current plot configuration of a model. "NO_ARGS". E.g., getplotconfig sir");
    ADD_CMD(setinitialvalue,  2, 3, "Changes the initial value of a model's ODE variable and reloads the model. "TWO_ARGS". E.g setinitialvalue sir I 10");
    ADD_CMD(getinitialvalue,  2, 2, "Prints the initial value of a model's ODE variable. "ONE_ARG". E.g., getinitialvalue sir R");
    ADD_CMD(getinitialvalues, 0, 1, "Prints the initial values of all model's ODE variables. "NO_ARGS". E.g., getinitialvalues sir");
    ADD_CMD(setparamvalue,    2, 3, "Changes the value of a model's parameter and reloads the model. "TWO_ARGS". E.g setparamvalue sir gamma 10");
    ADD_CMD(getparamvalue,    1, 2, "Prints the value of a model's parameter. "ONE_ARG". E.g., getparamvalue sir gamma");
    ADD_CMD(getparamvalues,   0, 1, "Prints the values of all model's parameters. "NO_ARGS". E.g., getparamvalues sir");
    ADD_CMD(setglobalvalue,   2, 3, "Changes the value of a model's global variable and reloads the model. "TWO_ARGS". E.g setglobalalue sir n 2000");
    ADD_CMD(getglobalvalue,   1, 2, "Prints the value of a model's global variable. "ONE_ARG". E.g., getglobalalue sir n");
    ADD_CMD(getglobalvalues,  0, 1, "Prints the values of all model's global variables. "NO_ARGS". E.g., getglobalalues sir");
    ADD_CMD(setodevalue,      2, 3, "Changes the value of a model's ODE and reloads the model. "TWO_ARGS". E.g seodevalue sir S gama*beta");
    ADD_CMD(getodevalue,      1, 2, "Prints the value of a model's ODE. "ONE_ARG". E.g., getodevalue sir S");
    ADD_CMD(getodevalues,     0, 1, "Prints the values of all model's ODEs. "NO_ARGS". E.g., getodevalues sir");
    ADD_CMD(saveplot,         1, 1, "Saves the current plot to a pdf file, e.g., saveplot plot.pdf");
    ADD_CMD(setcurrentmodel,  1, 1, "Set the current model to be used as default parameters in several commands , e.g., setcurrentmodel sir");
    ADD_CMD(printmodel,       0, 1, "Print a model on the screen. "NO_ARGS". E.g printmodel sir");
    ADD_CMD(editmodel,        0, 1, "Open the file containing the model ode code. "NO_ARGS". E.g editmodel sir");
    ADD_CMD(odestolatex,      2, 1, "Print the latex code for the model ODEs. "NO_ARGS". E.g odestolatex sir");
    ADD_CMD(setautolreload,   1, 2, "Enable/disable auto reload value of a model. "ONE_ARG". E.g setautolreload sir 1 or setautolreload sir 0");
    ADD_CMD(setshouldreload,  1, 2, "Enable/disable reloading when changed for a model. "ONE_ARG". E.g setshouldreload sir 1 or setshouldreload sir 0");
    ADD_CMD(setglobalreload,  1, 1, "Enable/disable reloading for all models. E.g setglobalreload 1 or setglobalreload 0");
    ADD_CMD(savemodeloutput,  1, 3, "Saves the model output to a file. "ONE_ARG". E.g. savemodeloutput sir output_sir.txt");

    qsort(commands_sorted, arrlen(commands_sorted), sizeof(char *), string_cmp);
}
