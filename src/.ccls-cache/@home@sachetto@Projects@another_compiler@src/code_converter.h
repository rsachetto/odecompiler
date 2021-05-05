#ifndef __C_CONVERTER_H
#define __C_CONVERTER_H 

#include <stdio.h>
#include "compiler/parser.h"

typedef struct declared_variable_entry_t {
    char *key;
    int value;
} declared_variable_entry;

typedef declared_variable_entry * declared_variable_hash;

void convert_to_c(program p, FILE *out);
	
#endif /* __C_CONVERTER_H */
