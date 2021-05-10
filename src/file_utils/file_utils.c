//
// Created by sachetto on 18/10/17.
//

#include "file_utils.h"

#define STB_DS_IMPLEMENTATION
#include "../stb/stb_ds.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

FILE *open_file_or_exit(char *filename, char *mode) {

    FILE *f = fopen(filename, mode);

    if(!f) {
        fprintf(stderr, "Error! Could not open %s!\n", filename);
        exit(EXIT_FAILURE);
    }

    return f;
}

char *get_current_directory() {
    long size;
    char *buf = NULL;
    size = pathconf(".", _PC_PATH_MAX);

    if((buf = (char *)malloc((size_t)size)) != NULL)
        buf = getcwd(buf, (size_t)size);

    buf = strcat(buf, "/");
    return buf;
}

const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename)
        return NULL;
    return dot + 1;
}

char *get_dir_from_path(const char *path) {
    char *last_slash = NULL;
    char *parent = NULL;
    last_slash = strrchr(path, '/');


	if(last_slash == NULL)
    	parent = strdup(".");
	else
    	parent = strndup(path, last_slash - path + 1);

    return parent;
}

char *get_file_from_path(const char *path) {
    char *last_slash = NULL;
    char *file = NULL;
    last_slash = strrchr(path, '/');

    if(last_slash) {
        file = strndup(last_slash + 1, path - last_slash + 1);
        return file;
    } else {
        return strdup(path);
    }
}

char *get_filename_without_ext(const char *filename) {
    char *last_dot = NULL;
    char *file = NULL;
    last_dot = strrchr(filename, '.');

    if(!last_dot || last_dot == filename)
        return (char *)filename;

    file = strndup(filename, strlen(filename) - strlen(last_dot));
    return file;
}

int cp_file(const char *to, const char *from) {
    int fd_to, fd_from;
    char buf[4096];
    int nread;
    int saved_errno;

    fd_from = open(from, O_RDONLY);
    if(fd_from < 0)
        return -1;

    fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
    if(fd_to < 0)
        goto out_error;

    while(nread = read(fd_from, buf, sizeof buf), nread > 0) {
        char *out_ptr = buf;
        int nwritten;

        do {
            nwritten = write(fd_to, out_ptr, nread);

            if(nwritten >= 0) {
                nread -= nwritten;
                out_ptr += nwritten;
            } else if(errno != EINTR) {
                goto out_error;
            }
        } while(nread > 0);
    }

    if(nread == 0) {
        if(close(fd_to) < 0) {
            fd_to = -1;
            goto out_error;
        }
        close(fd_from);

        /* Success! */
        return 0;
    }

out_error:
    saved_errno = errno;

    close(fd_from);
    if(fd_to >= 0)
        close(fd_to);

    errno = saved_errno;
    return -1;
}

char *read_entire_file_with_mmap(const char *filename, size_t *size) {

    char *f;

    if(!filename)
        return NULL;

    struct stat s;
    int fd = open(filename, O_RDONLY);

    if(fd == -1) {
        return NULL;
    }

    fstat(fd, &s);
    if(!S_ISREG(s.st_mode)) {
        close(fd);
        return NULL;
    }

    *size = s.st_size;

    size_t to_page_size = *size;

    int pagesize = getpagesize();
    to_page_size += pagesize - (to_page_size % pagesize);

    f = (char *)mmap(0, to_page_size, PROT_READ, MAP_PRIVATE, fd, 0);

    if(f == NULL)
        return NULL;

    close(fd);

    return f;
}

char *read_entire_file(const char *filename, size_t *size) {

    FILE *infile;
    char *buffer;
    long numbytes;

    if(!filename)
        return NULL;

    infile = fopen(filename, "r");

    if(infile == NULL)
        return NULL;

    fseek(infile, 0L, SEEK_END);
    numbytes = ftell(infile);

    fseek(infile, 0L, SEEK_SET);

    buffer = (char *)malloc(numbytes * sizeof(char));

    if(buffer == NULL)
        return NULL;

    fread(buffer, sizeof(char), numbytes, infile);
    fclose(infile);

    *size = numbytes;

    return buffer;
}

string_array read_lines(const char *filename) {

    string_array lines = NULL;

    size_t len = 0;

    FILE *fp;

    fp = fopen(filename, "r");

    if(fp == NULL) {
        fprintf(stderr, "Error reading file %s\n", filename);
        return NULL;
    }

    char *line = NULL;
    while((getline(&line, &len, fp)) != -1) {
        line[strlen(line) - 1] = '\0';
        arrput(lines, strdup(line));
    }

    free(line);
    fclose(fp);

    return lines;
}

/* qsort C-sds comparison function */
static int cstring_cmp(const void *a, const void *b) {
    char *ia = *((char **)a);
    char *ib = *((char **)b);

    int int_a = 0;
    int int_b = 0;

    for(; *ia; ia++) {
        if(isdigit(*ia))
            int_a = int_a * 10 + *ia - '0';
    }

    for(; *ib; ib++) {
        if(isdigit(*ib))
            int_b = int_b * 10 + *ib - '0';
    }

    return int_a - int_b;
    /* strcmp functions works exactly as expected from
    comparison function */
}

