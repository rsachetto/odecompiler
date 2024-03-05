#include <stdio.h>
#include <stdlib.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <setjmp.h>
#include <stdbool.h>

#include "commands.h"
#include "file_utils/file_utils.h"
#include "inotify_helpers.h"
#include "ode_shell.h"
#include "stb/stb_ds.h"
#include "string/sds.h"
#include "gnuplot_utils.h"

#include <argp.h>

#include <termios.h>
#include <unistd.h>
#include <signal.h>

static struct termios termios_save;

void reset_the_terminal() {
    tcsetattr(0, 0, &termios_save);
}

const char *argp_program_version = "ode_shell 0.3";
const char *argp_program_bug_address = "<rsachetto@gmail.com>";

/* Program documentation. */
static char doc[] = "A simple command line utility to play with Ordinary Differential Equations.";

static char args_doc[] = "";

/* The options we understand. */
static struct argp_option options[] = {
    {"work_dir", 'w', "DIR", 0, "DIR where the shell will use as working directory.", 0},
    {"enable_sixel", 's', 0, 0, "Enable sixel gnuplot terminal when available", 0},
    {"force_sixel",  'f', 0, 0, "Force sixel gnuplot terminal (may not work)", 0},
    { 0 }
};

/* Used by main to communicate with parse_opt. */
struct arguments {
    char *command_file;
    char *work_dir;
    bool enable_sixel;
    bool force_sixel;
};

/* Parse a single option. */
static error_t parse_opt (int key, char *arg, struct argp_state *state) {

    struct arguments *arguments = state->input;

    switch (key) {
        case 'w':
            arguments->work_dir = arg;
            break;
        case 's':
            arguments->enable_sixel = true;
            break;
        case 'f':
            arguments->force_sixel = true;
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

    ///////////////////////////////////////////////////////////////////
    //all of this is to avoid printing ^C after the user press crtl+c//
    ///////////////////////////////////////////////////////////////////
    struct termios termios_new;

    int rc = tcgetattr(0, &termios_save );
    if (rc) {
        perror("tcgetattr"); exit(1);
    }

    rc = atexit(reset_the_terminal);
    if (rc) {
        perror("atexit"); exit(1);
    }

    termios_new = termios_save;
    termios_new.c_lflag &= ~ECHOCTL;
    rc = tcsetattr(0, 0, &termios_new );

    if (rc) {
        perror("tcsetattr"); exit(1);
    }

}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc, NULL, NULL, NULL};

int main(int argc, char **argv) {

    struct arguments arguments = {0};

    argp_parse(&argp, argc, argv, 0, 0, &arguments);

    struct shell_variables shell_state = {0};

    shell_state.enable_sixel = arguments.enable_sixel;
    shell_state.force_sixel = arguments.force_sixel;

    shell_state.current_dir = get_current_directory();
    shell_state.never_reload = false;

    sh_new_strdup(shell_state.loaded_models);

    bool add_plot_commands = check_gnuplot_and_set_default_terminal(&shell_state);

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

    bool quit = false;

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
