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

#include "libfort/src/fort.h"
#include "command_corrector.h"

static command *commands = NULL;
static string_array commands_sorted = NULL;
static int num_commands = 0;
static struct shell_variables * global_state;

#define CREATE_TABLE(table)\
    ft_table_t *(table) = ft_create_table();\
    ft_set_border_style(table, FT_SOLID_ROUND_STYLE);\
    ft_set_cell_prop(table, 0, FT_ANY_COLUMN, FT_CPROP_ROW_TYPE, FT_ROW_HEADER);\

#define PRINT_AND_FREE_TABLE(table)\
    printf("%s", ft_to_string((table)));\
    ft_destroy_table(table);\

static void gnuplot_cmd(struct popen2 *handle, char const *cmd, ...) {
    va_list ap;

    va_start(ap, cmd);
    vdprintf(handle->to_child, cmd, ap);
    va_end(ap);

    dprintf(handle->to_child, "\nprint \"done\"\n");

    char msg[6];
    read(handle->from_child, msg, 6);

}

static void reset_terminal(struct popen2 *handle, const char *default_term) {
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

    if(model_name != NULL) {

        int index = shgeti(shell_state->loaded_models, model_name);

        if (index == -1) {
            printf("Error executing command %s. model %s is not loaded. Load a model first using \"load %s.edo\" or list loaded models using \"list\"\n", command, model_name, model_name);
            return NULL;
        }


        struct model_config *model_config = shell_state->loaded_models[index].value;
        return model_config;

    }
    else {
        return shell_state->current_model;
    }
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

    free(model_config->model_command);
    model_config->model_command = strdup(compiled_model_name);

    sds compiled_file;
    compiled_file = sdscatfmt(sdsempty(), "/tmp/%s_XXXXXX.c", modified_model_name);

    int fd = mkstemps(compiled_file, 2);

    FILE *outfile = fdopen(fd, "w");
    bool error = convert_to_c(model_config->program, outfile, EULER_ADPT_SOLVER);
    fclose(outfile);

    if(!error) {

        sds compiler_command = sdsnew("gcc");
#ifdef DEBUG_INFO
        compiler_command = sdscatfmt(compiler_command, " -g3 %s -o %s -lm", compiled_file, model_config->model_command);
#else
        compiler_command = sdscatfmt(compiler_command, " -O2 %s -o %s -lm", compiled_file, model_config->model_command);
#endif
        FILE *fp = popen(compiler_command, "r");
        error = check_and_print_execution_errors(fp);

        pclose(fp);
        unlink(compiled_file);
        sdsfree(compiler_command);
    }

    //Clean
    sdsfree(compiled_file);
    sdsfree(compiled_model_name);
    sdsfree(modified_model_name);

    return error;
}

static char *autocomplete_command(const char *text, int state) {

    rl_attempted_completion_over = 1;

    static int list_index, len;
    char *name;

    /* If this is a new word to complete, initialize now.  This includes
       saving the length of TEXT for efficiency, and initializing the index
       variable to 0. */
    if (state == 0) {
        list_index = 0;
        len = strlen (text);
    }

    /* Return the next name which partially matches from the command list. */
    while (list_index < num_commands) {

        name = commands[list_index].key;
        list_index++;

        if (strncmp (name, text, len) == 0) {
            return (strdup(name));
        }
    }

    /* If no names matched, then return NULL. */
    return ((char *)NULL);

}

static bool should_complete_model(const char *c) {
    return     STR_EQUALS(c, "editmodel")
        || STR_EQUALS(c, "getglobalvalues")
        || STR_EQUALS(c, "getinitialvalues")
        || STR_EQUALS(c, "getinitialvalue")
        || STR_EQUALS(c, "getodevalues")
        || STR_EQUALS(c, "getparamvalues")
        || STR_EQUALS(c, "getplotconfig")
        || STR_EQUALS(c, "odestolatex")
        || STR_EQUALS(c, "plot")
        || STR_EQUALS(c, "plottofile")
        || STR_EQUALS(c, "plottoterm")
        || STR_EQUALS(c, "printmodel")
        || STR_EQUALS(c, "replot")
        || STR_EQUALS(c, "replottofile")
        || STR_EQUALS(c, "replottoterm")
        || STR_EQUALS(c, "savemodeloutput")
        || STR_EQUALS(c, "setautolreload")
        || STR_EQUALS(c, "setcurrentmodel")
        || STR_EQUALS(c, "setplotlegend")
        || STR_EQUALS(c, "setplotxlabel")
        || STR_EQUALS(c, "setplotylabel")
        || STR_EQUALS(c, "setshouldreload")
        || STR_EQUALS(c, "solve")
        || STR_EQUALS(c, "solveplot")
        || STR_EQUALS(c, "unload")
        || STR_EQUALS(c, "vars");

}

static bool should_complete_model_and_var(const char *c) {
    return     STR_EQUALS(c, "getodevalue")
        || STR_EQUALS(c, "setodevalue")
        || STR_EQUALS(c, "setplotx")
        || STR_EQUALS(c, "setploty")
        || STR_EQUALS(c, "plotvar")
        || STR_EQUALS(c, "replotvar");
}

#define FIND_MODEL()                                            \
    do {                                                        \
        while (list_index < num_loaded_models) {                \
            name = global_state->loaded_models[list_index].key; \
            list_index++;                                       \
            if (strncmp(name, text, len) == 0) {                \
                sdsfreesplitres(splitted_buferr, count);        \
                return (strdup(name));                          \
            }                                                   \
        }                                                       \
    } while (0)

