#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <search.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>

#include "command_corrector.h"

static bool first_call = true;

static char *strtolower(char *word) {
    for (char *s = word; *s; ++s) *s = tolower(*s);
    return word;
}

static ENTRY *find(char *word) {
    return hsearch((ENTRY){.key = word}, FIND);
}

static int update(char *word) {
    ENTRY *e = find(word);

    if (!e) return 0;

    e->data++;
    return 1;
}

static int start_dict(char **commands, int num_commands, ENTRY dict) {

    char *w = NULL;
    for(int i = 0; i < num_commands; i++) {
        char *word = commands[i];
        w = strtolower(strdup(word));

        if (!update(w)) {
            dict.key  = w;
            dict.data = (void *) 1;
            hsearch(dict, ENTER);
        }
    }

    return 1;
}

static void *checked_malloc(int len) {
    void *ret = malloc(len);

    if (ret == NULL) {
        fprintf(stderr, "Out of memory\n");
        exit(0);
    }
    return ret;
}

/**
 * Takes a part of the source string and appends it to the destination string.
 *
 * @param     dst       Destination string to append to.
 * @param     dstLen    Current length of the destination string.  This will
 *                      be updated with the new length after appending.
 * @param     src       Source string.
 * @param     srcBegin  Starting index in the source string to copy from.
 * @param     len       Length of portion to copy.
 */
static void append(char *dst, int *dstLen, const char *src, int srcBegin, int len) {
    if (len > 0) {
        memcpy(&dst[*dstLen], &src[srcBegin], len);
        *dstLen += len;
    }
    dst[*dstLen] = 0;
}

static void array_cleanup(char **array, int rows) {
    for (int i = 0; i < rows; i++)
    {
        free(array[i]);
    }
}

static int deletion(char *word, char **array, int start_idx) {
    int i = 0;
    size_t word_len = strlen(word);

    for (; i < word_len; i++)
    {
        int pos = 0;
        array[i+start_idx] = checked_malloc(word_len);
        append(array[i+start_idx], &pos, word, 0, i);
        append(array[i+start_idx], &pos, word, i+1, word_len-(i+1));
    }
    return i;
}

static int transposition(char *word, char **array, int start_idx) {
    int i = 0;
    size_t word_len = strlen(word);

    for (; i < word_len-1; i++)
    {
        int pos = 0;
        array[i+start_idx] = checked_malloc(word_len+1);
        append(array[i+start_idx], &pos, word, 0,   i);
        append(array[i+start_idx], &pos, word, i+1, 1);
        append(array[i+start_idx], &pos, word, i,   1);
        append(array[i+start_idx], &pos, word, i+2, word_len-(i+2));
    }
    return i;
}

static int alteration(char *word, char **array, int start_idx) {
    int k = 0;
    size_t word_len = strlen(word);
    char c[2] = {};

    for (int i = 0; i < word_len; ++i)
    {
        for (int j = 0; j < ALPHABET_SIZE; ++j, ++k)
        {
            int pos = 0;
            c[0] = alphabet[j];
            array[k+start_idx] = checked_malloc(word_len+1);
            append(array[k+start_idx], &pos, word, 0, i);
            append(array[k+start_idx], &pos, c   , 0, 1);
            append(array[k+start_idx], &pos, word, i+1, word_len-(i+1));
        }
    }
    return k;
}

static int insertion(char *word, char **array, int start_idx) {
    int k = 0;
    size_t word_len = strlen(word);
    char c[2] = {};

    for (int i = 0; i <= word_len; ++i)
    {
        for (int j = 0; j < ALPHABET_SIZE; ++j, ++k)
        {
            int pos = 0;
            c[0] = alphabet[j];
            array[k+start_idx] = checked_malloc(word_len+2);
            append(array[k+start_idx], &pos, word, 0, i);
            append(array[k+start_idx], &pos, c   , 0, 1);
            append(array[k+start_idx], &pos, word, i, word_len-i);
        }
    }
    return k;
}

static size_t edits1_rows(char *word) {
    size_t size = strlen(word);

    return (size)                + // deletion
        (size - 1)                   + // transposition
        (size * ALPHABET_SIZE)       + // alteration
        (size + 1) * ALPHABET_SIZE;    // insertion
}

static char **edits1(char *word) {
    int next_idx;
    char **array = malloc(edits1_rows(word) * sizeof(char *));

    if (!array) return NULL;

    next_idx  = deletion(word, array, 0);
    next_idx += transposition(word, array, next_idx);
    next_idx += alteration(word, array, next_idx);
    insertion(word, array, next_idx);

    return array;
}

static int array_exist(char **array, int rows, char *word) {
    for (int i = 0; i < rows; ++i)
    {
        if (!strcmp(array[i], word)) return 1;
    }
    return 0;
}

static char **known_edits2(char **array, int rows, int *e2_rows) {
    size_t e1_rows = 0;
    int res_size = 0;
    int res_max  = 0;
    char **res = NULL;
    char **e1  = NULL;

    for (int i = 0; i < rows; i++)
    {
        if(array[i][0] == '\0') continue;

        e1      = edits1(array[i]);
        e1_rows = edits1_rows(array[i]);

        for (int j = 0; j < e1_rows; j++)
        {
            if (find(e1[j]) && !array_exist(res, res_size, e1[j]))
            {
                if (res_size >= res_max) {
                    // First time, allocate 50 entries.  After that, double
                    // the size of the array.
                    if (res_max == 0)
                        res_max = 50;
                    else
                        res_max *= 2;
                }
                res = realloc(res, sizeof(char *) * res_max);
                res[res_size++] = strdup(e1[j]);
            }
        }
        array_cleanup(e1, e1_rows);
        free(e1);
    }

    *e2_rows = res_size;

    return res;
}

static char *max(char **array, int rows) {
    char *max_word = NULL;
    size_t max_size = 0;
    ENTRY *e;

    for (int i = 0; i < rows; i++)
    {
        e = find(array[i]);
        if (e && ((size_t) e->data > max_size))
        {
            max_size = (size_t) e->data;
            max_word = e->key;
        }
    }

    return max_word;
}

char *correct(char *word, char **commands, int num_commands) {


    static ENTRY dict = {};

    if(first_call) {
        hcreate(num_commands);
        start_dict(commands, num_commands, dict);
        first_call = false;
    }

    char **e1 = NULL;
    char **e2 = NULL;
    char *e1_word = NULL;
    char *e2_word = NULL;
    char *res_word = word;
    int e1_rows = 0;
    int e2_rows = 0;

    if (find(word)) return word;

    e1_rows = (unsigned)edits1_rows(word);
    if (e1_rows)
    {
        e1 = edits1(word);
        e1_word = max(e1, e1_rows);

        if (e1_word)
        {
            array_cleanup(e1, e1_rows);
            free(e1);
            return e1_word;
        }
    }

    e2 = known_edits2(e1, e1_rows, &e2_rows);
    if (e2_rows)
    {
        e2_word = max(e2, e2_rows);
        if (e2_word)
            res_word = e2_word;
    }

    array_cleanup(e1, e1_rows);
    array_cleanup(e2, e2_rows);

    free(e1);
    free(e2);

    return res_word;
}
