#include "commands.h"
#include "file_utils/file_utils.h"
#include "model_config.h"
#include "stb/stb_ds.h"
#include "code_converter.h"

#include <linux/limits.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <math.h>

#include "hash/meow_hash_x64_aesni.h"

command *commands = NULL;
string_array commands_sorted = NULL;

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

static struct model_config *load_model_config_or_print_error(struct shell_variables *shell_state, sds *tokens, int num_args, int model_name_position) {

    char *command = tokens[0];

    if (shlen(shell_state->loaded_models) == 0) {
        printf("Error executing command %s. No models loaded. Load a model first using load modelname.edo\n", command);
        return NULL;
    }

    const char *model_name;

    if (num_args == model_name_position) {
        return shell_state->current_model;
    } else {
        model_name = tokens[1];
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

static void add_cmd(char *cmd, command_type type, int accept_min, int accept_max, char *help) {

    command c;

    c.key = strdup(cmd);
    c.value = type;

    c.accept[0] = accept_min;
    c.accept[1] = accept_max;

    if (help) {
        c.help = strdup(help);
    }

    shputs(commands, c);
    arrput(commands_sorted, strdup(cmd));
}

void initialize_commands() {

	#define NO_ARGS  "If no arguments are provided, the command is executed using the last loaded model"
	#define ONE_ARG  "If only one argurment is provided, the command is executed using the last loaded model"
	#define TWO_ARGS "If only two argurments are provided, the command is executed using the last loaded model"

    command def;
    def.key = strdup("invalid");
    def.value = CMD_INVALID;

    shdefaults(commands, def);
    sh_new_arena(commands);

    arrsetcap(commands_sorted, 32);

    add_cmd("cd",               CMD_CD,                 1, 1, "Changes the current directory, e.g., cd examples");
    add_cmd("quit",             CMD_QUIT,               0, 0, "Quits the shell (CTRL+d also quits).");
    add_cmd("help",             CMD_HELP,               0, 1, "Prints all available commands or the help for a specific command, e.g., help run");
    add_cmd("list",             CMD_LIST,               0, 0, "Lists all loaded models");
    add_cmd("loadcmds",         CMD_LOAD_CMDS,          1, 1, "Loads a list of command from a file and execute them, e.g., loadcmds file.os");
    add_cmd("load",             CMD_LOAD,               1, 1, "Loads a model from a ode file, e.g, load sir.ode");
    add_cmd("unload",           CMD_UNLOAD,             1, 1, "Unloads previously loaded model, e.g, unload sir.ode");
    add_cmd("ls",               CMD_LS,                 0, 1, "Lists the content of a given directory.");
    add_cmd("plot",             CMD_PLOT,               0, 1, "Plots the output of a model execution (one variable). "NO_ARGS". E.g., plot sir");
    add_cmd("replot",           CMD_REPLOT,             0, 1, "Adds the output of a model execution (one variable) in to an existing plot. "NO_ARGS". E.g., plot sir");
    add_cmd("plottofile",       CMD_PLOT_FILE,          1, 2, "Plots the output of a model execution (one variable) in the specified file (pdf or png). "ONE_ARG". E.g., plot sir");
    add_cmd("replottofile",     CMD_REPLOT_FILE,        1, 2, "Adds the output of a model execution (one variable) in to an existing plot in the specified file (pdf or png). "ONE_ARG". E.g., replottofile sir");
    add_cmd("plottoterm",       CMD_PLOT_TERM,          0, 1, "Plots the output of a model execution (one variable) using the terminal (text). "NO_ARGS". E.g., plottoterm sir");
    add_cmd("replottoterm",     CMD_REPLOT_TERM,        0, 1, "Adds the output of a model execution (one variable) in to an existing plot using the terminal (text). "NO_ARGS". E.g., replototerm sir");
    add_cmd("setplotx",         CMD_PLOT_SET_X,         1, 2, "Sets the variable to be plotted along the x axis. "ONE_ARG". E.g., setplotx sir t or setplotx t");
    add_cmd("setploty",         CMD_PLOT_SET_Y,         1, 2, "Sets the variable to be plotted along the y axis. "ONE_ARG". E.g., setplotx sir R or setplotx R");
    add_cmd("setplotxlabel",    CMD_PLOT_SET_X_LABEL,   1, 2, "Sets x axis label. "ONE_ARG". E.g., setplotxlabel sir Pop or setplotxlabel Pop");
    add_cmd("setplotylabel",    CMD_PLOT_SET_Y_LABEL,   1, 2, "Sets y axis label. "ONE_ARG". E.g., setplotylabel sir days or setplotylabel days");
    add_cmd("setplottitle",     CMD_PLOT_SET_TITLE,     1, 2, "Sets the current plot title. "ONE_ARG". E.g., setplottitle sir title1 or setplottitle title1");
    add_cmd("pwd",              CMD_PWD,                0, 0, "Shows the current directory");
    add_cmd("solve",            CMD_SOLVE,              1, 2, "Solves the ODE(s) of a loaded model for x steps. "ONE_ARG". E.g., run sir 100");
    add_cmd("solveplot",        CMD_SOLVE_PLOT,         1, 2, "Solves the ODE(s) of a loaded model for x steps and plot it. "ONE_ARG". E.g., solveplot sir 100");
    add_cmd("vars",             CMD_VARS,               0, 1, "List all variables available for plotting in a loaded model. "NO_ARGS". E.g vars sir");
    add_cmd("getplotconfig",    CMD_GET_PLOT_CONFIG,    0, 1, "Prints the current plot configuration of a model. "NO_ARGS". E.g., getplotconfig sir");
    add_cmd("setinitialvalue",  CMD_SET_INITIAL_VALUE,  2, 3, "Changes the initial value of a model's ODE variable and reloads the model. "TWO_ARGS". E.g setinitialvalue sir I 10");
    add_cmd("getinitialvalue",  CMD_GET_INITIAL_VALUE,  1, 2, "Prints the initial value of a model's ODE variable. "ONE_ARG". E.g., getinitialvalue sir R");
    add_cmd("getinitialvalues", CMD_GET_INITIAL_VALUES, 0, 1, "Prints the initial values of all model's ODE variables. "NO_ARGS". E.g., getinitialvalues sir");
    add_cmd("setparamvalue",    CMD_SET_PARAM_VALUE,    2, 3, "Changes the value of a model's parameter and reloads the model. "TWO_ARGS". E.g setparamvalue sir gamma 10");
    add_cmd("getparamvalue",    CMD_GET_PARAM_VALUE,    1, 2, "Prints the value of a model's parameter. "ONE_ARG". E.g., getparamvalue sir gamma");
    add_cmd("getparamvalues",   CMD_GET_PARAM_VALUES,   0, 1, "Prints the values of all model's parameters. "NO_ARGS". E.g., getparamvalues sir");
    add_cmd("setglobalvalue",   CMD_SET_GLOBAL_VALUE,   2, 3, "Changes the value of a model's global variable and reloads the model. "TWO_ARGS". E.g setglobalalue sir n 2000");
    add_cmd("getglobalvalue",   CMD_GET_GLOBAL_VALUE,   1, 2, "Prints the value of a model's global variable. "ONE_ARG". E.g., getglobalalue sir n");
    add_cmd("getglobalvalues",  CMD_GET_GLOBAL_VALUES,  0, 1, "Prints the values of all model's global variables. "NO_ARGS". E.g., getglobalalues sir");
    add_cmd("setodevalue",      CMD_SET_ODE_VALUE,      2, 3, "Changes the value of a model's ODE and reloads the model. "TWO_ARGS". E.g seodevalue sir S gama*beta");
    add_cmd("getodevalue",      CMD_GET_ODE_VALUE,      1, 2, "Prints the value of a model's ODE. "ONE_ARG". E.g., getodevalue sir S");
    add_cmd("getodevalues",     CMD_GET_ODE_VALUES,     0, 1, "Prints the values of all model's ODEs. "NO_ARGS". E.g., getodevalues sir");
    add_cmd("saveplot",         CMD_SAVEPLOT,           1, 1, "Saves the current plot to a pdf file, e.g., saveplot plot.pdf");
    add_cmd("setcurrentmodel",  CMD_SET_CURRENT_MODEL,  1, 1, "Set the current model to be used as default parameters in several commands , e.g., setcurrentmodel sir");
    add_cmd("printmodel",       CMD_PRINT_MODEL,        0, 1, "Print a model on the screen. "NO_ARGS". E.g printmodel sir");
    add_cmd("editmodel",        CMD_EDIT_MODEL,         0, 1, "Open the file containing the model ode code. "NO_ARGS". E.g editmodel sir");
    add_cmd("setautolreload",   CMD_SET_AUTO_RELOAD,    1, 2, "Enable/disable auto reload value of a model. "ONE_ARG". E.g setautolreload sir 1 or setautolreload sir 0");
    add_cmd("setshouldreload",  CMD_SET_RELOAD,         1, 2, "Enable/disable reloading when changed for a model. "ONE_ARG". E.g setreload sir 1 or setreload sir 0");
    add_cmd("setglobalreload",  CMD_SET_GLOBAL_RELOAD,  1, 1, "Enable/disable reloading for all models. E.g setglobalreload 1 or setglobalreload 0");
	add_cmd("savemodeloutput",  CMD_SAVE_OUTPUT,        1, 2, "Saves the model output to a file. "ONE_ARG". E.g. savemodeloutput sir output_sir.txt");

    qsort(commands_sorted, arrlen(commands_sorted), sizeof(char *), string_cmp);
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

char **command_completion(const char *text, int start, int end) {
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

static void execute_load_command(struct shell_variables *shell_state, const char *model_file, struct model_config *model_config) {

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

        if (error) return;

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
}

static bool execute_solve_command(struct shell_variables *shell_state, sds *tokens, int num_args) {
    char *simulation_steps_str;
    double simulation_steps = 0;

    if (num_args == 1) {
        simulation_steps_str = tokens[1];
    } else {
        simulation_steps_str = tokens[2];
    }

    simulation_steps = string_to_double(simulation_steps_str);

    if (isnan(simulation_steps) || simulation_steps == 0) {
        printf("Error parsing command %s. Invalid number: %s\n", tokens[0], simulation_steps_str);
        return false;
    }

    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 1);

    if (!model_config) return false;

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

    if (!have_gnuplot(shell_state)) return;

    const char *command = tokens[0];
    struct model_config *model_config = NULL;

    if (c_type == CMD_PLOT || c_type == CMD_REPLOT || c_type == CMD_PLOT_TERM || c_type == CMD_REPLOT_TERM) {
        model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 0);
    } else if (c_type == CMD_PLOT_FILE || c_type == CMD_REPLOT_FILE) {
        model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 1);
    }

    if (!model_config) return;

    if (!model_config->output_file) {
        printf("Error executing command %s. Model %s was not executed. Run then model first using \"solve %s\" or list loaded models using \"list\"\n", command, model_config->model_name, model_config->model_name);
        return;
    }

    if (shell_state->gnuplot_handle == NULL) {
        if (c_type == CMD_REPLOT || c_type == CMD_REPLOT_FILE) {
            printf("Error executing command %s. No previous plot. plot the model first using \"plot modelname\" or list loaded models using \"list\"\n", command);
            return;
        }

        shell_state->gnuplot_handle = popen("gnuplot -persistent", "w");
    }

    if (c_type == CMD_PLOT_FILE || c_type == CMD_REPLOT_FILE) {
        if (c_type == CMD_PLOT_FILE) {
            command = "plot";
        } else {
            command = "replot";
        }

        const char *file_name = tokens[num_args];

        const char *ext = get_filename_ext(file_name);

        if (!FILE_HAS_EXTENSION(ext, "pdf") && !FILE_HAS_EXTENSION(ext, "png")) {
            printf("Error executing command %s. Only .pdf and .png outputs are supported\n", command);
            return;
        }

        gnuplot_cmd(shell_state->gnuplot_handle, "set terminal %s", ext);
        gnuplot_cmd(shell_state->gnuplot_handle, "set output \"%s", file_name);
    }

    if (c_type == CMD_PLOT_TERM || c_type == CMD_REPLOT_TERM) {
        if (c_type == CMD_PLOT_TERM) {
            command = "plot";
        } else {
            command = "replot";
        }
        gnuplot_cmd(shell_state->gnuplot_handle, "set term dumb");
    }

    gnuplot_cmd(shell_state->gnuplot_handle, "set xlabel \"%s\"", model_config->plot_config.xlabel);
    gnuplot_cmd(shell_state->gnuplot_handle, "set ylabel \"%s\"", model_config->plot_config.ylabel);
    gnuplot_cmd(shell_state->gnuplot_handle, "%s '%s' u %d:%d title \"%s\" w lines lw 2", command, model_config->output_file, model_config->plot_config.xindex, model_config->plot_config.yindex, model_config->plot_config.title);


    if (c_type == CMD_PLOT_FILE || c_type == CMD_REPLOT_FILE || c_type == CMD_PLOT_TERM || c_type == CMD_REPLOT_TERM) {
        reset_terminal(shell_state->gnuplot_handle, shell_state->default_gnuplot_term);
    }
}

static void execute_setplot_command(struct shell_variables *shell_state, sds *tokens, command_type c_type, int num_args) {

    if (!have_gnuplot(shell_state)) return;

    const char *command = tokens[0];

    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 1);

