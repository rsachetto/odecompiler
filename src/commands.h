#ifndef __COMMMANDS_H
#define __COMMMANDS_H

#include <stdbool.h>
#include <stdio.h>

typedef enum commmand_type_t {
    CMD_INVALID,
    CMD_CD,
    CMD_QUIT,
    CMD_HELP,
    CMD_LIST,
    CMD_LOAD_CMDS,
    CMD_LOAD,
    CMD_LS,
    CMD_PLOT,
    CMD_PLOT_SET_X,
    CMD_PLOT_SET_Y,
    CMD_PWD,
    CMD_REPLOT,
    CMD_RUN,
    CMD_VARS,
    CMD_GET_PLOT_CONFIG,
    CMD_SET_INITIAL_VALUE,
    CMD_GET_INITIAL_VALUE,
    CMD_SET_PARAM_VALUE,
    CMD_GET_PARAM_VALUE,
    CMD_SET_GLOBAL_VALUE,
    CMD_GET_GLOBAL_VALUE,
    CMD_GET_INITIAL_VALUES,
    CMD_GET_PARAM_VALUES,
    CMD_GET_GLOBAL_VALUES,
    CMD_SET_ODE_VALUE,
    CMD_GET_ODE_VALUE,
    CMD_GET_ODE_VALUES,
    CMD_SAVEPLOT
} command_type;

typedef struct command_t {
    char *key; //command name
    command_type value;
    char *help;
    int accept[2];
} command;

#define CHECK_ARGS(command, expected, received)                                  \
    do {                                                                         \
        if (!check_command_number_argument((command), (expected), (received))) { \
            goto dealloc_vars;                                                   \
        }                                                                        \
    } while (0)


#define CHECK_2_ARGS(command, accept0, accept1, num_args)                                                                          \
    do {                                                                                                                           \
        if (accept0 == accept1) {                                                                                                  \
            CHECK_ARGS(command, accept0, num_args);                                                                                \
        }                                                                                                                          \
        if (num_args != accept0 && num_args != accept1) {                                                                          \
            printf("Error: command %s accept %d or %d argument(s). %d argument(s) given!\n", command, accept0, accept1, num_args); \
            goto dealloc_vars;                                                                                                     \
        }                                                                                                                          \
    } while (0)

#define CHECK_3_ARGS(command, accept0, accept1, accept2, num_args)                                                                              \
    do {                                                                                                                                        \
        if (num_args != accept0 && num_args != accept1 && num_args != accept2) {                                                                \
            printf("Error: command %s accept %d, %d or %d argument(s). %d argument(s) given!\n", command, accept0, accept1, accept2, num_args); \
            goto dealloc_vars;                                                                                                                  \
        }                                                                                                                                       \
    } while (0)

void initialize_commands();
char **command_completion(const char *text, int start, int end);
bool check_command_number_argument(const char *command, int expected_args, int num_args);


#endif /* __COMMMANDS_H */
