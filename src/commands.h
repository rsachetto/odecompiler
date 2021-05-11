#ifndef __COMMMANDS_H
#define __COMMMANDS_H 

#include <stdio.h>
#include <stdbool.h>

#define CMD_EXIT        "quit"
#define CMD_LOAD        "load"
#define CMD_RUN         "run"
#define CMD_PLOT        "plot"
#define CMD_REPLOT      "replot"
#define CMD_LIST        "list"
#define CMD_VARS        "vars"
#define CMD_PLOT_SET_X  "setplotx"
#define CMD_PLOT_SET_Y  "setploty"
#define CMD_CD          "cd"
#define CMD_LS          "ls"
#define CMD_PWD         "pwd"
#define CMD_LOAD_CMDS   "load_cmds"

#define CHECK_ARGS(command, expected, received)                                  \
    do {                                                                         \
        if (!check_command_number_argument((command), (expected), (received))) { \
            return;                                                              \
        }                                                                        \
    } while (0)

struct shell_variables {
    struct model_hash_entry *loaded_models;
    char *last_loaded_model;
    FILE *gnuplot_handle;
};

char *command_generator(const char *text, int state);
char **command_completion(const char *text, int start, int end);
bool check_command_number_argument(const char* command, int expected_args, int num_args);


#endif /* __COMMMANDS_H */
