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
    CMD_GET_ODE_VALUES
} command_type;

typedef struct command_t {
    char *key; //command name
    command_type value;
    int accept[2];
} command;

/*
static const char *commands[] = {"cd",  "quit",  "help",  "list",  "loadcmds", "load", "ls", "plot", "setplotx", "setploty",
                                 "pwd", "replot",  "run", "vars", "getplotconfig", "setinitialvalue", "getinitialvalue",
                                 "setparamvalue", "getparamvalue", "setglobalvalue", "getglobalvalue",
                                 "getinitialvalues", "getparamvalues", "getglobalvalues", "setodevalue", "getodevalue", "getodevalues"};

#define CMD_CD                 commands[0]
#define CMD_EXIT               commands[1]
#define CMD_HELP               commands[2]
#define CMD_LIST               commands[3]
#define CMD_LOAD_CMDS          commands[4]
#define CMD_LOAD               commands[5]
#define CMD_LS                 commands[6]
#define CMD_PLOT               commands[7]
#define CMD_PLOT_SET_X         commands[8]
#define CMD_PLOT_SET_Y         commands[9]
#define CMD_PWD                commands[10]
#define CMD_REPLOT             commands[11]
#define CMD_RUN                commands[12]
#define CMD_VARS               commands[13]
#define CMD_GET_PLOT_CONFIG    commands[14]
#define CMD_SET_INITIAL_VALUE  commands[15]
#define CMD_GET_INITIAL_VALUE  commands[16]
#define CMD_SET_PARAM_VALUE    commands[17]
#define CMD_GET_PARAM_VALUE    commands[18]
#define CMD_SET_GLOBAL_VALUE   commands[19]
#define CMD_GET_GLOBAL_VALUE   commands[20]
#define CMD_GET_INITIAL_VALUES commands[21]
#define CMD_GET_PARAM_VALUES   commands[22]
#define CMD_GET_GLOBAL_VALUES  commands[23]
#define CMD_SET_ODE_VALUE      commands[24]
#define CMD_GET_ODE_VALUE      commands[25]
#define CMD_GET_ODE_VALUES     commands[26]
//#define CMD_SET_CURRENT_MODEL commands[27] //Next command
*/

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
char *command_generator(const char *text, int state);
char **command_completion(const char *text, int start, int end);
bool check_command_number_argument(const char *command, int expected_args, int num_args);


#endif /* __COMMMANDS_H */
