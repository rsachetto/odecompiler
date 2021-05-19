#include<sys/inotify.h>
#include <dirent.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include "stb/stb_ds.h"

#include "inotify_helpers.h"

void add_dir_watch(struct shell_variables *shell_state, char *path) {
    //TODO: skip paths that have a watch already
    int wd = inotify_add_watch(shell_state->fd_notify, path, IN_MODIFY);
    hmput(shell_state->notify_entries, wd, shell_state->current_model);
}

void *check_for_model_file_changes(void *args) {

    pthread_detach(pthread_self());

    struct shell_variables *shell_state = (struct shell_variables*)(args);

    while(1) {

        int i=0, length;
        char buffer[BUF_LEN];

        length = read(shell_state->fd_notify, buffer, BUF_LEN);

        while(i < length) {

            struct inotify_event *event = (struct inotify_event *) &buffer[i];

            if(event->len) {

                if ( event->mask & IN_MODIFY ) {

                    if ( event->mask & IN_ISDIR ) {
                    }
                    else {
                        maybe_reload_from_file_change(shell_state, event->wd);
                    }

                }
            }
            i += EVENT_SIZE + event->len;
        }
    }
}

