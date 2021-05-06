#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/limits.h>
#include <math.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "file_utils/file_utils.h"
#include "string/sds.h"
#include "stb/stb_ds.h"

struct model_run_config {
    char *model_command;
    char *output_file;
};


//TODO: move to a header file
struct model_hash_entry {
    char *key;
    struct model_run_config *value;
};

struct shell_variables {
    const char *compiler_path;
    struct model_hash_entry *loaded_models;
    char *last_loaded_model;
};

#define STR_EQUALS(s1, s2) (strcmp((s1), (s2)) == 0)
#define PROMPT "ode_shell> "

#define CMD_EXIT "quit"
#define CMD_LOAD "load"
#define CMD_RUN  "run"
#define CMD_PLOT "plot"
#define CMD_LIST "list"

#define CHECK_ARGS(command, expected, received)                                  \
    do {                                                                         \
        if (!check_command_number_argument((command), (expected), (received))) { \
            return;                                                              \
        }                                                                        \
    } while (0)


char *command_generator(const char *text, int state) {

    static char *commands[] = {CMD_EXIT, CMD_LOAD, CMD_RUN, CMD_PLOT};

    static string_array matches = NULL;
    static size_t match_index = 0;

    if (state == 0) {
        arrsetlen(matches, 0);
        match_index = 0;

        int len = sizeof(commands)/sizeof(commands[0]);

        sds textstr = sdsnew(text);
        for (int i = 0; i < len; i++) {
            char *word = commands[i];
            int wlen = strlen(word);
            int tlen = strlen(textstr);

            if (wlen >= sdslen(textstr) &&  strncmp(word, textstr, tlen) == 0) {
                arrput(matches, word);
            }
        }
    }

    if (match_index >= arrlen(matches)) {
        return NULL;
    } else {
        return strdup(matches[match_index++]);
    }
}

char **command_completion(const char *text, int start, int end) {
  //rl_attempted_completion_over = 1;
  return rl_completion_matches(text, command_generator);
}

void gnuplot_cmd(FILE *handle, char const *cmd, ...) {
  va_list ap;

  va_start(ap, cmd);
  vfprintf(handle, cmd, ap);
  va_end(ap);

  fputs("\n", handle);
  fflush(handle);
  return;
}

bool check_command_number_argument(const char* command, int expected_args, int num_args) {
    if(expected_args != num_args) {
        printf("Error: command %s accept %d argument(s). %d argument(s) given!\n", command, expected_args, num_args);
        return false;
    }

    return true;
}

bool check_and_print_execution_errors(FILE *fp) {

    bool error = false;
    char msg[PATH_MAX];

    while (fgets(msg, PATH_MAX, fp) != NULL) {
        printf("%s", msg);
        if(!error) error = true;
    }

    return error;

}

double string_to_double(char *string) {

    char *endptr = NULL;
    double result = 0;

    /* reset errno to 0 before call */
    errno = 0;

    result = strtod(string, &endptr);

    /* test return to number and errno values */
    if ( (string == endptr) || (errno != 0 && result == 0) || (errno == 0 && string && *endptr != 0))  {
        return NAN;
    }

    return result;

}