    if (!model_config) return;

    char *cmd_param = tokens[num_args];

    if (c_type == CMD_PLOT_SET_X || c_type == CMD_PLOT_SET_Y) {

        bool index_as_str;
        int index = (int) string_to_long(cmd_param, &index_as_str);
        char *var_name = NULL;

        if (index_as_str) {
            //string was passed, try to get the index from the var_indexes on model_config
            index = shget(model_config->var_indexes, cmd_param);

            if (index == -1) {
                printf("Error parsing command %s. Invalid variable name: %s. You can list model variable name using vars %s\n", command, cmd_param, model_config->model_name);
                return;
            }
        } else {
            var_name = get_var_name(model_config, index);

            if (!var_name) {
                printf("Error parsing command %s. Invalid index: %d. You can list model variable name using vars %s\n", command, index, model_config->model_name);
                return;
            }
        }

        if (c_type == CMD_PLOT_SET_X) {
            model_config->plot_config.xindex = index;
        } else {
            model_config->plot_config.yindex = index;

            if (index_as_str) {
                model_config->plot_config.title = strdup(cmd_param);
            } else {
                model_config->plot_config.title = strdup(var_name);
            }
        }
    } else if (c_type == CMD_PLOT_SET_X_LABEL || c_type == CMD_PLOT_SET_Y_LABEL) {

        if (c_type == CMD_PLOT_SET_X_LABEL) {
            model_config->plot_config.xlabel = strdup(cmd_param);
        } else {
            model_config->plot_config.ylabel = strdup(cmd_param);
        }
    } else {
        model_config->plot_config.title = strdup(cmd_param);
    }
}

