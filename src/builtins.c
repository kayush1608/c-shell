// src/builtins.c

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>
// ... other includes
#include "builtins.h"
#include "prompt.h"
#include "jobs.h"
#include "globals.h"
#include "executor.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h> // FIX: Added for PATH_MAX
#include <signal.h> // FIX: Added for SIGCONT
#define MAX_JOBS 64

// Track whether our shell has ever established an OLDPWD via hop
static int oldpwd_initialized = 0;

int compare_jobs(const void* a, const void* b) {
    Job* jobA = (Job*)a;
    Job* jobB = (Job*)b;
    return strcmp(jobA->cmd, jobB->cmd);
}


// Helper function for qsort to sort strings lexicographically
int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

void builtin_hop(char **args) {
    const char *initial_pwd = getenv("PWD");
    char *old_pwd = (char *)(initial_pwd ? initial_pwd : "");
    char *allocated_old_pwd = NULL; // track strdup'ed pointer

    for (int i = 1; args[i] != NULL; i++) {
        const char *new_dir = args[i];

        if (strcmp(new_dir, "~") == 0) {
            new_dir = get_home_dir();
        } else if (strcmp(new_dir, "-") == 0) {
            new_dir = getenv("OLDPWD");
            if (!new_dir || !oldpwd_initialized) {
                fprintf(stderr, "No such directory!\n");
                break;
            }
        }

        if (chdir(new_dir) != 0) {
            fprintf(stderr, "No such directory!\n");
            break;
        }

        // Update OLDPWD only after successful chdir
        if (old_pwd && *old_pwd) {
            setenv("OLDPWD", old_pwd, 1);
            oldpwd_initialized = 1; // Mark that OLDPWD is now valid for this shell session
        }

        char cwd[PATH_MAX];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            setenv("PWD", cwd, 1);
            if (allocated_old_pwd) {
                free(allocated_old_pwd);
            }
            allocated_old_pwd = strdup(cwd);
            old_pwd = allocated_old_pwd;
        } else {
            perror("hop:getcwd");
            break;
        }
    }

    if (allocated_old_pwd) {
        free(allocated_old_pwd);
    }
    // Do not print prompt here
}


void builtin_reveal(char **args) {
    int arg_count = 0;
    for (int i = 1; args[i] != NULL; ++i) {
        arg_count++;
    }

    if (arg_count > 2) {
        fprintf(stderr, "reveal: Invalid Syntax!\n");
        return;
    }

    int show_all = 0;
    int long_fmt = 0;
    const char *path = ".";

    for (int i = 1; args[i] != NULL; ++i) {
        if (args[i][0] == '-') {
            // Distinguish lone "-" (meaning OLDPWD) from flags
            if (strcmp(args[i], "-") == 0) {
                path = "-";
            } else {
                for (size_t j = 1; j < strlen(args[i]); ++j) {
                    if (args[i][j] == 'a') show_all = 1;
                    if (args[i][j] == 'l') long_fmt = 1;
                }
            }
        } else {
            path = args[i];
        }
    }

    if (strcmp(path, "-") == 0) {
        const char *old_pwd = getenv("OLDPWD");
        // Require that our shell actually set OLDPWD (ignore inherited one)
        if (!oldpwd_initialized || !old_pwd || !*old_pwd) {
            fprintf(stderr, "No such directory!\n");
            return;
        }
        path = old_pwd;
    }

    DIR *d = opendir(path);
    if (!d) {
        fprintf(stderr, "No such directory!\n");
        return;
    }

    struct dirent *dir;
    char *file_list[1024];
    int count = 0;

    // Read all directory entries into an array for sorting
    while ((dir = readdir(d)) != NULL && count < 1024) {
        if (!show_all && dir->d_name[0] == '.') continue;
        file_list[count++] = strdup(dir->d_name);
    }
    closedir(d);

    // Sort the list lexicographically
    qsort(file_list, count, sizeof(char *), compare_strings);

    for (int i = 0; i < count; ++i) {
        if (long_fmt) {
            printf("%s\n", file_list[i]);
        } else {
            printf("%s\t", file_list[i]);
        }
        free(file_list[i]);
    }
    if (!long_fmt) printf("\n");
}


void builtin_fg(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "fg: job ID required\n");
        return;
    }

    int job_id = atoi(args[1]);
    Job *job = get_job_by_id(job_id);

    if (!job || !job->active) {
        fprintf(stderr, "No such job\n");
        return;
    }

    pid_t pgid = job->pgid;
    fg_pid = pgid;

    /* Give terminal to this job */
    if (tcsetpgrp(STDIN_FILENO, pgid) < 0) {
        perror("fg: tcsetpgrp");
    }

    /* Resume the job */
    if (kill(-pgid, SIGCONT) < 0) {
        perror("fg: SIGCONT");
    }

    /* Wait for job */
    int status;
    while (1) {
        pid_t w = waitpid(-pgid, &status, WUNTRACED);
        if (w <= 0) break;

        if (WIFSTOPPED(status)) {
            job->state = STOPPED;
            printf("\n[%d] Stopped %s\n", job->job_id, job->cmd);
            break;
        }
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            remove_job_by_pgid(pgid);
            break;
        }
    }

    /* Shell takes terminal back */
    tcsetpgrp(STDIN_FILENO, shell_pgid);
    fg_pid = -1;
}



