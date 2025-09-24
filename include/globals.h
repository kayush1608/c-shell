// include/globals.h
#ifndef GLOBALS_H
#define GLOBALS_H

#include <sys/types.h>

// DECLARES the variables as existing somewhere
extern pid_t shell_pgid;
extern pid_t fg_pid;
extern int next_job_id; // Added this for consistency

#endif // GLOBALS_H