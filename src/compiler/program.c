#include "program.h"
#include "../stb/stb_ds.h"
#include <stdio.h>

extern int indentation_level;

void print_program(program p) {

    sds *p_str = program_to_string(p);

    int n = arrlen(p_str);

    for(int i = 0; i < n; i++) {
        printf("%s\n", p_str[i]);
        sdsfree(p_str[i]);
    }

    arrfree(p_str);
}

sds *program_to_string(program p) {

    indentation_level = 0;

    int n_stmt = arrlen(p);
    sds *return_str = NULL;

    for (int i = 0; i < n_stmt; i++) {
        sds s = ast_to_string(p[i]);
        if (s) {
            arrput(return_str, s);
        }
    }

    return return_str;
}

program copy_program(program src_program) {

    program dst_program = NULL;

    int p_len = arrlen(src_program);

    for (int i = 0; i < p_len; i++) {
        ast *a = copy_ast(src_program[i]);
        arrput(dst_program, a);
    }

    return dst_program;
}

void free_program(program src_program) {
    free_asts(src_program);
}

