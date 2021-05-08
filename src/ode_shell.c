#include <errno.h>
#include <linux/limits.h>
#include <math.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>

#include "file_utils/file_utils.h"
#include "string/sds.h"
#include "stb/stb_ds.h"

#include "compiler/parser.h"

#include "code_converter.h"

struct var_index_hash_entry {
    char *key;
    int value;
};

struct model_config {
    char *model_name;
    char *model_command;
    char *output_file;
    char *xlabel;
    char *ylabel;
    program program;
    struct var_index_hash_entry *var_indexes;
    int xindex;
    int yindex;
};

//TODO: move to a header file
struct model_hash_entry {
    char *key;
    struct model_config *value;
};

struct shell_variables {
    struct model_hash_entry *loaded_models;
    char *last_loaded_model;
    FILE *gnuplot_handle;
};
void run_commands_from_file(char *file_name, struct shell_variables *shell_state);

#define STR_EQUALS(s1, s2) (strcmp((s1), (s2)) == 0)
#define PROMPT "ode_shell> "

#define CMD_EXIT        "quit"
#define CMD_LOAD        "load"
#define CMD_RUN         "run"
#define CMD_PLOT        "plot"
#define CMD_REPLOT      "replot"
#define CMD_LIST        "list"
#define CMD_VARS        "vars"
#define CMD_PLOT_SET_X "plotsetx"
#define CMD_PLOT_SET_Y "plotsety"
#define CMD_CD          "cd"
#define CMD_LS          "ls"
#define CMD_PWD         "pwd"
#define CMD_LOAD_CMDS   "load_cmds"

#define HISTORY_FILE ".ode_history"

#define CHECK_ARGS(command, expected, received)                                  \
    do {                                                                         \
        if (!check_command_number_argument((command), (expected), (received))) { \
            return;                                                              \
        }                                                                        \
    } while (0)


static sigjmp_buf env;

void  ctrl_c_handler(int sig) {
    siglongjmp(env, 42);
}

