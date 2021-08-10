#ifndef __COMMMANDS_H
#define __COMMMANDS_H

#include <stdbool.h>
#include <stdio.h>
#include "string/sds.h"
#include "ode_shell.h"
#include "inotify_helpers.h"

#define COMMAND_FUNCTION(name) bool name(struct shell_variables *shell_state, sds *tokens, int num_args)
typedef COMMAND_FUNCTION(command_fn);

#define ADD_CMD(command, min_arg, max_arg, help) add_cmd(command, #command, min_arg, max_arg, help); num_commands++

typedef struct command_t {
    char *key; //command name
    command_fn *command_function;
    char *help;
    int accept[2];
} command;

typedef enum cmd_type_t {
    CMD_PLOT,
    CMD_REPLOT,
    CMD_PLOT_TERM,
    CMD_REPLOT_TERM,
    CMD_PLOT_FILE,
    CMD_REPLOT_FILE,
    CMD_SET_PLOT_X,
    CMD_SET_PLOT_Y,
    CMD_SET_PLOT_X_LABEL,
    CMD_SET_PLOT_Y_LABEL,
    CMD_SET_PLOT_TITLE,
    CMD_SET_RELOAD,
    CMD_SET_GLOBAL_RELOAD,
    CMD_SET_AUTO_RELOAD,
    CMD_SET,
    CMD_GET
} command_type;

#define CHECK_ARGS(command, expected, received)                                  \
    do {                                                                         \
        if (!check_command_number_argument((command), (expected), (received))) { \
            goto dealloc_vars;                                                   \
        }                                                                        \
    } while (0)


#define CHECK_N_ARGS(command, accept0, accept1, num_args)                                                                                \
    do {                                                                                                                                 \
        if ((accept0) == (accept1)) {                                                                                                        \
            CHECK_ARGS(command, accept0, num_args);                                                                                      \
        } else if ((num_args) < (accept0) || (num_args) > (accept1)) {                                                                           \
            printf("Error: command %s accepts from %d to %d argument(s). %d argument(s) given!\n", command, accept0, accept1, num_args); \
            goto dealloc_vars;                                                                                                           \
        }                                                                                                                                \
    } while (0)

void initialize_commands(struct shell_variables* state, bool plot_enabled);
bool parse_and_execute_command(sds line, struct shell_variables *shell_state);
void clean_and_exit(struct shell_variables *shell_state);
void maybe_reload_from_file_change(struct shell_variables *shell_state, struct inotify_event *event);
bool run_commands_from_file(struct shell_variables *shell_state, char *file_name);

#endif /* __COMMMANDS_H */
