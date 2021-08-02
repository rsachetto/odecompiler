#ifndef __COMMAND_CORRECTOR_H
#define __COMMAND_CORRECTOR_H

#define ALPHABET_SIZE        (sizeof(alphabet) - 1)

static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
char *correct(char *word, char **commands, int num_commands);

#endif /* __COMMAND_CORRECTOR_H */
