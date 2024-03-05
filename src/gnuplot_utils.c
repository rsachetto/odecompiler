#include "gnuplot_utils.h"
#include "file_utils/file_utils.h"
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>


static bool check_sixel_support() {

    bool can_bash = can_run_command("bash");

    if(!can_bash) {
        return false;
    }

    char * command  = "#!/usr/bin/env bash\n"
        "\n"
        "stty -echo\n"
        "\n"
        "IFS=\";?c\" read -a REPLY -s -t 1 -d \"c\" -p $'\\e[c' >&2\n"
        "for code in \"${REPLY[@]}\"; do\n"
        "    if [[ $code == \"4\" ]]; then\n"
        "stty echo\n"
        "\texit 1\n"
        "    fi\n"
        "done\n"
        "\n"
        "if [[ \"$TERM\" == yaft* ]]; then stty echo\n exit 1; fi\n"
        "stty echo\n"
        "exit 0\n"
        "";


    FILE *tmp_file = fopen("/tmp/has_sixel_support_XXXX.sh", "w");
    fprintf(tmp_file, "%s\n", command);
    fclose(tmp_file);

    FILE *pipe = popen("bash /tmp/has_sixel_support_XXXX.sh", "r");
    return WEXITSTATUS(pclose(pipe));

}

void gnuplot_cmd(struct popen2 *handle, char const *cmd, ...) {
    va_list ap;

    va_start(ap, cmd);
    vdprintf(handle->to_child, cmd, ap);
    va_end(ap);

    dprintf(handle->to_child, "\nprint \"done\"\n");

    //To avoid plot and shell text overwriting when using sixel terminal
    char msg[20];
    read(handle->from_child, msg, 20);

}

void reset_terminal(struct popen2 *handle) {
    gnuplot_cmd(handle, "set term pop");
    gnuplot_cmd(handle, "set output");
}

bool check_gnuplot_and_set_default_terminal(struct shell_variables *shell_state) {

    bool gnuplot_installed = can_run_command("gnuplot");

    if (!gnuplot_installed) {
        fprintf(stderr, "Warning - gnuplot was not found. Make sure that it is installed and added to the PATH variable!\n");
        fprintf(stderr, "Plotting commands will not be available!\n");
        shell_state->default_gnuplot_term = NULL;
        return false;
    }

    bool sixel_available = (shell_state->enable_sixel && check_sixel_support()) || shell_state->force_sixel;

    if(!sixel_available) {
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
        } else {
            //I think this will never happen
            shell_state->default_gnuplot_term = strdup("dummy");
        }
        pclose(f);

        if(shell_state->enable_sixel) {
            printf("Warning: sixel gnuplot terminal was requested, but your current terminal does not support sixel!\n");
        }

    }
    else {
        shell_state->default_gnuplot_term = strdup("sixel");
    }

    if(!shell_state->force_sixel) {
        printf("Using %s as gnuplot terminal.\n", shell_state->default_gnuplot_term);
    } else {
        printf("Using sixel as gnuplot terminal (forced, it may not work).\n");
    }
    return true;
}
