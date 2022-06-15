#ifndef __STRING_UTILS_H
#define __STRING_UTILS_H 

#include <stdbool.h>
#include <string.h>
#include "string/sds.h"

#define STR_EQUALS(s1, s2) (strcmp((s1), (s2)) == 0)

typedef char **string_array;

int string_cmp(const void *a, const void *b);
double string_to_double(char *string);
long string_to_long(char *string, bool *error);
void strip_extra_spaces(char* str);
bool string_ends_with(const char *str, const char *suffix);

#endif /* __STRING_UTILS_H */