#define FIND_VARS(model_name)                                                                      \
    do {                                                                                           \
        struct model_config *model_config = NULL;                                                  \
        int index = shgeti(global_state->loaded_models, model_name);                               \
        if (index != -1) model_config = global_state->loaded_models[index].value;                  \
        int num_vars = 0;                                                                          \
        if (model_config) {                                                                        \
            num_vars = shlen(model_config->var_indexes);                                           \
        }                                                                                          \
        while (list_index < num_vars) {                                                            \
            name = model_config->var_indexes[list_index].key;                                      \
            list_index++;                                                                          \
            if (strncmp(name, text, len) == 0) {                                                   \
                sdsfreesplitres(splitted_buferr, count);                                           \
                return (strdup(name));                                                             \
            }                                                                                      \
        }                                                                                          \
    } while (0)

static char *autocomplete_command_params(const char *text, int state) {

    static int list_index, len;
    char *name;

    rl_attempted_completion_over = 1;

    if (state == 0) {
        list_index = 0;
        len = strlen (text);
    }

    int count;
    sds *splitted_buferr = sdssplit(rl_line_buffer, " ", &count);

    int num_loaded_models = shlen(global_state->loaded_models);

    int index = shgeti(commands, splitted_buferr[0]);

    command c = {0};

    if(index != -1) {
        c = commands[index];
    }

    if(!c.key
            || STR_EQUALS(c.key, "list")
            || STR_EQUALS(c.key, "pwd")
            || STR_EQUALS(c.key, "quit")
            || STR_EQUALS(c.key, "setglobalreload")) {

        //Do nothing

    } else if(should_complete_model(c.key)) {
        if(count <= 2) {
            FIND_MODEL();
        }
    } else if(should_complete_model_and_var(c.key)) {
        if(count <= 2) {
            FIND_MODEL();
        }
        else if(count <= 3) {
            FIND_VARS(splitted_buferr[1]);
        }
    } else if(STR_EQUALS(c.key, "help")) {

        if(count <= 2) {
            while (list_index < num_commands) {

                name = commands[list_index].key;
                list_index++;

                if (strncmp (name, text, len) == 0) {
                    return (strdup(name));
                }
            }
        }
    }
    else {
        rl_attempted_completion_over = 0;
    }

    sdsfreesplitres(splitted_buferr, count);
    return ((char *)NULL);

}

static char **command_completion(const char *text, int start, int end) {
    (void) end;

    if(start == 0) {
        return rl_completion_matches(text, autocomplete_command);
    }
    else {
        return rl_completion_matches(text, autocomplete_command_params);
    }
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
        *run_number = (uint)string_to_long(tokens[1], &error);

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
            printf("Error running command %s. The model was executed %u time(s), but it was requested to plot run %u!\n", tokens[0], model_config->num_runs, *run_number);
            return NULL;
        }
    }

    return model_config;

}

static bool load_model(struct shell_variables *shell_state, const char *model_file, struct model_config *model_config) {

    bool error = false;
    bool new_model = (model_config == NULL);

    if (new_model) {

        const char *ext = get_filename_ext(model_file);

        sds new_file = sdsnew(model_file);

        if (ext == NULL) {
            //we try to add .ode and find the file again
            new_file = sdscat(new_file, ".ode");
        }

        model_config = calloc(1, sizeof(struct model_config));

        if(model_config == NULL) {
            fprintf(stderr, "%s - Error allocating memory fot the model config!\n", __FUNCTION__);
            return true;
        }

        model_config->should_reload = true;
        model_config->auto_reload = false;

        model_config->model_name = get_filename_without_ext(model_file);

        if(shgeti(shell_state->loaded_models, model_config->model_name) != -1) {
            printf("Model %s is alread loaded!\n", model_config->model_name);
            free_model_config(model_config);
            sdsfree(new_file);
            return true;
        }

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

        if (error) {
            free_model_config(model_config);
            return true;
        }

        model_config->plot_config.xindex = 1;
        model_config->plot_config.yindex = 2;

        if (!model_config->plot_config.xlabel) {
            model_config->plot_config.xlabel = strdup(get_var_name(model_config, 1));
        }

        if (!model_config->plot_config.ylabel) {
            model_config->plot_config.ylabel = strdup("Variable");
        }

        if (!model_config->plot_config.title) {
            char *title =get_var_name(model_config, 2);
            if(title)
                model_config->plot_config.title = strdup(title);
            else
                model_config->plot_config.title = strdup("None");
        }
    }

    model_config->is_derived = !new_model;

    error = compile_model(model_config);

    if(!error) {
        shput(shell_state->loaded_models, model_config->model_name, model_config);
        shell_state->current_model = model_config;

        if (new_model) {
            char *tmp = get_dir_from_path(model_config->model_file);
            add_file_watch(shell_state, tmp);
            free(tmp);
        }
    }
    else {
        free_model_config(model_config);
    }

    if(!error) {
        printf("Model %s successfully loaded\n", model_config->model_name);
    }

    return !error;
}

COMMAND_FUNCTION(load) {
    return load_model(shell_state, tokens[1], NULL);
}

