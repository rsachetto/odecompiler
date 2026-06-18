#ifndef __INOTIFY_HELPERS_H
#define __INOTIFY_HELPERS_H

#include "ode_shell.h"

#ifdef __linux__

#include <sys/inotify.h>

#define MAX_EVENTS 1024
#define LEN_NAME 1024
#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( MAX_EVENTS * ( EVENT_SIZE + LEN_NAME ))

#endif

void add_file_watch(struct shell_variables *shell_state, char *path);
_Noreturn void *check_for_model_file_changes(void *args);

#endif
