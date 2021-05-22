#include <readline/history.h>
#include <readline/readline.h>
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "code_converter.h"
#include "commands.h"
#include "file_utils/file_utils.h"
#include "inotify_helpers.h"
#include "ode_shell.h"
#include "stb/stb_ds.h"
#include "string/sds.h"
#include "string_utils.h"

static sigjmp_buf env;
static void ctrl_c_handler(int sig) {
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

static void check_gnuplot_and_get_default_terminal(struct shell_variables *shell_state) {

    bool gnuplot_installed = can_run_command("gnuplot");

    if (!gnuplot_installed) {
        printf("Warning - gnuplot was not found. Make sure that it is installed and added to the PATH variable\n");
        printf("Plotting commands will not be available\n");
        shell_state->default_gnuplot_term = NULL;
        return;
    }

	const int BUF_MAX = 1024;

    FILE *f = popen("gnuplot -e \"show t\" 2>&1", "r");
    char msg[BUF_MAX];

    sds tmp = NULL;

    while (fgets(msg, BUF_MAX, f) != NULL) {
        int n = strlen(msg);
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
	}
	else {
		//I think this will never happen
		shell_state->default_gnuplot_term = strdup("dummy");
	}
    fclose(f);
}

int main(int argc, char **argv) {

    initialize_commands();

    print_current_dir();


    struct shell_variables shell_state = {0};

    shell_state.current_dir = get_current_directory();
    shell_state.never_reload = false;

    shdefault(shell_state.loaded_models, NULL);
    sh_new_strdup(shell_state.loaded_models);

	hmdefault(shell_state.notify_entries, NULL);

    check_gnuplot_and_get_default_terminal(&shell_state);

    //Setting up inotify
    shell_state.fd_notify = inotify_init();

    pthread_t inotify_thread;

    if (pthread_mutex_init(&shell_state.lock, NULL) != 0) {
        printf("\n mutex init has failed\n");
        return EXIT_FAILURE;
    }

    pthread_create(&inotify_thread, NULL, check_for_model_file_changes, (void *) &shell_state);

    if (argc == 2) {
        run_commands_from_file(argv[1], &shell_state);
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

        if (!line[0] || line[0] == '#' || line[0] == '\n') continue;
        add_history(line);
        quit = parse_and_execute_command(line, &shell_state);
        if (quit) break;
    }

    clean_and_exit(&shell_state);
}