static void load_min_max_data(const char *filename, struct model_config *model_config) {

    size_t len = 0;

    FILE *fp;
    double data;

    fp = fopen(filename, "r");

    if(fp == NULL) {
        fprintf(stderr, "Error reading file %s\n", filename);
        return;
    }

    char *line = NULL;

    int n  = shlen(model_config->var_indexes) - 1;

    model_config->runs[model_config->num_runs-1].vars_max_value = (double*)malloc(sizeof(double)*n);
    model_config->runs[model_config->num_runs-1].vars_min_value = (double*)malloc(sizeof(double)*n);

    int count = 0;
    while((getline(&line, &len, fp)) != -1) {
        if(line) {
            char *end;
            double min = strtod(line, &end);
            double max = strtod(end+1, NULL);
            model_config->runs[model_config->num_runs-1].vars_max_value[count] = max;
            model_config->runs[model_config->num_runs-1].vars_min_value[count] = min;
            count++;
        }
    }

    free(line);
    fclose(fp);
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

    GET_MODEL_ONE_ARG_OR_RETURN_FALSE(model_config, 1);

    model_config->num_runs++;

    struct run_info params = {0};
    params.time = simulation_steps;
    params.vars_max_value = NULL;
    params.vars_min_value = NULL;

    arrput(model_config->runs, params);

    sds output_file = get_model_output_file(model_config, model_config->num_runs);

    sds model_command = sdscat(sdsempty(), model_config->model_command);

    model_command = sdscatprintf(model_command, " %lf %s", simulation_steps, output_file);

    FILE *fp = popen(model_command, "r");
    bool error = check_and_print_execution_errors(fp);
    pclose(fp);

    if(!error) {
        printf("Model %s solved for %lf steps.\n", model_config->model_name, simulation_steps);
        sds min_max_filename = sdsempty();
        min_max_filename = sdscatfmt(min_max_filename, "%s_min_max", output_file);

        load_min_max_data(min_max_filename, model_config);
        sdsfree(min_max_filename);
    }
    else {
        model_config->num_runs--;
        arrpop(model_config->runs);
    }

    sdsfree(model_command);
    sdsfree(output_file);

    return true;
}