// ... (rest of file) ...

// src/builtins.c

void builtin_bg(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "bg: job ID required\n");
        return;
    }
    int job_id = atoi(args[1]);
    Job* job = get_job_by_id(job_id);

    if (job == NULL) {
        fprintf(stderr, "No such job\n");
        return;
    }

    if (job->state == RUNNING) {
        fprintf(stderr, "bg: job %d already running\n", job_id);
        return;
    }

    // FIX: Use the negative PGID to signal the entire process group
    if (kill(-job->pgid, SIGCONT) < 0) {
        perror("kill (SIGCONT)");
        return;
    }

    // Update the job's state in our records
    job->state = RUNNING;
}

// src/builtins.c

void builtin_activities(char **args) {
    (void)args; // Unused argument
    
    Job all_jobs[MAX_JOBS];
    int job_count = get_all_jobs(all_jobs, MAX_JOBS);

    if (job_count == 0) {
        return;
    }

    qsort(all_jobs, job_count, sizeof(Job), compare_jobs);

    for (int i = 0; i < job_count; i++) {
        char proc_path[256];
        // FIX: Use the '.pgid' field to build the path to /proc
        snprintf(proc_path, sizeof(proc_path), "/proc/%d/stat", all_jobs[i].pgid);
        
        FILE *f = fopen(proc_path, "r");
        if (f) {
            char state_char;
            fscanf(f, "%*d %*s %c", &state_char);
            fclose(f);
            
            const char* state_str = "Running";
            if (state_char == 'T') {
                state_str = "Stopped";
            }

            // FIX: Print the '.pgid' field as required
            printf("[%d]: %s - %s\n", all_jobs[i].pgid, all_jobs[i].cmd, state_str);
        }
    }
}


void builtin_log(char **args) {
    const char *log_file = ".shell_log";
    FILE *log_fp = fopen(log_file, "r");
    if (!log_fp) {
        return; // No log file, nothing to do.
    }

    char log[15][256];
    int log_count = 0;
    while (log_count < 15 && fgets(log[log_count], sizeof(log[log_count]), log_fp)) {
        log[log_count][strcspn(log[log_count], "\n")] = '\0'; // Remove newline
        log_count++;
    }
    fclose(log_fp);

    if (args[1] == NULL) {
        // Display log in the order they appear in the file (oldest to newest)
        for (int i = 0; i < log_count; i++) {
            printf("%s\n", log[i]);
        }
    } else if (strcmp(args[1], "execute") == 0 && args[2] != NULL) {
        int history_index = atoi(args[2]);
        
        // Convert the 1-based history number to the correct array index.
        // History item '1' should be the last item in the array (log_count - 1).
        // History item 'N' should be the Nth from the end (log_count - N).
        int array_index = log_count - history_index;

        if (history_index <= 0 || array_index < 0 || array_index >= log_count) {
            fprintf(stderr, "log: invalid index\n");
        } else {
            // Check if the command to execute contains "log execute" to prevent infinite loop
            if (strstr(log[array_index], "log execute") != NULL) {
                fprintf(stderr, "log: cannot execute log execute command to prevent infinite loop\n");
                return;
            }
            
            printf("Executing: %s\n", log[array_index]);
            char *command_copy = strdup(log[array_index]);
            if (command_copy) {
                // We pass 0 for the background flag since we want to run this in the foreground.
                execute_pipeline(command_copy, 0);
                free(command_copy);
            }
        }
    } else {
        fprintf(stderr, "log: invalid arguments\n");
    }
}

void builtin_ping(char **args) {
    if (args[1] == NULL || args[2] == NULL) {
        fprintf(stderr, "ping: usage: ping <pid> <signal_number>\n");
        return;
    }

    pid_t pid = atoi(args[1]);
    int signal_number = atoi(args[2]);
    int actual_signal = signal_number % 32;

    if (kill(pid, actual_signal) == 0) {
        printf("Sent signal %d to process with pid %d\n", actual_signal, pid);
    } else {
        perror("ping");
        fprintf(stderr, "No such process found\n");
    }
}

int handle_builtin(char **args) {
    if (strcmp(args[0], "hop") == 0) {
        builtin_hop(args);
        return 1;
    }
    if (strcmp(args[0], "reveal") == 0) {
        builtin_reveal(args);
        return 1;
    }
    if (strcmp(args[0], "jobs") == 0) {
        print_jobs();
        return 1;
    }
    if (strcmp(args[0], "activities") == 0) {
        builtin_activities(args);
        return 1;
    }
    if (strcmp(args[0], "fg") == 0) {
        builtin_fg(args);
        return 1;
    }
    if (strcmp(args[0], "bg") == 0) {
        builtin_bg(args);
        return 1;
    }
    if (strcmp(args[0], "exit") == 0) {
        exit(0);
    }
    if (strcmp(args[0], "log") == 0) {
        builtin_log(args);
        return 1;
    }
    if (strcmp(args[0], "ping") == 0) {
        builtin_ping(args);
        return 1;
    }
    return 0;
}