void parse_and_execute_command(sds line, struct shell_variables *shell_state) {

    int  num_args;
    int token_count;

    sds *tokens = sdssplit(line, " ", &token_count);

    num_args = token_count - 1;

    const char *command = tokens[0];

    if(STR_EQUALS(tokens[0], CMD_EXIT)) {
        CHECK_ARGS(command, 0, num_args);
        exit(0);
    }
    else if(STR_EQUALS(tokens[0], CMD_LOAD)) {

        CHECK_ARGS(command, 1, num_args);

        char *model_name = get_filename_without_ext(tokens[1]);

        sds compiled_file = sdsnew(model_name);
        compiled_file = sdscat(compiled_file, ".c");

        sds compiler_command = sdsnew(shell_state->compiler_path);
        compiler_command = sdscatfmt(compiler_command, " %s %s", tokens[1], compiled_file);

        FILE *fp = popen(compiler_command, "r");

        bool error = check_and_print_execution_errors(fp);

        pclose(fp);

        if(error) return;

        //no parsing errors, we can compile the solver
        sdsfree(compiler_command);

        sds compiled_model_name = sdscatfmt(sdsempty(), "%s_auto_compiled_model", model_name);

        //TODO: check this with models in different folders
        struct model_run_config *model_config = calloc(1, sizeof(struct model_run_config));
        model_config->model_command = compiled_model_name;

        shput(shell_state->loaded_models, model_name, model_config);
        shell_state->last_loaded_model = strdup(model_name);

        //TODO: we need to ship cvode with the compiler
        compiler_command = sdsnew("gcc -I /home/sachetto/sundials/instdir/include ");
        compiler_command = sdscatfmt(compiler_command, "%s -o %s -lm -L/home/sachetto/sundials/instdir/lib -lsundials_cvode", compiled_file, compiled_model_name);

        fp = popen(compiler_command, "r");
        error = check_and_print_execution_errors(fp);

        //Clean
        pclose(fp);
        sdsfree(compiled_file);
        sdsfree(compiler_command);

    }
    else if(STR_EQUALS(tokens[0], CMD_RUN)) {

        if(num_args != 1 && num_args != 2) {
            printf("Error: command %s accept 1 or 2 argument(s). %d argument(s) given!\n", command, num_args);
            return;
        }

        if(shlen(shell_state->loaded_models) == 0) {
            printf ("Error executing command %s. No models loaded. Load a model first using load modefile.edo\n", command);
            return;
        }

        char *simulation_steps_str;
        double simulation_steps = 0;

        if(num_args == 1) {
            simulation_steps_str = tokens[1];
        }
        else {
            simulation_steps_str = tokens[2];
        }

        simulation_steps = string_to_double(simulation_steps_str);

        if(isnan(simulation_steps) || simulation_steps == 0) {
            printf ("Error parsing command %s. Invalid number: %s\n", command, simulation_steps_str);
            return;
        }

        const char *model_to_run;

        if(num_args == 1) {
            model_to_run = shell_state->last_loaded_model;
        }
        else {
            model_to_run = tokens[1];
        }

        struct model_run_config *model_config = shget(shell_state->loaded_models, model_to_run);

        if(!model_config) {
            printf ("Error executing command %s. model %s is not loaded. Load a model first using \"load modefile.edo\" or list loaded models using \"list\"\n", command, model_to_run);
            return;
        }

        sds model_out_file = sdscatfmt(sdsempty(), "%s_out.txt", model_config->model_command);
        model_config->output_file = model_out_file;

        sds model_command = sdsnew("./");
        model_command = sdscat(model_command, model_config->model_command);

        model_command = sdscatprintf(model_command, " %lf %s", simulation_steps, model_config->output_file);

        FILE *fp = popen(model_command, "r");
        bool error = check_and_print_execution_errors(fp);
        pclose(fp);

        sdsfree(model_command);

    }
    else if(STR_EQUALS(tokens[0], CMD_PLOT)) {

        //TODO: accept more arguments (what to plot)
        if(num_args != 0 && num_args != 1) {
            printf("Error: command %s accept 0 or 1 argument(s). %d argument(s) given!\n", command, num_args);
            return;
        }

        if(shlen(shell_state->loaded_models) == 0) {
            printf ("Error executing command %s. No models loaded. Load a model first using load modefile.edo\n", command);
            return;
        }

        const char *model_to_plot;

        if(num_args == 0) {
            model_to_plot = shell_state->last_loaded_model;
        }
        else {
            model_to_plot = tokens[1];
        }

        struct model_run_config *model_config = shget(shell_state->loaded_models, model_to_plot);

        if(!model_config) {
            printf ("Error executing command %s. model %s is not loaded. Load a model first using \"load modefile.edo\" or list loaded models using \"list\"\n", command, model_to_plot);
            return;
        }

        if(!model_config->output_file) {
            printf ("Error executing command %s. model %s was not executed. Run then model first using \"run modelname\" or list loaded models using \"list\"\n", command, model_to_plot);
            return;
        }

        FILE *fp = popen("gnuplot", "w");
        gnuplot_cmd(fp, "plot '%s' u 1:2 w lines", model_config->output_file);
        //pclose(fp);


    } else if(STR_EQUALS(tokens[0], CMD_LIST)) {

        int len = shlen(shell_state->loaded_models);

        if(len == 0) {
            printf ("No models loaded\n");
            return;
        }

        printf("Last loaded model: %s\n", shell_state->last_loaded_model);
        printf("Loaded models:\n");
        for(int i = 0; i < len; i++) {
           printf("%s\n", shell_state->loaded_models[i].key);
        }

    }
    else {
        printf("Invalid command: %s\n", command);
    }
}


int main(int argc, char **argv) {


    rl_attempted_completion_function = command_completion;

    struct shell_variables shell_state = {0};

    sds command;
    char *line;

    char compiler_path[PATH_MAX];
    sprintf(compiler_path, "%sodec", get_executable_dir());

    shell_state.compiler_path = compiler_path;
    shdefault(shell_state.loaded_models, NULL);
    sh_new_strdup(shell_state.loaded_models);

    while ((line = readline(PROMPT)) != 0) {
        if(*line) {
            add_history(line);
            command = sdsnew(line);
            command = sdstrim(command, "\n ");
            parse_and_execute_command(command, &shell_state);
            sdsfree(command);
        }
    }

    printf("\n");
    exit(0);

}