static bool plot_helper(struct shell_variables *shell_state, sds *tokens, command_type c_type, int num_args) {

    if (!have_gnuplot(shell_state)) return false;

    const char *command = "plot";
    uint run_number;
    struct model_config *model_config = get_model_and_n_runs_for_plot_cmds(shell_state, tokens, num_args, 0, &run_number);

    if (!model_config) return false;

    if (shell_state->gnuplot_handle == NULL) {

        if (c_type == CMD_REPLOT) {
            printf("Error executing command %s. No previous plot. plot the model first using \"plot modelname\" or list loaded models using \"list\"\n", command);
            return false;
        }

        shell_state->gnuplot_handle = (struct popen2 *) malloc(sizeof(struct popen2));
        if(shell_state->gnuplot_handle == NULL) {
            fprintf(stderr, "%s - error allocating memory for gnuplot handle\n", __FUNCTION__);
            return false;
        }

        popen2("gnuplot", shell_state->gnuplot_handle);
    }

    if (c_type == CMD_PLOT_TERM || c_type == CMD_REPLOT_TERM) {
        gnuplot_cmd(shell_state->gnuplot_handle, "set term dumb");
    }

    if(c_type == CMD_REPLOT || c_type == CMD_REPLOT_TERM) {
        command = "replot";
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

        shell_state->gnuplot_handle = (struct popen2 *) malloc(sizeof(struct popen2));
        if(shell_state->gnuplot_handle == NULL) {
            fprintf(stderr, "%s - error allocating memory for gnuplot handle\n", __FUNCTION__);
            return false;
        }
        popen2("gnuplot", shell_state->gnuplot_handle);
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

    GET_MODEL_ONE_ARG_OR_RETURN_FALSE(model_config, 1);

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

        CREATE_TABLE(table);
        if (c_type == CMD_SET_PLOT_X) {
            model_config->plot_config.xindex = index;
            if (index_as_str) {
                ft_printf_ln(table, "\'%s\' variable will be plotted along the X axis", cmd_param);
            }
            else {
                ft_printf_ln(table, "\'%s\' variable will be plotted along the X axis", var_name);
            }
        } else {

            model_config->plot_config.yindex = index;
            free(model_config->plot_config.title);

            if (index_as_str) {
                model_config->plot_config.title = strdup(cmd_param);
            } else {
                model_config->plot_config.title = strdup(var_name);
            }

            ft_printf_ln(table, "\'%s\' variable will be plotted along the Y axis", model_config->plot_config.title);
        }

        PRINT_AND_FREE_TABLE(table);

    } else if (c_type == CMD_SET_PLOT_X_LABEL || c_type == CMD_SET_PLOT_Y_LABEL) {

        if (c_type == CMD_SET_PLOT_X_LABEL) {
            free(model_config->plot_config.xlabel);
            model_config->plot_config.xlabel = strdup(cmd_param);
        } else {
            free(model_config->plot_config.ylabel);
            model_config->plot_config.ylabel = strdup(cmd_param);
        }
    } else if (c_type == CMD_SET_PLOT_TITLE) {
        free(model_config->plot_config.title);
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

COMMAND_FUNCTION(setplotlegend) {
    return setplot_helper(shell_state, tokens, CMD_SET_PLOT_TITLE, num_args);
}

bool plotvar_helper(struct shell_variables *shell_state, sds *tokens, command_type c_type, int num_args) {

    struct model_config *model_config = NULL;

    GET_MODEL_ONE_ARG_OR_RETURN_FALSE(model_config, 1);

    char *last_title = strdup(model_config->plot_config.title);
    int last_index   = model_config->plot_config.yindex;

    bool ret;

    ret = setplot_helper(shell_state, tokens, CMD_SET_PLOT_Y, num_args);

    if(ret) {
        char *new_tokens[2];
        new_tokens[0] = "plot";

        if(num_args == 2) {
            new_tokens[1] = tokens[1];
        }

        ret = plot_helper(shell_state, new_tokens, c_type, num_args-1);
        free(model_config->plot_config.title);
        model_config->plot_config.title = last_title;
        model_config->plot_config.yindex = last_index;

    }

    return ret;

}

COMMAND_FUNCTION(plotvar) {

    int varcount = 0;
    sds *vars = sdssplit(tokens[num_args], " ", &varcount);

    bool ret = true;

    char *new_tokens[num_args + 1];
    for(int i  = 0; i < num_args; i++) {
        new_tokens[i] = tokens[i];
    }

    for(int i = 0; i < varcount; i++) {
        new_tokens[num_args] = vars[i];
        if(i == 0) {
            ret &= plotvar_helper(shell_state, new_tokens, CMD_PLOT, num_args);
        }
        else {
            ret &= plotvar_helper(shell_state, new_tokens, CMD_REPLOT, num_args);
        }

    }

    sdsfreesplitres(vars, varcount);

    return ret;

}

COMMAND_FUNCTION(replotvar) {
    return plotvar_helper(shell_state, tokens, CMD_REPLOT, num_args);
}

COMMAND_FUNCTION(list) {

    int len = shlen(shell_state->loaded_models);

    CREATE_TABLE(table);

    ft_set_cell_prop(table, FT_ANY_COLUMN, 0, FT_CPROP_TEXT_ALIGN, FT_ALIGNED_CENTER);
    ft_set_cell_prop(table, FT_ANY_COLUMN, 1, FT_CPROP_TEXT_ALIGN, FT_ALIGNED_RIGHT);

    if(len == 0) {
        ft_printf_ln(table, "No models loaded");
    }
    else {
        ft_printf_ln(table, "Available models|Num. of runs");
        for (int i = 0; i < len; i++) {
            char *name = shell_state->loaded_models[i].key;
            if(!STR_EQUALS(name, shell_state->current_model->model_name)) {
                ft_printf_ln(table, "%s|%d", name, shell_state->loaded_models[i].value->num_runs);
            }
            else {
                ft_printf_ln(table, "%s (*)|%d", name, shell_state->loaded_models[i].value->num_runs);
            }
        }
    }

    if(len > 0) {
        ft_printf_ln(table, "(*) - current model");
        ft_set_cell_span(table, len+1, 0, 2);
    }

    PRINT_AND_FREE_TABLE(table);


    return true;
}

COMMAND_FUNCTION(vars) {

    struct model_config *model_config = NULL;

    GET_MODEL_ONE_ARG_OR_RETURN_FALSE(model_config, 0);

    int len = shlen(model_config->var_indexes);

    CREATE_TABLE(table);

    ft_set_cell_prop(table, FT_ANY_COLUMN, FT_ANY_ROW, FT_CPROP_TEXT_ALIGN, FT_ALIGNED_CENTER);

    ft_printf_ln(table, "%s", model_config->model_name);
    ft_printf_ln(table, "Var name|Index");

    for (int i = 0; i < len; i++) {
        ft_printf_ln(table, "%s|%d", model_config->var_indexes[i].key, model_config->var_indexes[i].value);
    }

    ft_set_cell_span(table, 0, 0, 2);

    PRINT_AND_FREE_TABLE(table);

    return true;
}

COMMAND_FUNCTION(cd) {

    const char *path = tokens[1];
    int ret = -1;

    if(strlen(path) == 1 && path[0] == '-') {
        if(shell_state->last_dir) {
            ret = chdir(shell_state->last_dir);
        }
    } else {
        ret = chdir(path);
    }

    if (ret == -1) {
        printf("Error changing working directory to %s\n", path);
        return false;
    } else {

        free(shell_state->last_dir);
        shell_state->last_dir = shell_state->current_dir;

        shell_state->current_dir = get_current_directory();
        print_current_dir();
        return true;
    }
}

COMMAND_FUNCTION(ls) {
    char *path_name;

    if (num_args == 0) {
        path_name = ".";
    } else {
        path_name = tokens[1];
    }

    print_path_contents(path_name);
    return true;
}

COMMAND_FUNCTION(help) {

    CREATE_TABLE(table);

    ft_set_cell_prop(table, FT_ANY_ROW, FT_ANY_COLUMN, FT_CPROP_TEXT_ALIGN, FT_ALIGNED_CENTER);

    if (num_args == 0) {

        ft_printf_ln(table, "Available commands");
        int nc = arrlen(commands_sorted);
        for (int i = 0; i < nc; i+=3) {
            if(i + 2 < nc) {
                ft_printf_ln(table, "%s|%s|%s", commands_sorted[i], commands_sorted[i + 1], commands_sorted[i + 2]);
            }
            else if(i + 1 < nc) {
                ft_printf_ln(table, "%s|%s| ", commands_sorted[i], commands_sorted[i + 1]);
            }
            else {
                ft_printf_ln(table, "%s| | ", commands_sorted[i]);
            }
        }

    } else {
        command command = {0};
        int index = shgeti(commands, tokens[1]);

        if(index != -1) {
            command = commands[index];
        }

        if (command.help) {
            ft_printf_ln(table, "Command|help");
            ft_printf_ln(table, "%s|%s", tokens[1], command.help);
        } else if(command.key) {
            ft_printf_ln(table, "%s|No help available yet!", tokens[1]);
        }
        else {
            ft_printf_ln(table, "Invalid command %s!", tokens[1]);
        }
    }

    if(num_args == 0) {
        ft_set_cell_span(table, 0, 0, 3);
    }

    PRINT_AND_FREE_TABLE(table);

    if(num_args == 0 ) {
        printf("\ntype 'help command' for more information about a specific command\n\n");
    }

    return true;
}

COMMAND_FUNCTION(getplotconfig) {

    CREATE_TABLE(table);

    struct model_config *model_config = NULL;

    GET_MODEL_ONE_ARG_OR_RETURN_FALSE(model_config, 0);

    char *xname = get_var_name(model_config, model_config->plot_config.xindex);
    char *yname = get_var_name(model_config, model_config->plot_config.yindex);

    ft_printf_ln(table, "Plot configuration for model %s|", model_config->model_name);

    ft_printf_ln(table, "Var on X|%s (%d)", xname, model_config->plot_config.xindex);
    ft_printf_ln(table, "Var on Y|%s (%d)", yname, model_config->plot_config.yindex);

    ft_printf_ln(table, "Label X|%s", model_config->plot_config.xlabel);
    ft_printf_ln(table, "Label Y|%s", model_config->plot_config.ylabel);
    ft_printf_ln(table, "Title (legend)|%s", model_config->plot_config.title);

    ft_set_cell_span(table, 0, 0, 2);
    ft_set_cell_prop(table, FT_ANY_ROW, FT_ANY_COLUMN, FT_CPROP_TEXT_ALIGN, FT_ALIGNED_CENTER);
    PRINT_AND_FREE_TABLE(table);

    return true;
}

static bool set_or_get_value_helper(struct shell_variables *shell_state, sds *tokens, int num_args, ast_tag tag, command_type action) {

    const char *command = tokens[0];
    sds var_name;
    char *new_value;

    struct model_config *parent_model_config = NULL;

    if (action == CMD_SET) {
        var_name = sdsnew(tokens[num_args - 1]);
        new_value = tokens[num_args];

        if(num_args == 2) {
            parent_model_config = load_model_config_or_print_error(shell_state, command, NULL);
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
            parent_model_config = load_model_config_or_print_error(shell_state, tokens[0], NULL);
        }
        else if(num_args == 2) {
            parent_model_config = load_model_config_or_print_error(shell_state, command, tokens[1]);
        }
    }

    if (!parent_model_config) return false;

    struct model_config *model_config;

    if (action == CMD_SET) {
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
                if (action == CMD_SET) {
                    lexer *l = new_lexer(new_value, model_config->model_name);
                    parser *p = new_parser(l);
                    program program = parse_program(p, false, false);

                    sds tmp1 = ast_to_string(a->assignement_stmt.value);
                    sds tmp2 = ast_to_string(program[0]);

                    printf("Changing value of variable %s from %s to %s for model %s\n",
                            var_name, tmp1, tmp2, parent_model_config->model_name);

                    sdsfree(tmp1);
                    sdsfree(tmp2);

                    int old_decl_pos = a->assignement_stmt.declaration_position;
                    free_ast(a->assignement_stmt.value);
                    a->assignement_stmt.value = copy_ast(program[0]);
                    a->assignement_stmt.declaration_position = old_decl_pos;

                    free_parser(p);
                    free_program(program);
                    free_lexer(l);

                } else {
                    sds tmp = ast_to_string(a->assignement_stmt.value);
                    printf("%s = %s for model %s\n", var_name, tmp, model_config->model_name);
                    sdsfree(tmp);
                }
                break;
            }
        }
    }

    if (i == n) {
        parent_model_config->version--;
        int command_len = strlen(command);
        printf("Error parsing command %s. Invalid variable name: %s. You can list model variable using g%*ss %s\n",
                command, var_name, command_len-1,  command + 1 ,parent_model_config->model_name);
        if(action == CMD_SET) {
            free_model_config(model_config);
        }
    } else {
        if (action == CMD_SET) {
            printf("Reloading model %s as %s\n", parent_model_config->model_name, model_config->model_name);
            load_model(shell_state, NULL, model_config);
        }
    }

    sdsfree(var_name);

    return true;

}

