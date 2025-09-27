// src/executor.c
#include <string.h>
#include "executor.h"
#include "parser.h"
#include "builtins.h"
#include "jobs.h"
#include <stdio.h>
#include "globals.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>

static int is_builtin_command(const char *cmd) {
    if (!cmd) return 0;
    return  strcmp(cmd,"hop")==0 ||
            strcmp(cmd,"reveal")==0 ||
            strcmp(cmd,"jobs")==0 ||
            strcmp(cmd,"activities")==0 ||
            strcmp(cmd,"fg")==0 ||
            strcmp(cmd,"bg")==0 ||
            strcmp(cmd,"exit")==0 ||
            strcmp(cmd,"log")==0 ||
            strcmp(cmd,"ping")==0;
}

void launch_process(char* command_str, int in_fd, int out_fd) {
    char *args[MAX_ARGS];
    char *input_file = NULL;
    char *output_file = NULL;
    int append_mode = 0;

    char* command_copy = strdup(command_str);
    if (!command_copy) _exit(1);

    // Multi-output redirection handling (existing code)
    typedef struct {
        char *fname;
        int append;
    } OutRedir;
    OutRedir outs[16];
    int out_count = 0;

    {
        char *p = command_copy;
        int in_quote = 0;
        char quote_ch = 0;
        while (*p) {
            if (in_quote) {
                if (*p == quote_ch) { in_quote = 0; }
                p++;
                continue;
            }
            if (*p == '\'' || *p == '"') {
                in_quote = 1;
                quote_ch = *p;
                p++;
                continue;
            }
            if (*p == '>') {
                int append = 0;
                char *q = p + 1;
                if (*q == '>') {
                    append = 1;
                    q++;
                }
                while (*q && isspace((unsigned char)*q)) q++;
                if (!*q) break;
                char *fname_start = q;
                while (*q && !isspace((unsigned char)*q) &&
                       *q!='>' && *q!='<' && *q!='&' && *q!='|' && *q!=';')
                    q++;
                char saved = *q;
                *q = '\0';

                if (out_count < (int)(sizeof(outs)/sizeof(outs[0]))) {
                    outs[out_count].fname = strdup(fname_start);
                    outs[out_count].append = append;
                    out_count++;
                }
                *q = saved;

                char *wipe = p;
                while (wipe < q) {
                    if (*wipe != '\0') *wipe = ' ';
                    wipe++;
                }
                p = q;
                continue;
            }
            p++;
        }
    }
    // Open output redirections left-to-right
    int final_out_fd = -1;
    if (out_count > 0) {
        for (int i = 0; i < out_count; i++) {
            int flags = O_WRONLY | O_CREAT;
            if (outs[i].append) {
                flags |= O_APPEND;
            } else {
                flags |= O_TRUNC;
            }
            int fd = open(outs[i].fname, flags, 0644);
            if (fd < 0) {
                fprintf(stderr, "Unable to create file for writing\n");
                for (int j = 0; j < out_count; j++) {
                    free(outs[j].fname);
                }
                free(command_copy);
                _exit(1);
            }
            if (i < out_count - 1) {
                close(fd);
            } else {
                final_out_fd = fd;
            }
        }
        out_fd = final_out_fd;
        for (int i = 0; i < out_count; i++) {
            free(outs[i].fname);
        }
    }

    parse_command(command_copy, args, &input_file, &output_file, &append_mode);
    if (out_count > 0) output_file = NULL; // ignore parser's single redir if multi handled

    if (args[0] == NULL) {
        if (final_out_fd != -1) close(final_out_fd);
        free(command_copy);
        _exit(0);
    }

    // NEW: Check if this is a builtin command in a pipeline
    // Set up file descriptors first, then handle builtin
    if (input_file) {
        int fd = open(input_file, O_RDONLY);
        if (fd < 0) {
            fprintf(stderr, "No such file or directory\n");
            free(command_copy);
            _exit(1);
        }
        if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2"); close(fd); free(command_copy); _exit(1); }
        close(fd);
    } else if (in_fd != STDIN_FILENO) {
        if (dup2(in_fd, STDIN_FILENO) < 0) { perror("dup2"); free(command_copy); _exit(1); }
        close(in_fd);
    }

    if (output_file) {
        int flags = O_WRONLY | O_CREAT | (append_mode ? O_APPEND : O_TRUNC);
        int fd = open(output_file, flags, 0644);
        if (fd < 0) { fprintf(stderr, "Unable to create file for writing\n"); free(command_copy); _exit(1); }
        if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2"); close(fd); free(command_copy); _exit(1); }
        close(fd);
    } else if (out_fd != STDOUT_FILENO) {
        if (dup2(out_fd, STDOUT_FILENO) < 0) { perror("dup2"); free(command_copy); _exit(1); }
        close(out_fd);
    }

    // Reset signals for child process
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);

    // Try builtin first, even in pipeline
    if (handle_builtin(args)) {
        free(command_copy);
        _exit(0);  // Builtin succeeded, exit child
    }

    // Not a builtin, try external command
    execvp(args[0], args);
    fprintf(stderr, "Command not found!!\n");
    free(command_copy);
    _exit(1);
}

