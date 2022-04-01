#include "stb/stb_ds.h"
#include <pthread.h>
#include <unistd.h>

#include "commands.h"
#include "inotify_helpers.h"

void add_file_watch(struct shell_variables *shell_state, char *path) {
    //TODO: skip paths that have a watch already
    int wd = inotify_add_watch(shell_state->fd_notify, path, IN_MODIFY|IN_MOVED_TO);

	struct model_config **entries = hmget(shell_state->notify_entries, wd);
	arrput(entries, shell_state->current_model);

    hmput(shell_state->notify_entries, wd, entries);
}

_Noreturn void *check_for_model_file_changes(void *args) {

    pthread_detach(pthread_self());

    struct shell_variables *shell_state = (struct shell_variables *) (args);

    while (1) {

        size_t i = 0;
        char buffer[BUF_LEN];

        size_t length = read(shell_state->fd_notify, buffer, BUF_LEN);

        while (i < length) {

            struct inotify_event *event = (struct inotify_event *) &buffer[i];

            if (event->len) {
                if ((event->mask & IN_MODIFY) || (event->mask & IN_MOVED_TO)) {

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