COMMAND_FUNCTION(setinitialvalue) {
    return set_or_get_value_helper(shell_state, tokens, num_args, ast_initial_stmt, CMD_SET);
}

COMMAND_FUNCTION(getinitialvalue) {
    return set_or_get_value_helper(shell_state, tokens, num_args, ast_initial_stmt, CMD_GET);
}

COMMAND_FUNCTION(setparamvalue) {
    return set_or_get_value_helper(shell_state, tokens, num_args, ast_assignment_stmt, CMD_SET);
}

COMMAND_FUNCTION(getparamvalue) {
    return set_or_get_value_helper(shell_state, tokens, num_args, ast_assignment_stmt, CMD_GET);
}

COMMAND_FUNCTION(setglobalvalue) {
    return set_or_get_value_helper(shell_state, tokens, num_args, ast_global_stmt, CMD_SET);
}

COMMAND_FUNCTION(getglobalvalue) {
    return set_or_get_value_helper(shell_state, tokens, num_args, ast_global_stmt, CMD_GET);
}

COMMAND_FUNCTION(setodevalue) {
    return set_or_get_value_helper(shell_state, tokens, num_args, ast_ode_stmt, CMD_SET);
}

COMMAND_FUNCTION(getodevalue) {
    return set_or_get_value_helper(shell_state, tokens, num_args, ast_ode_stmt, CMD_GET);
}

static bool get_values_helper(struct shell_variables *shell_state, sds *tokens, int num_args, ast_tag tag) {

    struct model_config *model_config = NULL;

    GET_MODEL_ONE_ARG_OR_RETURN_FALSE(model_config, 0);

    int n = arrlen(model_config->program);

    CREATE_TABLE(table);

    ft_printf_ln(table, "%s", model_config->model_name);
    ft_printf_ln(table, "Variable|Value");

    bool empty = true;

    for (int i = 0; i < n; i++) {
        ast *a = model_config->program[i];
        if (a->tag == tag) {
            sds ast_string = ast_to_string(a->assignement_stmt.value);
            char *first_paren = strchr(ast_string, '(');
            if(first_paren) {
                ft_printf_ln(table, "%s|%.*s", a->assignement_stmt.name->identifier.value, (int)strlen(first_paren+1) - 1, first_paren + 1);
            }
            else {
                ft_printf_ln(table, "%s|%s", a->assignement_stmt.name->identifier.value, ast_string);
            }
            sdsfree(ast_string);
            empty = false;
        }
    }

    if (empty) {
        ft_printf_ln(table, "No values to show");
    }

    ft_set_cell_span(table, 0, 0, 2);
    ft_set_cell_prop(table, FT_ANY_ROW, FT_ANY_COLUMN, FT_CPROP_TEXT_ALIGN, FT_ALIGNED_CENTER);

    PRINT_AND_FREE_TABLE(table);

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

    GET_MODEL_ONE_ARG_OR_RETURN_FALSE(model_config, 0);

    printf("\n");
    print_program(model_config->program);
    printf("\n");

    return true;
}

