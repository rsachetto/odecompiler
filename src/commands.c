#include "commands.h"
#include "stb/stb_ds.h"
#include "string/sds.h"
#include "file_utils/file_utils.h"

#include <readline/readline.h>

command *commands = NULL;
string_array commands_sorted = NULL;

static void add_cmd(char *cmd, command_type type, int accept_min, int accept_max, char *help) {

    command c;

    c.key = strdup(cmd);
    c.value = type;

    c.accept[0] = accept_min;
    c.accept[1] = accept_max;

    if(help) {
        c.help = strdup(help);
    }

    shputs(commands, c);
    arrput(commands_sorted, strdup(cmd));

}

void initialize_commands() {

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
    add_cmd("load",             CMD_LOAD,               1, 1, "Loads a model from a ode file. The model has to be located in the current directory, e.g, load sir.ode");
    add_cmd("ls",               CMD_LS,                 0, 1, "Lists the content of a given directory.");
    add_cmd("plot",             CMD_PLOT,               0, 1, "Plots the output of a model execution (one variable). If no argurments are provided, the command is executed using the last loaded model. E.g., plot sir" );
    add_cmd("setplotx",         CMD_PLOT_SET_X,         1, 2, "Sets the variable to be plotted along the x axis. If only one argurment is provided, the command is executed using the last loaded model. E.g., setplotx sir t or setplotx t");
    add_cmd("setploty",         CMD_PLOT_SET_Y,         1, 2, "Sets the variable to be plotted along the y axis. If only one argurment is provided, the command is executed using the last loaded model. E.g., setplotx sir R or setplotx R");
    add_cmd("pwd",              CMD_PWD,                0, 0, "Shows the current directory");
    add_cmd("replot",           CMD_REPLOT,             0, 1, "Adds the output of a model execution (one variable) in to an existing plot. If no argurments are provided, the command is executed using the last loaded model. E.g., plot sir" );
    add_cmd("run",              CMD_RUN,                1, 2, "Solves the ODE(s) of a loaded model for x steps. If only one argurment is provided, the command is executed using the last loaded model. E.g., run sir 100");
    add_cmd("vars",             CMD_VARS,               0, 1, "List all variables available for plotting in a loaded model. If no argurments are provided, the command is executed using the last loaded model. E.g vars sir");
    add_cmd("getplotconfig",    CMD_GET_PLOT_CONFIG,    0, 1, "Prints the current plot configuration of a model. If no argurment are provided, the command is executed using the last loaded model. E.g., getplotconfig sir");
    add_cmd("setinitialvalue",  CMD_SET_INITIAL_VALUE,  2, 3, "Changes the initial value of a model's ODE variable and reloads the model. If only two argurments are provided, the command is executed using the last loaded model. E.g setinitialvalue sir I 10");
    add_cmd("getinitialvalue",  CMD_GET_INITIAL_VALUE,  1, 2, "Prints the initial value of a model's ODE variable. If only one argurment is provided, the command is executed using the last loaded model. E.g., getinitialvalue sir R");
    add_cmd("getinitialvalues", CMD_GET_INITIAL_VALUES, 0, 1, "Prints the initial values of all model's ODE variables. If no argurments are provided, the command is executed using the last loaded model. E.g., getinitialvalues sir");
    add_cmd("setparamvalue",    CMD_SET_PARAM_VALUE,    2, 3, "Changes the value of a model's parameter and reloads the model. If only two argurments are provided, the command is executed using the last loaded model. E.g setparamvalue sir gamma 10");
    add_cmd("getparamvalue",    CMD_GET_PARAM_VALUE,    1, 2, "Prints the value of a model's parameter. If only one argurment is provided, the command is executed using the last loaded model. E.g., getparamvalue sir gamma");
    add_cmd("getparamvalues",   CMD_GET_PARAM_VALUES,   0, 1, "Prints the values of all model's parameters. If no argurments are provided, the command is executed using the last loaded model. E.g., getparamvalues sir");
    add_cmd("setglobalvalue",   CMD_SET_GLOBAL_VALUE,   2, 3, "Changes the value of a model's global variable and reloads the model. If only two argurments are provided, the command is executed using the last loaded model. E.g setglobalalue sir n 2000");
    add_cmd("getglobalvalue",   CMD_GET_GLOBAL_VALUE,   1, 2, "Prints the value of a model's global variable. If only one argurment is provided, the command is executed using the last loaded model. E.g., getglobalalue sir n");
    add_cmd("getglobalvalues",  CMD_GET_GLOBAL_VALUES,  0, 1, "Prints the values of all model's global variables. If no argurments are provided, the command is executed using the last loaded model. E.g., getglobalalues sir");
    add_cmd("setodevalue",      CMD_SET_ODE_VALUE,      2, 3, "Changes the value of a model's ODE and reloads the model. If only two argurments are provided, the command is executed using the last loaded model. E.g seodevalue sir S gama*beta");
    add_cmd("getodevalue",      CMD_GET_ODE_VALUE,      1, 2, "Prints the value of a model's ODE. If only one argurment is provided, the command is executed using the last loaded model. E.g., getodevalue sir S");
    add_cmd("getodevalues",     CMD_GET_ODE_VALUES,     0, 1, "Prints the values of all model's ODEs. If no argurments are provided, the command is executed using the last loaded model. E.g., getodevalues sir");
    add_cmd("saveplot",         CMD_SAVEPLOT,           1, 1, "Saves the current plot to a pdf file, e.g., saveplot plot.pdf");
    add_cmd("setcurrentmodel",  CMD_SET_CURRENT_MODEL,  1, 1, "Set the current model to be used as default parameters in several commands , e.g., setcurrentmodel sir");

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
            char *word = (char*)commands[i].key;
            size_t wlen = strlen(word);
            size_t tlen = strlen(textstr);

            if (wlen >= sdslen(textstr) &&  strncmp(word, textstr, tlen) == 0) {
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
    (void)start;
    (void)end;
    return rl_completion_matches(text, autocomplete_command);
}

bool check_command_number_argument(const char* command, int expected_args, int num_args) {
    if(expected_args != num_args) {
        printf("Error: command %s accept %d argument(s). %d argument(s) given!\n", command, expected_args, num_args);
        return false;
    }
    return true;
}
