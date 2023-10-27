#include "string_utils.h"
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>

int string_cmp(const void *a, const void *b) {
    const char **ia = (const char **)a;
    const char **ib = (const char **)b;
    return strcmp(*ia, *ib);
}

double string_to_double(const char *string) {

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

long string_to_long(const char *string, bool *error) {

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