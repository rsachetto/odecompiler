#include "model_config.h"

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

#elif defined(__APPLE__)

#include "stb/stb_ds.h"
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/event.h>
#include <fcntl.h>

#include "commands.h"
#include "inotify_helpers.h"
#include "file_utils/file_utils.h"
#include "md5/md5.h"
#include "compiler/program.h"

void add_file_watch(struct shell_variables *shell_state, char *file_path) {
    int fd = open(file_path, O_EVTONLY);
    if(fd < 0) return;

    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_VNODE, EV_ADD | EV_CLEAR | EV_ENABLE,
           NOTE_WRITE | NOTE_EXTEND | NOTE_RENAME | NOTE_DELETE, 0, NULL);

    if(kevent(shell_state->fd_notify, &ev, 1, NULL, 0, NULL) < 0) {
        close(fd);
        return;
    }

    shell_state->current_model->notify_code = fd;

    struct model_config **entries = hmget(shell_state->notify_entries, fd);
    arrput(entries, shell_state->current_model);
    hmput(shell_state->notify_entries, fd, entries);
}

static void sync_reload_model(struct shell_variables *shell_state, struct model_config *model_config) {
    if(!model_config || !model_config->should_reload) return;

    size_t file_size;
    char *source = read_entire_file_with_mmap(model_config->model_file, &file_size);
    if(!source) return;

    uint8_t hash[16];
    md5Stringn(source, (uint8_t *)&hash, file_size);
    munmap(source, file_size);

    if(memcmp(hash, model_config->hash, sizeof(model_config->hash)) == 0) return;

    if(!model_config->is_derived) {
        char answer = 0;
        if(!model_config->auto_reload) {
            printf("\nModel %s was modified externally. Would you like to reload it? [y]: ", model_config->model_name);
            answer = (char) getchar();
        }
        if(model_config->auto_reload || answer == 'Y' || answer == 'y' || answer == '\r') {
            printf("\nReloading model %s", model_config->model_name);
            fflush(stdout);
            program tmp = model_config->program;
            bool error = generate_model_program(model_config);
            if(!error) {
                error = compile_model(model_config);
                free_program(tmp);
                if(error) {
                    printf("Error compiling model %s", model_config->model_name);
                }
            } else {
                printf("Error compiling model %s", model_config->model_name);
            }
        }
    }

    printf("\n%s", PROMPT);
    fflush(stdout);
}

_Noreturn void *check_for_model_file_changes(void *args) {
    pthread_detach(pthread_self());
    struct shell_variables *shell_state = (struct shell_variables *)args;

    while(1) {
        struct kevent ev;
        struct timespec timeout = {1, 0};

        int nevents = kevent(shell_state->fd_notify, NULL, 0, &ev, 1, &timeout);

        if(nevents < 0) {
            if(errno == EINTR) continue;

            if(shell_state->fd_notify >= 0) {
                close(shell_state->fd_notify);
                shell_state->fd_notify = -1;
            }

            int n = hmlen(shell_state->notify_entries);
            for(int j = 0; j < n; j++) {
                close(shell_state->notify_entries[j].key);
                arrfree(shell_state->notify_entries[j].value);
            }
            hmfree(shell_state->notify_entries);
            shell_state->notify_entries = NULL;

            int new_fd = kqueue();
            if(new_fd < 0) {
                sleep(1);
                continue;
            }
            shell_state->fd_notify = new_fd;

            int n_models = shlen(shell_state->loaded_models);
            for(int j = 0; j < n_models; j++) {
                struct model_config *config = shell_state->loaded_models[j].value;
                int fd = open(config->model_file, O_EVTONLY);
                if(fd >= 0) {
                    struct kevent new_ev;
                    EV_SET(&new_ev, fd, EVFILT_VNODE, EV_ADD | EV_CLEAR | EV_ENABLE,
                           NOTE_WRITE | NOTE_EXTEND | NOTE_RENAME | NOTE_DELETE, 0, NULL);
                    kevent(shell_state->fd_notify, &new_ev, 1, NULL, 0, NULL);
                    config->notify_code = fd;
                    struct model_config **entries = hmget(shell_state->notify_entries, fd);
                    arrput(entries, config);
                    hmput(shell_state->notify_entries, fd, entries);
                }
            }
            continue;
        }

        if(nevents > 0) {
            int fd = (int)ev.ident;

            if(ev.fflags & (NOTE_RENAME | NOTE_DELETE)) {
                struct model_config **model_configs = hmget(shell_state->notify_entries, fd);
                struct model_config *model_config = arrlen(model_configs) > 0 ? model_configs[0] : NULL;

                arrfree(model_configs);
                (void)hmdel(shell_state->notify_entries, fd);
                close(fd);

                if(model_config) {
                    int new_fd = open(model_config->model_file, O_EVTONLY);
                    if(new_fd >= 0) {
                        struct kevent new_ev;
                        EV_SET(&new_ev, new_fd, EVFILT_VNODE, EV_ADD | EV_CLEAR | EV_ENABLE,
                               NOTE_WRITE | NOTE_EXTEND | NOTE_RENAME | NOTE_DELETE, 0, NULL);
                        kevent(shell_state->fd_notify, &new_ev, 1, NULL, 0, NULL);
                        model_config->notify_code = new_fd;
                        struct model_config **new_entries = NULL;
                        arrput(new_entries, model_config);
                        hmput(shell_state->notify_entries, new_fd, new_entries);
                    }
                }
                continue;
            }

            if(ev.fflags & (NOTE_WRITE | NOTE_EXTEND)) {
                struct model_config **model_configs = hmget(shell_state->notify_entries, fd);
                struct model_config *model_config = arrlen(model_configs) > 0 ? model_configs[0] : NULL;

                if(shell_state->never_reload || !model_config) continue;

                pthread_mutex_lock(&shell_state->lock);
                sync_reload_model(shell_state, model_config);
                pthread_mutex_unlock(&shell_state->lock);
            }
        }
    }
}

#endif
