#include "commands.h"

#include "stb/stb_ds.h"
#include "string/sds.h"
#include "file_utils/file_utils.h"

#include <readline/readline.h>

char *command_generator(const char *text, int state) {

    static char *commands[] = {CMD_EXIT, CMD_LOAD, CMD_RUN, CMD_PLOT, CMD_REPLOT, CMD_LIST, CMD_VARS, CMD_PLOT_SET_X, CMD_PLOT_SET_Y, CMD_CD, CMD_LS, CMD_PWD, CMD_LOAD_CMDS};

    static string_array matches = NULL;
    static size_t match_index = 0;

    if (state == 0) {
        arrsetlen(matches, 0);
        match_index = 0;

        int len = sizeof(commands)/sizeof(commands[0]);

        sds textstr = sdsnew(text);
        for (int i = 0; i < len; i++) {
            char *word = commands[i];
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
