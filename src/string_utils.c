#include "string_utils.h"
#include <stdlib.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "stb/stb_ds.h"

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

string_array parse_input(sds line, bool *error) {

    *error = false;

    string_array tokens = NULL;

    sds par = sdsempty();

    int l_size = sdslen(line);
    bool mark_begin = false;
    bool mark_end = false;

    bool wrong_mark = false;

    for(int i = 0; i < l_size; i++) {

        if(line[i] == '\"') {

            if(!mark_begin) {
                mark_begin = true;
                mark_end = false;
            }
            else {

                if(i < l_size - 1) {
                    if(!isspace(line[i+1])) {
                        wrong_mark = true;
                    }
                }

                mark_begin = false;
                mark_end = true;
            }
            continue;
        }

        if(!mark_begin) {
            if(!isspace(line[i])) {
                par = sdscatprintf(par, "%c", line[i]);
            }
            else {
                arrput(tokens, sdsnew(par));
                sdsfree(par);
                par = sdsempty();
            }
        }

        else {
            if(!mark_end) {
                par = sdscatprintf(par, "%c", line[i]);
            }
            else {
                arrput(tokens, sdsnew(par));
                sdsfree(par);
                par = sdsempty();
            }
        }
    }

    //copy the last token
    arrput(tokens, sdsnew(par));

    int token_count = arrlen(tokens);


    if(mark_begin && !mark_end) {
        printf("Error - unterminated parameter!\n");
        *error = true;
    }

    if(wrong_mark) {
        printf("Error - invalid \" in the middle of parameter!\n");
        *error = true;
    }


    for(int i = 0; i < token_count; i++) {
        tokens[i] = sdstrim(tokens[i], " ");
    }

    return tokens;

}