COMMAND_FUNCTION(editmodel) {

    struct model_config *model_config = NULL;

    GET_MODEL_ONE_ARG_OR_RETURN_FALSE(model_config, 0);

    if (!can_run_command("xdg-open")) {
        printf("Error - xdg-open is not in your path or is not installed\n");
        return false;
    }

    sds cmd = sdscatfmt(sdsempty(), "xdg-open %s.ode & 2> /dev/null", model_config->model_name);

    FILE *f = popen(cmd, "r");
    pclose(f);

    sdsfree(cmd);

    return true;
}

COMMAND_FUNCTION(unload) {

    struct model_config *model_config = NULL;

    GET_MODEL_ONE_ARG_OR_RETURN_FALSE(model_config, 0);

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

    model_config->runs[run_number-1].filename = strdup(file_name);
    model_config->runs[run_number-1].saved = true;

    sdsfree(output_file);
    return true;

}

bool run_commands_from_file(struct shell_variables *shell_state, char *file_name) {

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

                int cmd_count = 0;
                sds *commands = sdssplit(command, ";", &cmd_count);

                for(int i = 0; i < cmd_count; i++) {
                    if(*commands[i]) {
                        quit = parse_and_execute_command(commands[i], shell_state);
                        if (quit) break;
                    }
                }

                sdsfreesplitres(commands, cmd_count);
                sdsfree(command);
                if (quit) break;
            }

            fclose(f);
            if (line) {
                free(line);
            }

            if (quit) {
                //clean_and_exit(shell_state);
                return false;
            }
        }
    }

    return true;
}

COMMAND_FUNCTION(loadcmds) {
    return run_commands_from_file(shell_state, tokens[1]);
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
    GET_MODEL_ONE_ARG_OR_RETURN_FALSE(model_config, 0);

    sds *odes = odes_to_latex(model_config->program);

    for(int i = 0; i < arrlen(odes); i++) {
        char *first_paren = strchr(odes[i], '(');
        if(first_paren) {
            printf("%.*s ", (int)(first_paren - &odes[i][0]), odes[i]);
            printf("%.*s\n", (int)strlen(first_paren+1) - 1, first_paren + 1);
        }
        else {
            printf("%s\n", odes[i]);
        }
    }

    return true;

}


COMMAND_FUNCTION(listruns)  {

    struct model_config *model_config = NULL;
    GET_MODEL_ONE_ARG_OR_RETURN_FALSE(model_config, 0);

    int n_runs = model_config->num_runs;

    struct run_info *run_info = model_config->runs;

    printf("\nModel %s was solved %d time(s).\n\n", model_config->model_name, n_runs);

    CREATE_TABLE(table);

    if(n_runs > 0) {
        ft_printf_ln(table, "Run|Time|Output File");
    }

    for(int i = 0; i < n_runs; i++) {
        if(run_info[i].saved) {
            ft_printf_ln(table, "%d|%lf|%s", i+1, run_info[i].time, run_info[i].filename);
        }
        else {
            ft_printf_ln(table, "%d|%lf|%s", i+1, run_info[i].time, "output not saved!");
        }
    }


    PRINT_AND_FREE_TABLE(table);

    return true;
}

COMMAND_FUNCTION(getruninfo) {

    uint run_number;
    struct model_config *model_config = get_model_and_n_runs_for_plot_cmds(shell_state, tokens, num_args, 0, &run_number);

    if (!model_config) return false;

    struct run_info run_info = model_config->runs[run_number-1];

    CREATE_TABLE(table);

    ft_printf_ln(table, "Run|Time|Output File");

    if(run_info.saved) {
        ft_printf_ln(table, "%d|%lf|%s", run_number, run_info.time, run_info.filename);
    }
    else {
        ft_printf_ln(table, "%d|%lf|%s", run_number, run_info.time, "output not saved!");
    }

    PRINT_AND_FREE_TABLE(table);

    int len = shlen(model_config->var_indexes);

    CREATE_TABLE(table2);
    ft_printf_ln(table2, "Var name|Min value|Max value");

    for (int i = 0; i < len; i++) {
        int var_index = model_config->var_indexes[i].value;
        if(var_index > 1) {
            double min_value = model_config->runs[run_number-1].vars_min_value[var_index-2];
            double max_value = model_config->runs[run_number-1].vars_max_value[var_index-2];
            ft_printf_ln(table2, "%s|%lf|%lf", model_config->var_indexes[i].key, min_value, max_value);
        }
    }

    PRINT_AND_FREE_TABLE(table2);

    return true;
}

COMMAND_FUNCTION(resetruns) {

    struct model_config *model_config = NULL;
    GET_MODEL_ONE_ARG_OR_RETURN_FALSE(model_config, 0);

    if(model_config->num_runs > 0) {
        arrfree(model_config->runs);
        model_config->runs = NULL;
    }

    model_config->num_runs = 0;

    return true;

}

