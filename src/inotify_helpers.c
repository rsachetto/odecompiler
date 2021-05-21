#include "stb/stb_ds.h"
#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "commands.h"
#include "inotify_helpers.h"

void add_file_watch(struct shell_variables *shell_state, char *path) {
    //TODO: skip paths that have a watch already
    int wd = inotify_add_watch(shell_state->fd_notify, path, IN_MODIFY);

	struct model_config **entries = hmget(shell_state->notify_entries, wd);
	arrput(entries, shell_state->current_model);

    hmput(shell_state->notify_entries, wd, entries);
}

void *check_for_model_file_changes(void *args) {

    pthread_detach(pthread_self());

    struct shell_variables *shell_state = (struct shell_variables *) (args);

    while (1) {

        int i = 0, length;
        char buffer[BUF_LEN];

        length = read(shell_state->fd_notify, buffer, BUF_LEN);

        while (i < length) {

            struct inotify_event *event = (struct inotify_event *) &buffer[i];

            if (event->len) {
                if (event->mask & IN_MODIFY) {

                    if (event->mask & IN_ISDIR) {
                    } else {
                        maybe_reload_from_file_change(shell_state, event);
						break;
                    }
                }
            }
            i += EVENT_SIZE + event->len;
        }
    }
}