void add_to_log(const char *command) {

    const char *log_file = ".shell_log";
    FILE *log_fp = fopen(log_file, "r+");
    if (!log_fp) {
        log_fp = fopen(log_file, "w+");
        if (!log_fp) {
            perror("log");
            return;
        }
    }
    char log[15][256];
    int log_count = 0;
    while (log_count < 15 && fgets(log[log_count], sizeof(log[log_count]), log_fp)) {
        log[log_count][strcspn(log[log_count], "\n")] = '\0';
        log_count++;
    }
    if (log_count > 0 && strcmp(log[log_count - 1], command) == 0) {
        fclose(log_fp);
        return;
    }
    if (log_count < 15) {
        strcpy(log[log_count++], command);
    } else {
        for (int i = 1; i < 15; i++) {
            strcpy(log[i - 1], log[i]);
        }
        strcpy(log[14], command);
    }
    freopen(log_file, "w", log_fp);
    for (int i = 0; i < log_count; i++) {
        fprintf(log_fp, "%s\n", log[i]);
    }
    fclose(log_fp);
}

void execute_pipeline(char* line, int background) {
    check_background_jobs();
    add_to_log(line);

    // Split by pipes
    char* commands[MAX_ARGS];
    int num_commands = 0;
    char *saveptr = NULL;
    char *line_copy = strdup(line);
    if (!line_copy) return;

    char *seg = strtok_r(line_copy, "|", &saveptr);
    while (seg && num_commands < MAX_ARGS) {
        while (*seg==' '||*seg=='\t') seg++;
        char *end = seg + strlen(seg) - 1;
        while (end > seg && (*end==' '||*end=='\t')) { *end = '\0'; end--; }
        commands[num_commands++] = seg;
        seg = strtok_r(NULL, "|", &saveptr);
    }
    if (num_commands == 0) { free(line_copy); return; }

    // Check if single command has redirection
    if (num_commands == 1) {
        char *args[MAX_ARGS];
        char *infile=NULL,*outfile=NULL;
        int append_mode=0;
        char *cc = strdup(commands[0]);
        if (!cc) { free(line_copy); return; }
        parse_command(cc, args, &infile, &outfile, &append_mode);
        
        // If it's a builtin AND has redirection, we need to handle redirection
        if (args[0] && is_builtin_command(args[0]) && (infile || outfile)) {
            // This builtin has redirection, so we need to fork and handle it properly
            pid_t pid = fork();
            if (pid == 0) {
                // Child: set up redirection and execute builtin
                if (infile) {
                    int fd = open(infile, O_RDONLY);
                    if (fd < 0) {
                        fprintf(stderr, "No such file or directory\n");
                        free(cc);
                        free(line_copy);
                        _exit(1);
                    }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
                
                if (outfile) {
                    int flags = O_WRONLY | O_CREAT | (append_mode ? O_APPEND : O_TRUNC);
                    int fd = open(outfile, flags, 0644);
                    if (fd < 0) {
                        fprintf(stderr, "Unable to create file for writing\n");
                        free(cc);
                        free(line_copy);
                        _exit(1);
                    }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                
                handle_builtin(args);
                free(cc);
                free(line_copy);
                _exit(0);
            } else if (pid > 0) {
                // Parent: wait for child
                int status;
                waitpid(pid, &status, 0);
                free(cc);
                free(line_copy);
                return;
            } else {
                perror("fork");
                free(cc);
                free(line_copy);
                return;
            }
        }
        
        // No redirection OR not a builtin: handle normally
        if (args[0] && handle_builtin(args)) {
            free(cc);
            free(line_copy);
            return;
        }
        free(cc);
    }

    // Continue with normal pipeline handling...
    pid_t pgid = 0;
    pid_t pids[num_commands];
    int in_fd = STDIN_FILENO;
    int pipe_fds[2];

    for (int i = 0; i < num_commands; i++) {
        int out_fd = STDOUT_FILENO;
        if (i < num_commands - 1) {
            if (pipe(pipe_fds) < 0) {
                perror("pipe");
                if (in_fd != STDIN_FILENO) close(in_fd);
                free(line_copy);
                return;
            }
            out_fd = pipe_fds[1];
        }

        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            if (in_fd != STDIN_FILENO) close(in_fd);
            if (i < num_commands - 1) { close(pipe_fds[0]); close(pipe_fds[1]); }
            free(line_copy);
            return;
        }

        if (pids[i] == 0) {
            if (pgid == 0) pgid = getpid();
            setpgid(0, pgid);
            launch_process(commands[i], in_fd, out_fd);
            _exit(1);
        }

        if (pgid == 0) pgid = pids[i];
        setpgid(pids[i], pgid);

        if (in_fd != STDIN_FILENO) close(in_fd);
        if (out_fd != STDOUT_FILENO) close(out_fd);
        if (i < num_commands - 1) in_fd = pipe_fds[0];
    }

    free(line_copy);

    if (!background) {
        fg_pid = pgid;
        tcsetpgrp(STDIN_FILENO, pgid);
        int status;
        waitpid(pids[num_commands - 1], &status, WUNTRACED);
        if (WIFSTOPPED(status)) {
            add_job(pgid, line, STOPPED);
            char tmp[256];
            strncpy(tmp, commands[num_commands - 1], sizeof(tmp)-1);
            tmp[sizeof(tmp)-1]=0;
            char *sp = strchr(tmp,' ');
            if (sp) *sp=0;
            printf("Stopped %s\n", tmp);
            fflush(stdout);
        }
        tcsetpgrp(STDIN_FILENO, shell_pgid);
        fg_pid = -1;
    } else {
        int jid = add_job(pgid, line, RUNNING);
        printf("[%d] %d\n", jid, pgid);
        fflush(stdout);
    }
}
