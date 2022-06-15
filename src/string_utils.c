#include "string_utils.h"
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

int string_cmp(const void *a, const void *b) {
    const char **ia = (const char **)a;
    const char **ib = (const char **)b;
    return strcmp(*ia, *ib);
}

double string_to_double(char *string) {

    if(string == NULL) return NAN;

    char *endptr = NULL;
    double result;

    /* reset errno to 0 before call */
    errno = 0;

    result = strtod(string, &endptr);

    /* test return to number and errno values */
    if ( (string == endptr) || (errno != 0 && result == 0) || (errno == 0 && *endptr != 0))  {
        return NAN;
    }

    return result;

}

long string_to_long(char *string, bool *error) {

    if(string == NULL) {
        *error = true;
        return 0;
    }

    char *endptr = NULL;
    long result;
    *error = false;

    /* reset errno to 0 before call */
    errno = 0;

    result = strtol(string, &endptr, 10);

    /* test return to number and errno values */
    if ( (string == endptr) || (errno != 0 && result == 0) || (errno == 0 && *endptr != 0))  {
        *error = true;
    }

    return result;
}

void strip_extra_spaces(char* str) {
    int i, x;
    for(i=x=0; str[i]; ++i) {
        if(!isspace(str[i]) || (i > 0 && !isspace(str[i-1]))) {
            str[x++] = str[i];
        }
    }
    str[x] = '\0';
}

bool string_ends_with(const char *str, const char *suffix) {
    if (!str || !suffix)
        return false;

    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);

    if (lensuffix >  lenstr)
        return false;

    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