static void execute_list_command(struct shell_variables *shell_state) {

    int len = shlen(shell_state->loaded_models);

    if (len == 0) {
        printf("No models loaded\n");
        return;
    }

    printf("Current model: %s\n", shell_state->current_model->model_name);
    printf("Loaded models:\n");
    for (int i = 0; i < len; i++) {
        printf("%s\n", shell_state->loaded_models[i].key);
    }
}

static void execute_vars_command(struct shell_variables *shell_state, sds *tokens, int num_args) {

    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 0);

    if (!model_config) return;

    int len = shlen(model_config->var_indexes);

    printf("Model vars for model %s:\n", model_config->model_name);
    for (int i = 0; i < len; i++) {
        printf("%s, %d\n", model_config->var_indexes[i].key, model_config->var_indexes[i].value);
    }

    return;
}

static void execute_cd_command(struct shell_variables *shell_state, const char *path) {

    int ret = chdir(path);
    if (ret == -1) {
        printf("Error changing working directory to %s\n", path);
    } else {
        free(shell_state->current_dir);
        shell_state->current_dir = get_current_directory();
        print_current_dir();
    }
}

static void execute_ls_command(char *p, int num_args) {

    char *path_name;

    if (num_args == 0) {
        path_name = ".";
    } else {
        path_name = p;
    }

    print_path_contents(path_name);
}