void clean_and_exit(struct shell_variables *shell_state) {

    sds history_path = sdsnew(get_home_dir());
    history_path = sdscatfmt(history_path, "/%s", HISTORY_FILE);

    int n_models = shlen(shell_state->loaded_models);

    for (int i = 0; i < n_models; i++) {
        struct model_config *config = shell_state->loaded_models[i].value;
        free_model_config(config);
    }

    shfree(shell_state->loaded_models);
    free(shell_state->default_gnuplot_term);

    int n = arrlen(commands_sorted);
    for(int i = 0; i < n; i++) {
        free(commands_sorted[i]);
    }
    arrfree(commands_sorted);

    shfree(commands);


    n = hmlen(shell_state->notify_entries);

    for(int i = 0; i < n; i++) {
        arrfree(shell_state->notify_entries[i].value);
    }

    hmfree(shell_state->notify_entries);

    printf("\n");
    write_history(history_path);

    sdsfree(history_path);
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

    int index = shgeti(commands, tokens[0]);

    if (index == -1) {
        printf("Invalid command: %s\n", tokens[0]);
        char *suggested_command = correct(tokens[0], commands_sorted, arrlen(commands_sorted));
        if(!STR_EQUALS(tokens[0], suggested_command)) {
            printf("Did you mean %s?\n", suggested_command);
        }
        goto dealloc_vars;
    }

    command command = commands[index];

    CHECK_N_ARGS(command.key, command.accept[0], command.accept[1], num_args);

    if (STR_EQUALS(command.key, "quit")) {
        if (tokens)
            sdsfreesplitres(tokens, token_count);

        return true;
    }

    if(!STR_EQUALS(tokens[0], "loadcmds")) {
        pthread_mutex_lock(&shell_state->lock);
    }
    command.command_function(shell_state, tokens, num_args);

    if(!STR_EQUALS(tokens[0], "loadcmds")) {
        pthread_mutex_unlock(&shell_state->lock);
    }

    sdsfreesplitres(tokens, token_count);
    return false;

dealloc_vars :
    if (tokens)
        sdsfreesplitres(tokens, token_count);

    return false;

}

