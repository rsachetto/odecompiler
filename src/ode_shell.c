#include <stdio.h>
#include <stdlib.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>

#include "commands.h"
#include "file_utils/file_utils.h"
#include "inotify_helpers.h"
#include "ode_shell.h"
#include "stb/stb_ds.h"
#include "string/sds.h"
#include "string_utils.h"

#include <argp.h>

const char *argp_program_version = "ode_shell 0.1";
const char *argp_program_bug_address = "<rsachetto@gmail.com>";

/* Program documentation. */
static char doc[] = "A simple command line utility to play with Ordinary Differential Equations (ODEs).";

/* A description of the arguments we accept. */
static char args_doc[] = "FILE - A file with command to be executed.";

/* The options we understand. */
static struct argp_option options[] = {
    {"work_dir", 'w', "DIR", 0, "DIR where the shell will start." },
    { 0 }
};

/* Used by main to communicate with parse_opt. */
struct arguments {
    char *command_file;
    char *work_dir;
};

/* Parse a single option. */
static error_t parse_opt (int key, char *arg, struct argp_state *state) {

    struct arguments *arguments = state->input;

    switch (key) {
        case 'w':
            arguments->work_dir = arg;
            break;
        case ARGP_KEY_ARG:
            if (state->arg_num >= 1) {
                /* Too many arguments. */
                argp_usage(state);
            }

            arguments->command_file = arg;

            break;
        case ARGP_KEY_END:
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}


static sigjmp_buf env;
static void ctrl_c_handler(__attribute__((unused)) int sig) {
    siglongjmp(env, 42);
}

static void setup_ctrl_c_handler() {
    /* Setup SIGINT */
    struct sigaction s;
    s.sa_handler = ctrl_c_handler;
    sigemptyset(&s.sa_mask);
    s.sa_flags = SA_RESTART;
    sigaction(SIGINT, &s, NULL);
}

static bool check_gnuplot_and_get_default_terminal(struct shell_variables *shell_state) {

    bool gnuplot_installed = can_run_command("gnuplot");

    if (!gnuplot_installed) {
        fprintf(stderr, "Warning - gnuplot was not found. Make sure that it is installed and added to the PATH variable!\n");
        fprintf(stderr, "Plotting commands will not be available!\n");
        shell_state->default_gnuplot_term = NULL;
        return false;
    }

    const int BUF_MAX = 1024;

    FILE *f = popen("gnuplot -e \"show t\" 2>&1", "r");
    char msg[BUF_MAX];

    sds tmp = NULL;

    while (fgets(msg, BUF_MAX, f) != NULL) {
        unsigned long n = strlen(msg);
        if (n > 5) {
            tmp = sdsnew(msg);
            tmp = sdstrim(tmp, " \n");
            break;
        }
    }

    if(tmp) {
        int c;
        sds *tmp_tokens = sdssplit(tmp, " ", &c);
        shell_state->default_gnuplot_term = strdup(tmp_tokens[3]);
        sdsfreesplitres(tmp_tokens, c);
        sdsfree(tmp);
    }
    else {
        //I think this will never happen
        shell_state->default_gnuplot_term = strdup("dummy");
    }
    pclose(f);

    return true;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char **argv) {

    struct arguments arguments = {0};

    argp_parse (&argp, argc, argv, 0, 0, &arguments);

    struct shell_variables shell_state = {0};

    shell_state.current_dir = get_current_directory();
    shell_state.never_reload = false;

    sh_new_strdup(shell_state.loaded_models);

    bool add_plot_commands = check_gnuplot_and_get_default_terminal(&shell_state);

    initialize_commands(&shell_state, add_plot_commands);

    if(arguments.work_dir) {
        sds command = sdscatfmt(sdsempty(), "cd %s\n", arguments.work_dir);
        parse_and_execute_command(command, &shell_state);
        sdsfree(command);
    } else {
        print_current_dir();
    }

    //Setting up inotify
    shell_state.fd_notify = inotify_init();

    pthread_t inotify_thread;

    if (pthread_mutex_init(&shell_state.lock, NULL) != 0) {
        printf("\n mutex init has failed\n");
        return EXIT_FAILURE;
    }

    pthread_create(&inotify_thread, NULL, check_for_model_file_changes, (void *) &shell_state);

    if (arguments.command_file) {
        bool continue_ = run_commands_from_file(&shell_state, arguments.command_file);
        if(!continue_)
            clean_and_exit(&shell_state);
    }

    sds history_path = sdsnew(get_home_dir());
    history_path = sdscatfmt(history_path, "/%s", HISTORY_FILE);

    read_history(history_path);

    setup_ctrl_c_handler();

    char *line;

    //if CTRL+C is pressed, we print a new line
    if (sigsetjmp(env, 1) == 42) {
        printf("\n");
    }

    bool quit;

    while ((line = readline(PROMPT)) != 0) {
        //We do not want blank lines in the history
        if (!line[0] || line[0] == '\n') {
            free(line);
            continue;
        }

        //We want commented lines in the history
        add_history(line);

        //do not execute commented lines
        if (line[0] == '#') continue;

        int cmd_count = 0;
        sds *commands = sdssplit(line, ";", &cmd_count);

        //executing multiple commands per line separated by ";"
        for(int i = 0; i < cmd_count; i++) {
            if(*commands[i]) {
                quit = parse_and_execute_command(commands[i], &shell_state);
                if (quit) break;
            }
        }

        sdsfreesplitres(commands, cmd_count);
        free(line);
        if (quit) break;

    }

    sdsfree(history_path);
    clean_and_exit(&shell_state);
}
