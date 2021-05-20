#ifndef __COMMMANDS_H
#define __COMMMANDS_H

#include <stdbool.h>
#include <stdio.h>
#include "string/sds.h"
#include "ode_shell.h"
#include "inotify_helpers.h"

typedef enum commmand_type_t {
    CMD_INVALID,
    CMD_CD,
    CMD_QUIT,
    CMD_HELP,
    CMD_LIST,
    CMD_LOAD_CMDS,
    CMD_LOAD,
    CMD_UNLOAD,
    CMD_LS,
    CMD_PLOT,
    CMD_REPLOT,
    CMD_PLOT_FILE,
    CMD_REPLOT_FILE,
    CMD_PLOT_TERM,
    CMD_REPLOT_TERM,
    CMD_PLOT_SET_X,
    CMD_PLOT_SET_Y,
    CMD_PLOT_SET_X_LABEL,
    CMD_PLOT_SET_Y_LABEL,
    CMD_PLOT_SET_TITLE,
    CMD_PWD,
    CMD_SOLVE,
    CMD_SOLVE_PLOT,
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
    CMD_SAVEPLOT,
    CMD_SET_CURRENT_MODEL,
    CMD_PRINT_MODEL,
    CMD_EDIT_MODEL,
    CMD_SET_RELOAD,
    CMD_SET_AUTO_RELOAD,
    CMD_SET_GLOBAL_RELOAD
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

void initialize_commands();
char **command_completion(const char *text, int start, int end);
bool parse_and_execute_command(sds line, struct shell_variables *shell_state);
void clean_and_exit(struct shell_variables *shell_state);
void maybe_reload_from_file_change(struct shell_variables *shell_state, struct inotify_event *event);
void run_commands_from_file(char *file_name, struct shell_variables *shell_state);

#endif /* __COMMMANDS_H */