void maybe_reload_from_file_change(struct shell_variables *shell_state, struct inotify_event *event) {

    if (shell_state->never_reload) return;

    pthread_mutex_lock(&shell_state->lock);

    struct model_config **model_configs = hmget(shell_state->notify_entries, event->wd);
    struct model_config *model_config = NULL;

    for (int i = 0; i < arrlen(model_configs); i++) {

        char *file_name = get_file_from_path(model_configs[i]->model_file);

        if (STR_EQUALS(file_name, event->name)) {
            model_config = model_configs[i];
            free(file_name);
            break;
        }

        free(file_name);

    }

    if (!model_config || !model_config->should_reload) {
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

    if (!model_config->is_derived) {

        char answer = 0;

        if (!model_config->auto_reload) {
            printf("\nModel %s was modified externally. Would you like to reload it? [y]: ", model_config->model_name);
            answer = (char) getchar();
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

    c.key = cmd;

    c.accept[0] = accept_min;
    c.accept[1] = accept_max;

    if (help) {
        c.help = help;
    }
    else {
        c.help = NULL;
    }

    c.command_function = function;

    shputs(commands, c);
    arrput(commands_sorted, strdup(cmd));
}

void initialize_commands(struct shell_variables *state, bool plot_enabled) {

#define DEFAULT_MSG "the command is executed using the last loaded model.\nE.g, "
#define DEFAULT_MSG_PLOT "the command is executed using the last loaded model and the output of the last execution.\nThe execution number can be passed to the command to plot/save a specific output.\nE.g.,"
#define NO_ARGS "\nIf no arguments are provided, " DEFAULT_MSG
#define ONE_ARG "\nIf only one argurment is provided, " DEFAULT_MSG
#define TWO_ARGS "\nIf only two argurments are provided, " DEFAULT_MSG
#define PLOT_ARGS "\nIf no arguments are provided, " DEFAULT_MSG_PLOT
#define PLOTFILE_ARGS "\nIf only the filename is provided, " DEFAULT_MSG_PLOT

#define DEFAULT_MSG_GETRUN "the command is executed using the last loaded model and the last execution.\nThe execution number can be passed to the command to list the details of an specific run.\nE.g.,"
#define GETRUN_ARGS "\nIf no arguments are provided, " DEFAULT_MSG_GETRUN


    global_state = state;

    rl_attempted_completion_function = command_completion;

    sh_new_arena(commands);

    arrsetcap(commands_sorted, 64);

    ADD_CMD(cd,               1, 1, "Changes the current directory.\n E.g., cd examples");
    ADD_CMD(quit,             0, 0, "Quits the shell (CTRL+d also quits).");
    ADD_CMD(help,             0, 1, "Prints all available commands or the help for a specific command.\nE.g., help run");
    ADD_CMD(list,             0, 0, "Lists all loaded models");
    ADD_CMD(loadcmds,         1, 1, "Loads a list of command from a file and execute them.\nE.g., loadcmds file");
    ADD_CMD(load,             1, 1, "Loads a model from a ode file.\nE.g., load sir.ode");
    ADD_CMD(listruns,         0, 1, "List all runs of a model."NO_ARGS" listruns sir");
    ADD_CMD(unload,           1, 1, "Unloads previously loaded model."NO_ARGS" unload sir");
    ADD_CMD(ls,               0, 1, "Lists the content of a given directory.");

    if(plot_enabled) {
        ADD_CMD(plot,          0, 2, "Plots the output of a model execution (one variable). "PLOT_ARGS " plot sir or plot sir 1 or plot 1");
        ADD_CMD(replot,        0, 2, "Adds the output of a model execution (one variable) in to an existing plot. "PLOT_ARGS" replot sir");
        ADD_CMD(plottofile,    1, 3, "Plots the output of a model execution (one variable) in the specified file (pdf or png). "PLOTFILE_ARGS"E.g., plottofile sir output.pdf 2");
        ADD_CMD(replottofile,  1, 3, "Adds the output of a model execution (one variable) in to an existing plot in the specified file (pdf or png). "PLOTFILE_ARGS "replottofile sir 2");
        ADD_CMD(plottoterm,    0, 2, "Plots the output of a model execution (one variable) using the terminal (text). "PLOT_ARGS". E.g., plottoterm sir");
        ADD_CMD(replottoterm,  0, 2, "Adds the output of a model execution (one variable) in to an existing plot using the terminal (text). "PLOT_ARGS" replototerm sir 2");
        ADD_CMD(setplotx,      1, 2, "Sets the variable to be plotted along the x axis. "ONE_ARG" setplotx sir t or setplotx t");
        ADD_CMD(setploty,      1, 2, "Sets the variable to be plotted along the y axis. "ONE_ARG" setploty sir R or setploty R");
        ADD_CMD(setplotxlabel, 1, 2, "Sets x axis label. "ONE_ARG" setplotxlabel sir Pop or setplotxlabel Pop");
        ADD_CMD(setplotylabel, 1, 2, "Sets y axis label. "ONE_ARG" setplotylabel sir days or setplotylabel days");
        ADD_CMD(setplotlegend, 1, 2, "Sets the current plot title. "ONE_ARG" setplottitle sir title1 or setplottitle title1");
        ADD_CMD(solveplot,     1, 2, "Solves the ODE(s) of a loaded model for x steps and plot it. "ONE_ARG" solveplot sir 100");
        ADD_CMD(saveplot,      1, 1, "Saves the current plot to a pdf file.\nE.g., saveplot plot.pdf");
        ADD_CMD(getplotconfig, 0, 1, "Prints the current plot configuration of a model. "NO_ARGS" getplotconfig sir");
        ADD_CMD(plotvar,       1, 2, "Plot a variable along the y axis. "ONE_ARG" plotvar sir R or plotvar R");
        ADD_CMD(replotvar,     1, 2, "Adds a variable to the current plot, along the y axis. "ONE_ARG" replotvar sir R or replotvar R");
    }

    ADD_CMD(pwd,              0, 0, "Shows the current directory");
    ADD_CMD(solve,            1, 2, "Solves the ODE(s) of a loaded model for x steps. "ONE_ARG" run sir 100");
    ADD_CMD(vars,             0, 1, "List all variables available for plotting in a loaded model. "NO_ARGS" vars sir");
    ADD_CMD(setinitialvalue,  2, 3, "Changes the initial value of a model's ODE variable and reloads the model. "TWO_ARGS"E.g setinitialvalue sir I 10");
    ADD_CMD(getinitialvalue,  1, 2, "Prints the initial value of a model's ODE variable. "ONE_ARG" getinitialvalue sir R");
    ADD_CMD(getinitialvalues, 0, 1, "Prints the initial values of all model's ODE variables. "NO_ARGS" getinitialvalues sir");
    ADD_CMD(setparamvalue,    2, 3, "Changes the value of a model's parameter and reloads the model. "TWO_ARGS" setparamvalue sir gamma 10");
    ADD_CMD(getparamvalue,    1, 2, "Prints the value of a model's parameter. "ONE_ARG"E.g., getparamvalue sir gamma");
    ADD_CMD(getparamvalues,   0, 1, "Prints the values of all model's parameters. "NO_ARGS" getparamvalues sir");
    ADD_CMD(setglobalvalue,   2, 3, "Changes the value of a model's global variable and reloads the model. "TWO_ARGS" setglobalalue sir n 2000");
    ADD_CMD(getglobalvalue,   1, 2, "Prints the value of a model's global variable. "ONE_ARG" getglobalalue sir n");
    ADD_CMD(getglobalvalues,  0, 1, "Prints the values of all model's global variables. "NO_ARGS" getglobalalues sir");
    ADD_CMD(setodevalue,      2, 3, "Changes the value of a model's ODE and reloads the model. "TWO_ARGS" seodevalue sir S gama*beta");
    ADD_CMD(getodevalue,      1, 2, "Prints the value of a model's ODE. "ONE_ARG" getodevalue sir S");
    ADD_CMD(getodevalues,     0, 1, "Prints the values of all model's ODEs. "NO_ARGS" getodevalues sir");
    ADD_CMD(setcurrentmodel,  1, 1, "Set the current model to be used as default parameters in several commands.\nE.g., setcurrentmodel sir");
    ADD_CMD(printmodel,       0, 1, "Print a model on the screen. "NO_ARGS" printmodel sir");
    ADD_CMD(editmodel,        0, 1, "Open the file containing the model ode code. "NO_ARGS" editmodel sir");
    ADD_CMD(odestolatex,      0, 1, "Print the latex code for the model ODEs. "NO_ARGS" odestolatex sir");
    ADD_CMD(setautolreload,   1, 2, "Enable/disable auto reload value of a model. "ONE_ARG" setautolreload sir 1 or setautolreload sir 0");
    ADD_CMD(setshouldreload,  1, 2, "Enable/disable reloading when changed for a model. "ONE_ARG" setshouldreload sir 1 or setshouldreload sir 0");
    ADD_CMD(setglobalreload,  1, 1, "Enable/disable reloading for all models.\nE.g., setglobalreload 1 or setglobalreload 0");
    ADD_CMD(savemodeloutput,  1, 3, "Saves the model output to a file. "PLOTFILE_ARGS" savemodeloutput sir output_sir.txt");
    ADD_CMD(resetruns,        0, 1, "Resets the runs information of a model. "NO_ARGS" resetruns sir");
    ADD_CMD(getruninfo,       0, 2, "Prints the information about an specific run. "GETRUN_ARGS " getruninfo sir or getruninfo sir 1 or getruninfo 1");
    qsort(commands_sorted, arrlen(commands_sorted), sizeof(char *), string_cmp);
}
