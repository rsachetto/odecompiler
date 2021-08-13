#ifndef __TO_LATEX_H
#define __TO_LATEX_H

#include "string/sds.h"
#include "compiler/token.h"
#include "compiler/ast.h"
#include <stdbool.h>

sds *odes_to_latex(program p);
sds ast_to_latex(ast *a);

#endif //__TO_LATEX_H
