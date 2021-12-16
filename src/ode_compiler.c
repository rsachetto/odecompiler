#include <stdio.h>
#include <stdlib.h>

#include "compiler/parser.h"
#include "code_converter.h"

#include "file_utils/file_utils.h"
#include <argp.h>

const char *argp_program_version = "odecompiler 0.1";
const char *argp_program_bug_address = "<rsachetto@gmail.com>";

/* Program documentation. */
static char doc[] = "Simple compiler for ODE description language.";

/* A description of the arguments we accept. */
static char args_doc[] = "";

/* The options we understand. */
static struct argp_option options[] = {
  {"input",        'i', "FILE", 0, "Input .ode FILE" },
  {"output",       'o', "FILE", 0, "Output FILE" },
  {"solver_impl",  't', "IMPL", 0, "Solver implementation. Available options: cvode, euler. Default: euler"},
  { 0 }
};

/* Used by main to communicate with parse_opt. */
struct arguments {
  char *input_file;
  char *output_file;
  char *solver_impl;
};

/* Parse a single option. */
static error_t parse_opt (int key, char *arg, struct argp_state *state) {

    struct arguments *arguments = state->input;

    switch (key) {
        case 'i':
            arguments->input_file = arg;
            break;
        case 'o':
            arguments->output_file = arg;
            break;
        case 't':
            arguments->solver_impl = arg;
            break;

        case ARGP_KEY_END:
            if (arguments->input_file == NULL || arguments->output_file == NULL) {
                argp_usage (state);
            }
            break;

        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

int main(int argc, char **argv) {

    struct arguments arguments = {0};
    arguments.solver_impl = "euler";

    argp_parse (&argp, argc, argv, ARGP_NO_ARGS, 0, &arguments);

    char *file_name = arguments.input_file;

    if(!file_exists(file_name)) {
        fprintf(stderr, "Error: file %s does not exist!\n", file_name);
        exit(EXIT_FAILURE);
    }

    size_t file_size;
    const char *source = read_entire_file_with_mmap(file_name, &file_size);

    lexer *l = new_lexer(source, file_name);
    parser *p = new_parser(l);
    program program = parse_program(p, true, true);

    check_parser_errors(p, true);

    FILE *outfile = fopen(arguments.output_file, "w");
    solver_type solver_type = EULER_ADPT_SOLVER;

    if(STR_EQUALS(arguments.solver_impl, "cvode")) {
        solver_type = CVODE_SOLVER;
    }
    else if(STR_EQUALS(arguments.solver_impl, "euler")) {
        solver_type = EULER_ADPT_SOLVER;
    }
    else {
        fprintf(stderr, "Error - invalid implementation %s. Available options: cvode, euler. Default: euler\n", arguments.solver_impl);
    }

    convert_to_c(program, outfile, solver_type);
    free_lexer(l);
    free_parser(p);
    free_program(program);
    fclose(outfile);
}
