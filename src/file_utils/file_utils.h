//
// Created by sachetto on 18/10/17.
//

#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>

#define FILE_HAS_EXTENSION(file_ext__, ext__) (strcmp(file_ext__, ext__) == 0)

#ifndef __linux__
#define PATH_MAX 4096
#endif

bool can_run_command(const char *cmd);
char *get_current_directory(void);
const char *get_filename_ext(const char *filename);
int cp_file(const char *to, const char *from, bool overwrite);
char *read_entire_file_with_mmap(const char *filename, size_t *size);
bool file_exists(const char *path);

char *get_filename_without_ext(const char *filename);
char *get_dir_from_path(const char *path);
char *get_file_from_path(const char *path);
const char *get_home_dir(void);
void print_current_dir(void);
void print_path_contents(const char *path);
#endif // FILE_UTILS_H
