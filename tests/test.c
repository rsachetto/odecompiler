////
//// Created by sachetto on 06/10/17.
////
#include <criterion/criterion.h>
#include <sys/mman.h>
#include <criterion/internal/assert.h>
#include <signal.h>
#include "../src/file_utils/file_utils.h"
#include "../src/compiler/lexer.h"
#include "../src/compiler/parser.h"
#include "../src/code_converter.h"

Test (compiler, ToRORd) {

    size_t size_expected;
    size_t size_converted;


    const char *source_file = "ToRORd.ode";

    size_t file_size;
    const char *source = read_entire_file_with_mmap(source_file, &file_size);

    lexer *l = new_lexer(source, source_file);
    parser *p = new_parser(l);
    program program = parse_program(p, true, true, NULL);

    check_parser_errors(p, true);

    const char *out_file = "converted.c";

    FILE *outfile = fopen(out_file, "w");

    convert_to_c(program, outfile, EULER_ADPT_SOLVER);
    free_lexer(l);
    free_parser(p);
    free_program(program);
    fclose(outfile);

    char *expected = read_entire_file_with_mmap("ToRORd.c", &size_expected);
    char *converted = read_entire_file_with_mmap(out_file, &size_converted);

    cr_expect_eq(size_expected, size_converted);

    munmap(expected, size_expected);
    munmap(converted, size_converted);

}


