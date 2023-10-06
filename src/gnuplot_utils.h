#include <stdbool.h>
#include "ode_shell.h"

void gnuplot_cmd(struct popen2 *handle, char const *cmd, ...);
void reset_terminal(struct popen2 *handle);
bool check_gnuplot_and_set_default_terminal(struct shell_variables *shell_state);
