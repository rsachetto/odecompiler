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

double string_to_double(char *string) {

    char *endptr = NULL;
    double result = 0;

    /* reset errno to 0 before call */
    errno = 0;

    result = strtod(string, &endptr);

    /* test return to number and errno values */
    if ( (string == endptr) || (errno != 0 && result == 0) || (errno == 0 && string && *endptr != 0))  {
        return NAN;
    }

    return result;

}

long string_to_long(char *string, bool *error) {

    char *endptr = NULL;
    long result;
    *error = false;

    /* reset errno to 0 before call */
    errno = 0;

    result = strtol(string, &endptr, 10);

    /* test return to number and errno values */
    if ( (string == endptr) || (errno != 0 && result == 0) || (errno == 0 && string && *endptr != 0))  {
        *error = true;
    }

    return result;
}