// We only return the file names here, not the full path!!
string_array list_files_from_dir(const char *dir, const char *prefix, const char *extension, string_array ignore_extensions, bool sort) {

    DIR *dp;

    string_array files = NULL;

    struct dirent *dirp;

    if((dp = opendir(dir)) == NULL) {
        fprintf(stderr, "Error opening %s\n", dir);
        return NULL;
    }

    while((dirp = readdir(dp)) != NULL) {

        if(dirp->d_type != DT_REG) {
            continue;
		}

        char *file_name = strdup(dirp->d_name);

		if(ignore_extensions) {

			int n = arrlen(ignore_extensions);
			bool ignore = false;

			for(int i = 0; i < n; i++) {
				if(FILE_HAS_EXTENSION(get_filename_ext(file_name), ignore_extensions[i])) {
					ignore = true;
					break;
				}
			}

			if(ignore) continue;
		}

        if(extension) {
            const char *file_ext = get_filename_ext(file_name);

            if(!FILE_HAS_EXTENSION(file_ext, extension)) {
				continue;
            }
        }

        if(prefix) {
            if(strncmp(prefix, file_name, strlen(prefix)) != 0) {
				continue;
            }
        }

        arrput(files, file_name);
    }

	if(files) {
		if(sort) {
			qsort(files, arrlen(files), sizeof(char *), cstring_cmp);
		}
	}

    closedir(dp);
    return files;
}

bool file_exists(const char *path) {

    if(access(path, F_OK) != -1) {
        // file exists
        return true;
    } else {
        // file doesn't exist
        return false;
    }
}

bool dir_exists(const char *path) {
    struct stat info;

    if(stat(path, &info) != 0)
        return false;
    else if(info.st_mode & S_IFDIR)
        return true;
    else
        return false;
}

void free_path_information(struct path_information *input_info) {
    free(input_info->dir_name);
    free(input_info->filename_without_extension);
    free(input_info->file_extension);
}

void get_path_information(const char *path, struct path_information *input_info) {
    struct stat info;

    input_info->dir_name = NULL;
    input_info->filename_without_extension = NULL;
    input_info->file_extension = NULL;

    input_info->exists = false;

    if(stat(path, &info) != 0) {
        return;
    }

    input_info->exists = true;
    input_info->is_dir = info.st_mode & S_IFDIR;
    input_info->is_file = !(input_info->is_dir);

    if(input_info->is_file) {
        input_info->file_extension = strdup(get_filename_ext(path));
        char *file_name = get_file_from_path(path);
        input_info->filename_without_extension = get_filename_without_ext(file_name);
        input_info->dir_name = get_dir_from_path(path);
        free(file_name);
    } else {
        input_info->dir_name = strdup(path);
    }

    return;
}

int remove_directory(const char *path) {
    DIR *d = opendir(path);
    size_t path_len = strlen(path);
    int r = -1;

    if(d) {
        struct dirent *p;

        r = 0;

        while(!r && (p = readdir(d))) {
            int r2 = -1;
            char *buf;
            size_t len;

            /* Skip the names "." and ".." as we don't want to recurse on them. */
            if(!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
                continue;
            }

            len = path_len + strlen(p->d_name) + 2;
            buf = malloc(len);

            if(buf) {
                struct stat statbuf;

                snprintf(buf, len, "%s/%s", path, p->d_name);

                if(!stat(buf, &statbuf)) {
                    if(S_ISDIR(statbuf.st_mode)) {
                        r2 = remove_directory(buf);
                    } else {
                        r2 = unlink(buf);
                    }
                }

                free(buf);
            }

            r = r2;
        }

        closedir(d);
    }

    if(!r) {
        r = rmdir(path);
    }

    return r;
}

void create_dir(char *out_dir) {

    if(dir_exists(out_dir))
        return;

    size_t out_dir_len = strlen(out_dir);

    char *new_dir = (char *)malloc(out_dir_len + 2);

    memcpy(new_dir, out_dir, out_dir_len + 1);

    if(new_dir[out_dir_len] != '/') {
        new_dir[out_dir_len] = '/';
        new_dir[out_dir_len + 1] = '\0';
    }

    int start = 0;

    if(new_dir[0] == '/') {
        start++;
    }

    char *slash = strchr(new_dir + start, '/');

    while(slash) {
        size_t dirname_size = slash - new_dir;
        char *dir_to_create = malloc(dirname_size + 1);
        memcpy(dir_to_create, new_dir, dirname_size);
        dir_to_create[dirname_size] = '\0';

        if(!dir_exists(dir_to_create)) {

            printf("%s does not exist! Creating!\n", dir_to_create);

            if(mkdir(dir_to_create, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
                if(!dir_exists(dir_to_create)) { // HACK: this can avoid MPI or batch simulation problems...
                    fprintf(stderr, "Error creating directory %s Exiting!\n", dir_to_create);
                    exit(EXIT_FAILURE);
                }
            }
        }

        slash = strchr(slash + 1, '/');
        free(dir_to_create);
    }

    free(new_dir);
}

char *get_executable_dir() {
        char buf[PATH_MAX];
        size_t s = readlink("/proc/self/exe", buf, PATH_MAX);

        if(s == -1) {
            return NULL;
        }

        char *dir = get_dir_from_path(buf);
        return dir;
}

const char *get_home_dir() {
    const char *homedir;

    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }

    return homedir;
}

void print_current_dir() {
    char buf[PATH_MAX];
    getcwd(buf, PATH_MAX);
    printf("Current directory %s\n", buf);
}

void print_path_contents(const char *path) {
    DIR *mydir;
    struct dirent *myfile;
    struct stat mystat;

    char buf[PATH_MAX];
    mydir = opendir(path);
    while((myfile = readdir(mydir)) != NULL)
    {
        sprintf(buf, "%s/%s", path, myfile->d_name);
        stat(buf, &mystat);
        if(strcmp(myfile->d_name, ".") != 0 && strcmp(myfile->d_name, "..") != 0) {
            printf("%s\n", myfile->d_name);
        }
    }
    closedir(mydir);
}
