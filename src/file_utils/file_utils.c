//
// Created by sachetto on 18/10/17.
//

#include "file_utils.h"
#include <stddef.h>

#define STB_DS_IMPLEMENTATION
#include "../stb/stb_ds.h"

#include "../string/sds.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

bool can_run_command(const char *cmd) {

    if(strchr(cmd, '/')) {
        // if cmd includes a slash, no path search must be performed,
        // go straight to checking if it's executable
        return access(cmd, X_OK)==0;
    }

    const char *path = getenv("PATH");
    if(!path) return false;

    int num_entries = 0;
    sds *path_entries = sdssplit(path, ":", &num_entries);

    sds path_and_command = NULL;

    for(int i = 0; i < num_entries; i++) {
        if(!*path_entries[i]) {
            path_and_command = sdscat(sdsnew("./"), cmd);
        } else {
            path_and_command = sdscatfmt(sdsempty(),"%s/%s", path_entries[i], cmd);
        }

        // check if we can execute it
        if(access(path_and_command, X_OK)==0) {
            sdsfreesplitres(path_entries, num_entries);
            sdsfree(path_and_command);
            return true;
        }

        sdsfree(path_and_command);
    }

    // not found
    sdsfreesplitres(path_entries, num_entries);
    return false;
}

char *get_current_directory() {
    size_t size;
    char *buf = NULL;
    size = (size_t) pathconf(".", _PC_PATH_MAX);

    if((buf = (char *)malloc(size)) != NULL) {
        buf = getcwd(buf, size);
        buf = strcat(buf, "/");
    }

    return buf;
}

const char *get_filename_ext(const char *filename) {
    if(filename == NULL)
        return NULL;

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

    if(filename == NULL) {
        return NULL;
    }

    char *last_dot = NULL;
    char *file = NULL;
    last_dot = strrchr(filename, '.');

    if(!last_dot || last_dot == filename)
        return strdup(filename);

    file = strndup(filename, strlen(filename) - strlen(last_dot));
    return file;
}

int cp_file(const char *to, const char *from, bool overwrite) {
    int fd_to, fd_from;
    char buf[4096];
    long nread;
    int saved_errno;

    fd_from = open(from, O_RDONLY);

	if(fd_from < 0) {
        return -1;
	}

	if(!overwrite) {
    	fd_to = open(to, O_WRONLY | O_CREAT | O_EXCL, 0666);
	}
	else {
    	fd_to = open(to, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	}

    if(fd_to < 0)
        goto out_error;

    while(nread = read(fd_from, buf, sizeof buf), nread > 0) {
        char *out_ptr = buf;
        long nwritten;

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

    size_t pagesize = getpagesize();
    to_page_size += pagesize - (to_page_size % pagesize);

    f = (char *)mmap(0, to_page_size, PROT_READ, MAP_PRIVATE, fd, 0);

    if(f == NULL)
        return NULL;

    close(fd);

    return f;
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

const char *get_home_dir() {
    const char *homedir;

    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }

    return homedir;
}

void print_current_dir() {
    char buf[PATH_MAX];
    if(!getcwd(buf, PATH_MAX)) {
        fprintf(stderr, "Error calling getcwd\n");
    }
    else {
        printf("Current directory %s\n", buf);
    }
}

void print_path_contents(const char *path) {
    DIR *mydir;
    struct dirent *myfile;
    struct stat mystat;

    char buf[PATH_MAX];
    mydir = opendir(path);

    if(!mydir) {
        fprintf(stderr, "Error - %s does not exist or is not a directory!\n", path);
        return;
    }

    while((myfile = readdir(mydir)) != NULL) {
        sprintf(buf, "%s/%s", path, myfile->d_name);
        stat(buf, &mystat);
        if(myfile->d_name[0] != '.' && strcmp(myfile->d_name, ".") != 0 && strcmp(myfile->d_name, "..") != 0) {
            printf("%s\n", myfile->d_name);
        }
    }
    closedir(mydir);
}
