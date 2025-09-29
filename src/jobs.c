#define _GNU_SOURCE

#include <stdio.h>
#include <sys/wait.h>
#include "jobs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#define MAX_JOBS 64
static Job jobs[MAX_JOBS];
static int next_job_id = 1;

void init_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        jobs[i].active = 0;
    }
}

int add_job(pid_t pid, const char *cmd, JobState state) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (!jobs[i].active) {
            pid_t pgid = getpgid(pid);   // fetch the PGID
            if (pgid == -1) {
                pgid = pid;             // fallback
            }
            jobs[i].pgid = pgid;
            strncpy(jobs[i].cmd, cmd, sizeof(jobs[i].cmd) - 1);
            jobs[i].cmd[sizeof(jobs[i].cmd) - 1] = '\0';
            jobs[i].state = state;
            jobs[i].job_id = next_job_id++;
            jobs[i].active = 1;
            return jobs[i].job_id;
        }
    }
    fprintf(stderr, "Error: Too many jobs.\n");
    return -1;
}


void remove_job_by_pgid(pid_t pid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active && jobs[i].pgid == pid) {
            jobs[i].active = 0;
            return;
        }
    }
}

Job* get_job_by_id(int job_id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active && jobs[i].job_id == job_id) {
            return &jobs[i];
        }
    }
    return NULL;
}

void print_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active) {
            printf("[%d] %s %s\n",
                   jobs[i].job_id,
                   jobs[i].state == RUNNING ? "Running" : "Stopped",
                   jobs[i].cmd);
        }
    }
}

void check_background_jobs() {
    int status;
    pid_t result;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active) {
            // Check the status of the entire process group without blocking
            result = waitpid(-jobs[i].pgid, &status, WNOHANG | WUNTRACED);

            if (result > 0) {
                // A process in the group has changed state.
                if (WIFEXITED(status)) {
                    // The job exited normally.
                    printf("\n%s with pid %d exited normally\n", jobs[i].cmd, result);
                    fflush(stdout);
                    jobs[i].active = 0; // Remove the job from the list.
                } 
                else if (WIFSIGNALED(status)) {
                    // The job was terminated by a signal.
                    printf("\n%s with pid %d terminated by signal %d\n", jobs[i].cmd, result, WTERMSIG(status));
                    fflush(stdout);
                    jobs[i].active = 0; // Remove the job.
                }
                else if (WIFSTOPPED(status)) {
                    // The job was stopped (e.g., by Ctrl+Z).
                    jobs[i].state = STOPPED;
                    printf("\n[%d]+ Stopped\t\t%s\n", jobs[i].job_id, jobs[i].cmd);
                    fflush(stdout);
                }

            } else if (result == -1 && errno == ECHILD) {
                // No such process group exists anymore; it's safe to clean up.
                jobs[i].active = 0;
            }
        }
    }
}

// Helper for qsort to sort jobs alphabetically by command name
// int compare_jobs(const void* a, const void* b) {
//     Job* jobA = (Job*)a;
//     Job* jobB = (Job*)b;
//     return strcmp(jobA->cmd, jobB->cmd);
// }

// Fills a provided array with a copy of all active jobs and returns the count
int get_all_jobs(Job* job_array, int max_jobs) {
    int count = 0;
    for (int i = 0; i < MAX_JOBS && count < max_jobs; i++) {
        if (jobs[i].active) {
            job_array[count++] = jobs[i];
        }
    }
    return count;
}

void kill_all_jobs() {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].active) {
            kill(-jobs[i].pgid, SIGKILL);
        }
    }
}