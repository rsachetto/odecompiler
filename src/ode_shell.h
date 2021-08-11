//
// Created by sachetto on 12/05/2021.
//

#ifndef ODECOMPILER_ODE_SHELL_H
#define ODECOMPILER_ODE_SHELL_H

#include "model_config.h"
#include "pipe_utils.h"

#include <stdio.h>
#include <pthread.h>

struct watch_entry {
    int key;
    struct model_config **value;
};

struct shell_variables {
    struct model_hash_entry *loaded_models;
    struct model_config *current_model;

    struct popen2 *gnuplot_handle;
    char *default_gnuplot_term;
    int fd_notify;
    pthread_mutex_t lock;
    struct watch_entry *notify_entries;
    char *current_dir;
    char *last_dir;
    bool never_reload;
};

struct model_hash_entry {
    char *key;
    struct model_config *value;
};

#define PROMPT "ode_shell> "
#define HISTORY_FILE ".ode_history"

#endif//ODECOMPILER_ODE_SHELL_H
