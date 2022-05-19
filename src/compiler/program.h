#ifndef __PROGRAM_H
#define __PROGRAM_H

#include "ast.h"

typedef ast **program;

void print_program(program p);
sds *program_to_string(program p);
program copy_program(program src);
void free_program(program src_program);

#endif //__PROGRAM_H
