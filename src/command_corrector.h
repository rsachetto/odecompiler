#ifndef COMMAND_CORRECTOR_H
#define COMMAND_CORRECTOR_H

#define ALPHABET_SIZE        (sizeof(alphabet) - 1)

static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789";
char *correct(char *word);
void initialize_corrector(char **commands, int num_commands);

#endif /* COMMAND_CORRECTOR_H */
