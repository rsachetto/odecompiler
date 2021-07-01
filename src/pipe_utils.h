#ifndef __PIPE_UTILS_H
#define __PIPE_UTILS_H 

#include <unistd.h>

struct popen2 {
    pid_t child_pid;
    int from_child, to_child;
};

int popen2(const char *cmdline, struct popen2 *childinfo);

#endif /* __PIPE_UTILS_H */