static void execute_help_command(sds *tokens, int num_args) {
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
}

static void execute_getplotconfig_command(struct shell_variables *shell_state, sds *tokens, int num_args) {

    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 0);

    if (!model_config) return;

    char *xname = get_var_name(model_config, model_config->plot_config.xindex);
    char *yname = get_var_name(model_config, model_config->plot_config.yindex);

    printf("\nPlot configuration for model %s\n\n", model_config->model_name);

    printf("Var on X: %s (%d)\n", xname, model_config->plot_config.xindex);
    printf("Var on Y: %s (%d)\n", yname, model_config->plot_config.yindex);

    printf("Label X: %s\n", model_config->plot_config.xlabel);
    printf("Label Y: %s\n", model_config->plot_config.ylabel);
	printf("Title (appears on plot legend): %s\n\n", model_config->plot_config.title);
}

static void execute_set_or_get_value_command(struct shell_variables *shell_state, sds *tokens, int num_args, ast_tag tag, bool set) {

    const char *command = tokens[0];
    char *var_name;
    char *new_value;

    struct model_config *parent_model_config;

    if (set) {
        var_name = tokens[num_args - 1];
        new_value = tokens[num_args];
        parent_model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 2);

    } else {
        var_name = tokens[num_args];
        parent_model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 1);
    }

    if (!parent_model_config) return;

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
            execute_load_command(shell_state, NULL, model_config);
        }
    }
}

