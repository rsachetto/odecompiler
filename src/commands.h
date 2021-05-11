#ifndef __COMMMANDS_H
#define __COMMMANDS_H

#include <stdbool.h>
#include <stdio.h>

static const char *commands[] = {"cd",  "quit",  "help",  "list",  "load_cmds", "load", "ls", "plot", "setplotx", "setploty", "pwd", "replot",  "run", "vars"};

#define CMD_CD         commands[0]
#define CMD_EXIT       commands[1]
#define CMD_HELP       commands[2]
#define CMD_LIST       commands[3]
#define CMD_LOAD_CMDS  commands[4]
#define CMD_LOAD       commands[5]
#define CMD_LS         commands[6]
#define CMD_PLOT       commands[7]
#define CMD_PLOT_SET_X commands[8]
#define CMD_PLOT_SET_Y commands[9]
#define CMD_PWD        commands[10]
#define CMD_REPLOT     commands[11]
#define CMD_RUN        commands[12]
#define CMD_VARS       commands[13]
//#define CMD_NEW     commands[14]

#define CHECK_ARGS(command, expected, received)                                  \
    do {                                                                         \
        if (!check_command_number_argument((command), (expected), (received))) { \
            goto dealloc_vars;                                                   \
        }                                                                        \
    } while (0)


#define CHECK_2_ARGS(command, accept0, accept1, num_args)                                                                          \
    do {                                                                                                                           \
        if (num_args != accept0 && num_args != accept1) {                                                                          \
            printf("Error: command %s accept %d or %d argument(s). %d argument(s) given!\n", command, accept0, accept1, num_args); \
            goto dealloc_vars;                                                                                                     \
        }                                                                                                                          \
    } while (0)


struct shell_variables {
    struct model_hash_entry *loaded_models;
    char *last_loaded_model;
    FILE *gnuplot_handle;
};

char *command_generator(const char *text, int state);
char **command_completion(const char *text, int start, int end);
bool check_command_number_argument(const char *command, int expected_args, int num_args);


#endif /* __COMMMANDS_H */