char *command_generator(const char *text, int state) {

    static char *commands[] = {CMD_EXIT, CMD_LOAD, CMD_RUN, CMD_PLOT, CMD_REPLOT, CMD_LIST, CMD_VARS, CMD_PLOT_SET_X, CMD_PLOT_SET_Y, CMD_CD, CMD_LS, CMD_PWD, CMD_LOAD_CMDS};

    static string_array matches = NULL;
    static size_t match_index = 0;

    if (state == 0) {
        arrsetlen(matches, 0);
        match_index = 0;

        int len = sizeof(commands)/sizeof(commands[0]);

        sds textstr = sdsnew(text);
        for (int i = 0; i < len; i++) {
            char *word = commands[i];
            size_t wlen = strlen(word);
            size_t tlen = strlen(textstr);

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
  return rl_completion_matches(text, command_generator);
}

void gnuplot_cmd(FILE *handle, char const *cmd, ...) {
  va_list ap;

  va_start(ap, cmd);
  vfprintf(handle, cmd, ap);
  va_end(ap);

  fputs("\n", handle);
  fflush(handle);

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

long string_to_long(char *string, bool *error) {

    char *endptr = NULL;
    long result;
    *error = false;

    /* reset errno to 0 before call */
    errno = 0;

    result = strtol(string, &endptr, 10);

    /* test return to number and errno values */
    if ( (string == endptr) || (errno != 0 && result == 0) || (errno == 0 && string && *endptr != 0))  {
        *error = true;
    }

    return result;
}

bool generate_program(char *file_name, struct model_config *model) {

    if(!file_exists(file_name)) {
        printf("Error: file %s does not exist!\n", file_name);
        return true;
    }

    size_t file_size;
    const char *source = read_entire_file_with_mmap(file_name, &file_size);

    lexer *l = new_lexer(source, file_name);
    parser *p = new_parser(l);
    program program = parse_program(p);

    bool error = check_parser_errors(p, false);

    if(!error) {
        sh_new_arena(model->var_indexes);
        shdefault(model->var_indexes, -1);

        int n_stmt = arrlen(program);

        shput(model->var_indexes, "t", 1);

        int ode_count = 2;
        for(int i = 0; i < n_stmt; i++) {
            ast *a = program[i];
            if(a->tag == ast_ode_stmt) {
                sds var_name = sdscatprintf(sdsempty(), "%.*s", (int)strlen(a->assignement_stmt.name->identifier.value)-1, a->assignement_stmt.name->identifier.value);
                shput(model->var_indexes, var_name, ode_count);
                ode_count++;

            }
        }
        model->program = program;
    }

    return error;

}

struct model_config *load_model_config(struct shell_variables *shell_state, sds *tokens, int num_args, int model_name_position) {

    char *command = tokens[0];

    const char *model_name;

    if(num_args == model_name_position) {
        model_name = shell_state->last_loaded_model;
    }
    else {
        model_name = tokens[1];
    }

    if(shlen(shell_state->loaded_models) == 0) {
        printf ("Error executing command %s. No models loaded. Load a model first using load modelname.edo\n", command);
        return NULL;
    }

    struct model_config *model_config = shget(shell_state->loaded_models, model_name);

    if(!model_config) {
        printf ("Error executing command %s. model %s is not loaded. Load a model first using \"load %s.edo\" or list loaded models using \"list\"\n", command, model_name, model_name);
        return NULL;
    }

    model_config->model_name = strdup(model_name);

    return model_config;

}

char * get_var_name_from_index(struct var_index_hash_entry *var_indexes, int index) {
    int len = shlen(var_indexes);

    for(int i = 0; i < len; i++) {
        if(var_indexes[i].value == index)
            return var_indexes[i].key;
    }
    return NULL;
}

void parse_and_execute_command(sds line, struct shell_variables *shell_state) {

    int  num_args;
    int token_count;

    sds *tokens = sdssplit(line, " ", &token_count);

    num_args = token_count - 1;

    const char *command = tokens[0];

    if(STR_EQUALS(tokens[0], CMD_EXIT)) {
        CHECK_ARGS(command, 0, num_args);
    }
    else if(STR_EQUALS(command, CMD_LOAD)) {

        CHECK_ARGS(command, 1, num_args);

        char *model_name = get_filename_without_ext(tokens[1]);

        sds compiled_file = sdsnew(model_name);
        compiled_file = sdscat(compiled_file, "_XXXXXX.c");

        struct model_config *model_config = calloc(1, sizeof(struct model_config));

        bool error = generate_program(tokens[1], model_config);

        if(error) return;

        sds compiled_model_name = sdscatfmt(sdsempty(), "%s_auto_compiled_model_tmp_file", model_name);
        model_config->model_command = compiled_model_name;

        model_config->xindex = 1;
        model_config->yindex = 2;

        int fd = mkstemps(compiled_file, 2);

        FILE *outfile = fdopen(fd, "w");
        convert_to_c(model_config->program, outfile, EULER_ADPT_SOLVER);
        fclose(outfile);

        model_config->xlabel = strdup(get_var_name_from_index(model_config->var_indexes, 1));
        model_config->ylabel = strdup(get_var_name_from_index(model_config->var_indexes, 2));

        shput(shell_state->loaded_models, model_name, model_config);
        shell_state->last_loaded_model = strdup(model_name);

        sds compiler_command = sdsnew("gcc");
        compiler_command = sdscatfmt(compiler_command, " %s -o %s -lm", compiled_file, compiled_model_name);

        FILE *fp = popen(compiler_command, "r");
        error = check_and_print_execution_errors(fp);

        unlink(compiled_file);

        //Clean
        pclose(fp);
        sdsfree(compiled_file);
        sdsfree(compiler_command);

        return;

    }
    else if(STR_EQUALS(command, CMD_RUN)) {

        if(num_args != 1 && num_args != 2) {
            printf("Error: command %s accept 1 or 2 argument(s). %d argument(s) given!\n", command, num_args);
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

        struct model_config *model_config = load_model_config(shell_state, tokens, num_args, 1);

        if(!model_config) return;

        sds model_out_file = sdscatfmt(sdsempty(), "%s_out.txt", model_config->model_command);
        model_config->output_file = model_out_file;

        sds model_command = sdsnew("./");
        model_command = sdscat(model_command, model_config->model_command);

        model_command = sdscatprintf(model_command, " %lf %s", simulation_steps, model_config->output_file);

        FILE *fp = popen(model_command, "r");
        bool error = check_and_print_execution_errors(fp);
        pclose(fp);

        sdsfree(model_command);
        return;

    }
    else if(STR_EQUALS(command, CMD_PLOT) || STR_EQUALS(command, CMD_REPLOT)) {

        if(num_args != 0 && num_args != 1) {
            printf("Error: command %s accept 0 or 1 argument(s). %d argument(s) given!\n", command, num_args);
            return;
        }

        struct model_config *model_config = load_model_config(shell_state, tokens, num_args, 0);

        if(!model_config) return;

        if(!model_config->output_file) {
            printf ("Error executing command %s. model %s was not executed. Run then model first using \"run %s\" or list loaded models using \"list\"\n", command, model_config->model_name, model_config->model_name);
            return;
        }

        if(shell_state->gnuplot_handle == NULL) {
            if(STR_EQUALS(command, CMD_REPLOT)) {
                printf ("Error executing command %s. no previous plot. plot the model first using \"plot modelname\" or list loaded models using \"list\"\n", command);
                return;
            }

            shell_state->gnuplot_handle = popen("gnuplot", "w");
        }

        gnuplot_cmd(shell_state->gnuplot_handle, "set xlabel \"%s\"", model_config->xlabel);
        gnuplot_cmd(shell_state->gnuplot_handle, "set ylabel \"%s\"", model_config->ylabel);
        gnuplot_cmd(shell_state->gnuplot_handle, "%s '%s' u %d:%d title \"%s\" w lines lw 2", command, model_config->output_file, model_config->xindex, model_config->yindex, model_config->ylabel);

        return;


    } else if(STR_EQUALS(command, CMD_LIST)) {

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

        return;

    }
    else if(STR_EQUALS(command, CMD_VARS)) {

        if(num_args != 0 && num_args != 1) {
            printf("Error: command %s accept 0 or 1 argument(s). %d argument(s) given!\n", command, num_args);
            return;
        }

        struct model_config *model_config = load_model_config(shell_state, tokens, num_args, 0);

        if(!model_config) return;


        int len = shlen(model_config->var_indexes);

        printf("Model vars for model %s:\n", model_config->model_name);
        for(int i = 0; i < len; i++) {
           printf("%s, %d\n", model_config->var_indexes[i].key, model_config->var_indexes[i].value);
        }

        return;
    }
    else if(STR_EQUALS(command, CMD_PLOT_SET_X) || STR_EQUALS(command, CMD_PLOT_SET_Y)) {

        if(num_args != 1 && num_args != 2) {
            printf("Error: command %s accept 1 or 2 argument(s). %d argument(s) given!\n", command, num_args);
            return;
        }

        struct model_config *model_config = load_model_config(shell_state, tokens, num_args, 1);

        if(!model_config) return;

        char *index_str;

        if(num_args == 1) {
            index_str = tokens[1];
        }
        else {
            index_str = tokens[2];
        }

        bool index_as_str;
        int index = (int)string_to_long(index_str, &index_as_str);

        if(index_as_str) {
            //string was passed, try to get the index from the var_indexes on model_config
            index = shget(model_config->var_indexes, index_str);

            if(index == -1) {
                printf ("Error parsing command %s. Invalid variable name: %s. You can list model variable name using vars %s\n", command, index_str, model_config->model_name);
                return;
            }
        }

        char *var_name = NULL;

        if(!index_as_str) {

            var_name = get_var_name_from_index(model_config->var_indexes, index);

            if(!var_name) {
                printf ("Error parsing command %s. Invalid index: %d. You can list model variable name using vars %s\n", command, index, model_config->model_name);
                return;

            }
        }

        if(STR_EQUALS(command, CMD_PLOT_SET_X)) {
            model_config->xindex = index;
            if(index_as_str) {
                model_config->xlabel = strdup(index_str);
            }
            else {
                model_config->xlabel = strdup(var_name);
            }
        }
        else {
            model_config->yindex = index;
            if(index_as_str) {
                model_config->ylabel = strdup(index_str);
            }
            else {
                model_config->ylabel = strdup(var_name);
            }
        }
        return;
    }
    else if(STR_EQUALS(command, CMD_CD)) {
        CHECK_ARGS(command, 1, num_args);
        char *path = tokens[1];

        int ret = chdir(path);
        if(ret == -1) {
            printf("Error changing working directory to %s\n", path);
        }
        else {
           print_current_dir();
        }
    }
    else if(STR_EQUALS(command, CMD_PWD)) {
        CHECK_ARGS(command, 0, num_args);
        print_current_dir();
    }
    else if(STR_EQUALS(command, CMD_LOAD_CMDS)) {
        CHECK_ARGS(command, 1, num_args);
        char *file_name = tokens[1];
        run_commands_from_file(file_name, shell_state);
    }
    else if(STR_EQUALS(command, CMD_LS)) {

        if(num_args != 0 && num_args != 1) {
            printf("Error: command %s accept 0 or 1 argument(s). %d argument(s) given!\n", command, num_args);
            return;
        }

        char *path_name;

        if(num_args == 0) {
            path_name = ".";
        }
        else {
            path_name = tokens[1];
        }

        print_path_contents(path_name);
    }
    else {
        printf("Invalid command: %s\n", command);
        return;
    }
}

void setup_ctrl_c_handler() {
    /* Setup SIGINT */
    struct sigaction s;
    s.sa_handler = ctrl_c_handler;
    sigemptyset(&s.sa_mask);
    s.sa_flags = SA_RESTART;
    sigaction(SIGINT, &s, NULL);
}

void run_commands_from_file(char *file_name, struct shell_variables *shell_state) {
    if (!file_exists(file_name)) {
        printf("File %s does not exist!\n", file_name);
    } else {
        FILE *f = fopen(file_name, "r");

        if (!f) {
            printf("Error opening file %s for reading!\n", file_name);
        } else {
            char *line = NULL;
            size_t len = 0;
            sds command;

            while ((getline(&line, &len, f)) != -1) {
                command = sdsnew(line);
                command = sdstrim(command, "\n ");
                add_history(command);
                parse_and_execute_command(command, shell_state);
                printf("%s%s\n", PROMPT, command);
                sdsfree(command);
            }

            fclose(f);
            if (line)
                free(line);
        }
    }
}

int main(int argc, char **argv) {

    print_current_dir();

    rl_attempted_completion_function = command_completion;

    struct shell_variables shell_state = {0};

    shdefault(shell_state.loaded_models, NULL);
    sh_new_strdup(shell_state.loaded_models);

    if(argc == 2) {
        run_commands_from_file(argv[1], &shell_state);
    }

    sds history_path = sdsnew(get_home_dir());
    history_path = sdscatfmt(history_path, "/%s", HISTORY_FILE);

    read_history(history_path);

    setup_ctrl_c_handler();

    sds command;
    char *line;

    //if CTRL+C is pressed, we print a new line
    if (sigsetjmp(env, 1) == 42) {
        printf("\n");
    }

    while ((line = readline(PROMPT)) != 0) {
        if(*line) {
            command = sdsnew(line);
            command = sdstrim(command, "\n ");
            add_history(command);
            if(STR_EQUALS(command, CMD_EXIT)) {
                break;
            }
            parse_and_execute_command(command, &shell_state);
            sdsfree(command);
        }
    }

    int n_models = shlen(shell_state.loaded_models);
    for(int i = 0; i < n_models; i++) {
        struct model_config *config = shell_state.loaded_models[i].value;
        if(config->output_file) {
            unlink(config->output_file);
        }
        if(config->model_command) {
            unlink(config->model_command);
        }
    }

    printf("\n");
    write_history(history_path);
    exit(0);

}

