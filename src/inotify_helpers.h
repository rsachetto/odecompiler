#ifndef __INOTIFY_HELPERS_H
#define __INOTIFY_HELPERS_H 

#include "ode_shell.h"
#include <sys/inotify.h>

#define MAX_EVENTS 1024  /* Maximum number of events to process*/
#define LEN_NAME 1024  /* Assuming that the length of the filename won't exceed 1024 bytes*/
#define EVENT_SIZE  ( sizeof (struct inotify_event) ) /*size of one event*/
#define BUF_LEN     ( MAX_EVENTS * ( EVENT_SIZE + LEN_NAME )) /*buffer to store the data of events*/

void add_file_watch(struct shell_variables *shell_state, char *path);
_Noreturn void *check_for_model_file_changes(void *args);

#endif /* __INOTIFY_HELPERS_H */
