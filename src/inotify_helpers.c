#ifdef __linux__

#include "stb/stb_ds.h"
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include "commands.h"
#include "inotify_helpers.h"
#include "file_utils/file_utils.h"

void add_file_watch(struct shell_variables *shell_state, char *path) {
    //TODO: skip paths that have a watch already
    int wd = inotify_add_watch(shell_state->fd_notify, path, IN_MODIFY|IN_MOVED_TO);

	struct model_config **entries = hmget(shell_state->notify_entries, wd);
    shell_state->current_model->notify_code = wd;
	arrput(entries, shell_state->current_model);

    hmput(shell_state->notify_entries, wd, entries);
}

_Noreturn void *check_for_model_file_changes(void *args) {

    pthread_detach(pthread_self());

    struct shell_variables *shell_state = (struct shell_variables *) (args);

    while (1) {

        size_t i = 0;
        char ibuffer[BUF_LEN];

        ssize_t length = read(shell_state->fd_notify, ibuffer, BUF_LEN);

        if(length < 0) {
            if(errno == EINTR) {
                continue;
            }

            // Unrecoverable error: attempt to recreate the inotify descriptor and watches
            if(shell_state->fd_notify >= 0) {
                close(shell_state->fd_notify);
                shell_state->fd_notify = -1;
            }

            // Clear old notify entries
            int n = hmlen(shell_state->notify_entries);
            for(int j = 0; j < n; j++) {
                arrfree(shell_state->notify_entries[j].value);
            }
            hmfree(shell_state->notify_entries);
            shell_state->notify_entries = NULL;

            // Re-initialize inotify
            int new_fd = inotify_init();
            if(new_fd < 0) {
                sleep(1);
                continue;
            }
            shell_state->fd_notify = new_fd;

            // Re-add watches for all currently loaded models
            int n_models = shlen(shell_state->loaded_models);
            for(int j = 0; j < n_models; j++) {
                struct model_config *config = shell_state->loaded_models[j].value;
                char *dir = get_dir_from_path(config->model_file);
                if(dir) {
                    int wd = inotify_add_watch(shell_state->fd_notify, dir, IN_MODIFY|IN_MOVED_TO);
                    if(wd >= 0) {
                        struct model_config **entries = hmget(shell_state->notify_entries, wd);
                        config->notify_code = wd;
                        arrput(entries, config);
                        hmput(shell_state->notify_entries, wd, entries);
                    }
                    free(dir);
                }
            }
            continue;
        }

        while (i < length) {

            struct inotify_event *event = (struct inotify_event *) &ibuffer[i];
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
#endif
