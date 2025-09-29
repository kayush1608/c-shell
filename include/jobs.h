#ifndef JOBS_H
#define JOBS_H

#define MAX_JOBS 64

#include <sys/types.h>

// States a job can be in
typedef enum {
    RUNNING,
    STOPPED
} JobState;

// Job struct to hold process group info
typedef struct Job {
    pid_t pgid; // Using Process Group ID for robust job control
    char cmd[256];
    JobState state;
    int job_id; // 1-based job number
    int active;
} Job;

void init_jobs();
int add_job(pid_t pgid, const char *cmd, JobState state);
void remove_job_by_pgid(pid_t pgid);
void print_jobs();
Job* get_job_by_id(int job_id);
void check_background_jobs();
int get_all_jobs(Job* job_array, int max_jobs);
void kill_all_jobs();

#endif // JOBS_H