static void execute_get_values_command(struct shell_variables *shell_state, sds *tokens, int num_args, ast_tag tag) {

    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 0);

    if (!model_config) return;

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
}

static void execute_save_plot_command(const char *command, struct shell_variables *shell_state, char *file_name) {

    if (!have_gnuplot(shell_state)) return;

    if (shell_state->gnuplot_handle == NULL) {
        printf("Error executing command %s. No previous plot. plot the model first using \"plot modelname\" or list loaded models using \"list\"\n", command);
        return;
    }

    const char *ext = get_filename_ext(file_name);

    if (!FILE_HAS_EXTENSION(ext, "pdf") && !FILE_HAS_EXTENSION(ext, "png")) {
        printf("Error executing command %s. Only .pdf and .png outputs are supported\n", command);
        return;
    }

    gnuplot_cmd(shell_state->gnuplot_handle, "set terminal %s", ext);
    gnuplot_cmd(shell_state->gnuplot_handle, "set output \"%s", file_name);
    gnuplot_cmd(shell_state->gnuplot_handle, "replot");

    reset_terminal(shell_state->gnuplot_handle, shell_state->default_gnuplot_term);
}

static void execute_set_current_model_command(struct shell_variables *shell_state, sds *tokens, int num_args) {

    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 0);

    if (!model_config) return;


    shell_state->current_model = model_config;

    printf("Current model set to %s\n", shell_state->current_model->model_name);
}

static void execute_solve_plot_command(struct shell_variables *shell_state, sds *tokens, int num_args, command_type c_type) {

    bool success = execute_solve_command(shell_state, tokens, num_args);

    if (!success) return;

    char *new_tokens[2];
    new_tokens[0] = "plot";

    if (num_args == 2) {
        new_tokens[1] = tokens[1];
    }

    execute_plot_command(shell_state, new_tokens, c_type, num_args - 1);
}

static void execute_print_model_command(struct shell_variables *shell_state, sds *tokens, int num_args) {

    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 0);

    if (!model_config) return;

    printf("\n");
    print_program(model_config->program);
    printf("\n");
}

