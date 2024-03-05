#ifndef __STRING_UTILS_H
#define __STRING_UTILS_H 

#include <stdbool.h>
#include <string.h>
#include "string/sds.h"

#define STR_EQUALS(s1, s2) (strcmp((s1), (s2)) == 0)

typedef char **string_array;

int    string_cmp(const void *a, const void *b);
long   string_to_long(const char *string, bool *error);
double string_to_double(const char *string);

#endif /* __STRING_UTILS_H */
