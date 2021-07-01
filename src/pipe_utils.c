#include "pipe_utils.h"
#include <stdbool.h>

#define	DUP2CLOSE(oldfd, newfd)	(dup2(oldfd, newfd) == 0  &&  close(oldfd) == 0)

static int remap_pipe_stdin_stdout(int rpipe, int wpipe, int out) {
	/*------------------------------------------------------------------
	 * CASE [A]
	 *
	 * This is the trivial case that probably never happens: the two FDs
	 * are already in the right place and we have nothing to do. Though
	 * this probably doesn't happen much, it's guaranteed that *doing*
	 * any shufflingn would close descriptors that shouldn't have been.
	 */
	if ( rpipe == 0  &&  wpipe == out )
		return true;

	/*----------------------------------------------------------------
	 * CASE [B] and [C]
	 *
	 * These two have the same handling but not the same rules. In case
	 * [C] where both FDs are "out of the way", it doesn't matter which
	 * of the FDs is closed first, but in case [B] it MUST be done in
	 * this order.
	 */
	if ( rpipe >= out  &&  wpipe > out )
	{
		return DUP2CLOSE(rpipe, 0)
		    && DUP2CLOSE(wpipe, out);
	}


	/*----------------------------------------------------------------
	 * CASE [D]
	 * CASE [E]
	 *
 	 * In these cases, *one* of the FDs is already correct and the other
	 * one can just be dup'd to the right place:
	 */
	if ( rpipe == 0  &&  wpipe >= out )
		return DUP2CLOSE(wpipe, out);

	if ( rpipe  >= out  &&  wpipe == out )
		return DUP2CLOSE(rpipe, 0);


	/*----------------------------------------------------------------
	 * CASE [F]
	 *
	 * Here we have the write pipe in the read slot, but the read FD
	 * is out of the way: this means we can do this in just two steps
	 * but they MUST be in this order.
	 */
	if ( rpipe >= out   &&  wpipe == 0 )
	{
		return DUP2CLOSE(wpipe, out)
		    && DUP2CLOSE(rpipe, 0);
	}

	/*----------------------------------------------------------------
	 * CASE [G]
	 *
	 * This is the trickiest case because the two file descriptors  are
	 * *backwards*, and the only way to make it right is to make a
	 * third temporary FD during the swap.
	 */
	if ( rpipe == out  &&  wpipe == 0 )
	{
	const int tmp = dup(wpipe);		/* NOTE! this is not dup2() ! */

		return	tmp > out
		    &&  close(wpipe)   == 0
		    &&  DUP2CLOSE(rpipe, 0)
		    &&  DUP2CLOSE(tmp,   out);
	}

	/* SHOULD NEVER GET HERE */

	return  false;
}

int popen2(const char *cmdline, struct popen2 *childinfo) {

    int pipe_stdin[2] = {-1,-1}, pipe_stdout[2];

#define	PARENT_READ	 pipe_stdout[0]
#define	CHILD_WRITE	 pipe_stdout[1]

#define CHILD_READ	 pipe_stdin[0]
#define PARENT_WRITE pipe_stdin[1]

    pid_t p;
    if(pipe(pipe_stdin)) return -1;
    if(pipe(pipe_stdout)) return -1;

    p = fork();
    if(p < 0) return p; /* Fork failed */
    if(p == 0) { /* child */
        close(PARENT_WRITE);
        close(PARENT_READ);
        remap_pipe_stdin_stdout(CHILD_READ, CHILD_WRITE, STDERR_FILENO);
        execl("/bin/sh", "sh", "-c", cmdline, 0);

    } else {
        close(CHILD_READ);
        close(CHILD_WRITE);
    }

    childinfo->child_pid = p;
    childinfo->to_child = PARENT_WRITE;
    childinfo->from_child = PARENT_READ;
    return 0;
}
