#ifndef __STRING_UTILS_H
#define __STRING_UTILS_H 

#include <stdbool.h>

#define STR_EQUALS(s1, s2) (strcmp((s1), (s2)) == 0)

typedef char **string_array;

double string_to_double(char *string);
long string_to_long(char *string, bool *error);

#endif /* __STRING_UTILS_H */
