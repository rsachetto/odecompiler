#include "commands.h"

#include "stb/stb_ds.h"
#include "string/sds.h"
#include "file_utils/file_utils.h"

#include <readline/readline.h>

command *commands = NULL;

static void add_cmd(char *cmd, command_type type, int accept_min, int accept_max) {

    command c;

    c.key = strdup(cmd);
    c.value = type;

    c.accept[0] = accept_min;
    c.accept[1] = accept_max;

    shputs(commands, c);

}

void initialize_commands() {

    command def;
    def.key = strdup("invalid");
    def.value = CMD_INVALID;

    sh_new_arena(commands);
    shdefaults(commands, def);

    add_cmd("cd", CMD_CD, 1, 1);
    add_cmd("quit", CMD_QUIT, 0, 0);
    add_cmd("help", CMD_HELP, 0, 1);
    add_cmd("list", CMD_LIST, 0, 0);
    add_cmd("loadcmds", CMD_LOAD_CMDS, 1, 1);
    add_cmd("load", CMD_LOAD, 1, 1);
    add_cmd("ls", CMD_LS, 0, 1);
    add_cmd("plot", CMD_PLOT, 0, 1);
    add_cmd("setplotx", CMD_PLOT_SET_X, 1, 2);
    add_cmd("setploty", CMD_PLOT_SET_Y, 1, 2);
    add_cmd("pwd", CMD_PWD, 0, 0);
    add_cmd("replot", CMD_REPLOT, 0, 1);
    add_cmd("run", CMD_RUN, 1, 2);
    add_cmd("vars", CMD_VARS, 0, 1);
    add_cmd("getplotconfig", CMD_GET_PLOT_CONFIG, 0, 1);
    add_cmd("setinitialvalue", CMD_SET_INITIAL_VALUE, 2, 3);
    add_cmd("getinitialvalue", CMD_GET_INITIAL_VALUE, 1, 2);
    add_cmd("getinitialvalues", CMD_GET_INITIAL_VALUES, 0, 1);
    add_cmd("setparamvalue", CMD_SET_PARAM_VALUE, 2, 3);
    add_cmd("getparamvalue", CMD_GET_PARAM_VALUE, 1, 2);
    add_cmd("getparamvalues", CMD_GET_PARAM_VALUES, 0, 1);
    add_cmd("setglobalvalue", CMD_SET_GLOBAL_VALUE, 2, 3);
    add_cmd("getglobalvalue", CMD_GET_GLOBAL_VALUE, 1, 2);
    add_cmd("getglobalvalues", CMD_GET_GLOBAL_VALUES, 0, 1);
    add_cmd("setodevalue", CMD_SET_ODE_VALUE, 2, 3);
    add_cmd("getodevalue", CMD_GET_ODE_VALUE, 1, 2);
    add_cmd("getodevalues", CMD_GET_ODE_VALUES, 0, 1);
}


char *command_generator(const char *text, int state) {

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
    return rl_completion_matches(text, command_generator);
}

bool check_command_number_argument(const char* command, int expected_args, int num_args) {
    if(expected_args != num_args) {
        printf("Error: command %s accept %d argument(s). %d argument(s) given!\n", command, expected_args, num_args);
        return false;
    }
    return true;
}