static void execute_edit_model_command(struct shell_variables *shell_state, sds *tokens, int num_args) {

    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 0);

    if (!model_config) return;

    if (!can_run_command("xdg-open")) {
        printf("Error - xdg-open is not in your path or is not installed\n");
        return;
    }

    sds cmd = sdscatfmt(sdsempty(), "xdg-open %s.ode", model_config->model_name);

    FILE *f = popen(cmd, "r");
    pclose(f);
}

static void execute_unload_model_command(struct shell_variables *shell_state, sds *tokens, int num_args) {

    struct model_config *model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 0);
    if (!model_config) return;

    bool is_current = (model_config == shell_state->current_model);

    free_model_config(model_config);
    shdel(shell_state->loaded_models, tokens[1]);

    if (shell_state->loaded_models != 0 && is_current) {
        //TODO: do we want to know the last loaded model??
        shell_state->current_model = shell_state->loaded_models[0].value;
    }
}

static void execute_set_reload_command(struct shell_variables *shell_state, sds *tokens, int num_args, command_type cmd_type) {

    char *command = tokens[0];
    char *arg = tokens[1];
    bool is_zero;

    if (STR_EQUALS(tokens[1], "0")) {
        is_zero = true;
    } else if (STR_EQUALS(tokens[1], "1")) {
        is_zero = false;
    } else {
        printf("Error - Invalid value %s for command %s. Valid values are 0 or 1\n", tokens[1], tokens[0]);
        return;
    }

    struct model_config *model_config;

    if (cmd_type == CMD_SET_GLOBAL_RELOAD) {
        shell_state->never_reload = is_zero;
    } else {
        model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 1);

        if (cmd_type == CMD_SET_RELOAD) {
            model_config->should_reload = !is_zero;
        } else if (cmd_type == CMD_SET_AUTO_RELOAD) {
            model_config->auto_reload = !is_zero;

            if (!is_zero) {
                model_config->should_reload = true;
            }
        }
    }
}

static void execute_save_output_command(struct shell_variables *shell_state, sds *tokens, int num_args) {

    const char *command = tokens[0];
    struct model_config *model_config = NULL;

    model_config = load_model_config_or_print_error(shell_state, tokens, num_args, 1);

    if (!model_config) return;

	if (!model_config->output_file) {
        printf("Error executing command %s. Model %s was not executed. Run then model first using \"solve %s\" or list loaded models using \"list\"\n", command, model_config->model_name, model_config->model_name);
        return;
    }

	const char *file_name = tokens[num_args];

	if(cp_file(file_name, model_config->output_file, true) == -1) {
		printf("Error executing command %s. Could not copy %s to %s\n", command, model_config->output_file, file_name);
		return;
	}

}

void run_commands_from_file(char *file_name, struct shell_variables *shell_state) {

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

void clean_and_exit(struct shell_variables *shell_state) {

    sds history_path = sdsnew(get_home_dir());
    history_path = sdscatfmt(history_path, "/%s", HISTORY_FILE);

    int n_models = shlen(shell_state->loaded_models);
    for (int i = 0; i < n_models; i++) {
        struct model_config *config = shell_state->loaded_models[i].value;
        if (config->output_file) {
            unlink(config->output_file);
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
    command_type c_type = command.value;

    if (c_type == CMD_INVALID) {
        printf("Invalid command: %s\n", tokens[0]);
        goto dealloc_vars;
    }

    CHECK_2_ARGS(command.key, command.accept[0], command.accept[1], num_args);

    pthread_mutex_lock(&shell_state->lock);

    switch (c_type) {
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
            execute_setplot_command(shell_state, tokens, c_type, num_args);
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
			break;
		case CMD_SAVE_OUTPUT:
			execute_save_output_command(shell_state, tokens, num_args);
			break;
    }

dealloc_vars : {
    if (tokens)
        sdsfreesplitres(tokens, token_count);

    pthread_mutex_unlock(&shell_state->lock);

    return false;
}

    pthread_mutex_unlock(&shell_state->lock);
    sdsfreesplitres(tokens, token_count);
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
