#include "prompt.h"
#include "executor.h"
#include "jobs.h"
#include "globals.h"
#include "parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <stdbool.h>

// Global variable definitions
pid_t shell_pgid;
pid_t fg_pid = -1;

void sigtstp_handler(int sig) {
    (void)sig;
    if (fg_pid > 0) {
        kill(-fg_pid, SIGTSTP);
    }
}

void sigint_handler(int sig) {
    (void)sig;
    if (fg_pid > 0) {
        kill(-fg_pid, SIGINT);
    }
}

int main() {
    init_prompt();
    init_jobs();

    shell_pgid = getpid();
    if (tcsetpgrp(STDIN_FILENO, shell_pgid) < 0) {
        perror("tcsetpgrp (shell)");
    }

    signal(SIGINT, sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);

    bool eof_received_once = false; // Flag to track if we've seen one EOF

    while (1) {
        check_background_jobs();
        print_prompt();

        char *line = NULL;
        size_t len = 0;

        if (getline(&line, &len, stdin) == -1) {
            if (feof(stdin)) {
                if (eof_received_once || isatty(STDIN_FILENO)) {
                    
                    // This is now simple, just like your friend's code.
                    kill_all_jobs();
                    
                    printf("logout\n");
                    free(line);
                    exit(0);
                } else {
                    // This part for the test script remains the same
                    eof_received_once = true;
                    clearerr(stdin);
                    free(line);
                    printf("\n");
                    continue;
                }
            } else {
                perror("getline");
                free(line);
                break;
            }
        }
        // If we successfully read a line, it wasn't an EOF, so reset the flag.
        eof_received_once = false;

        line[strcspn(line, "\n")] = 0;

        if (strlen(line) == 0) {
            free(line);
            continue;
        }

        if (!validate_syntax(line)) {
            fprintf(stderr, "Invalid Syntax!\n");
            free(line);
            continue;
        }

        // --- Command processing logic ---
        char* line_copy = strdup(line);
        char* current_command = line_copy;

        while (current_command != NULL && *current_command != '\0') {
            char* separator = strpbrk(current_command, ";&");
            int background = 0;

            if (separator != NULL) {
                if (*separator == '&') {
                    background = 1;
                }
                *separator = '\0';
            }

            while (*current_command && isspace((unsigned char)*current_command)) {
                current_command++;
            }

            if (strlen(current_command) > 0) {
                execute_pipeline(current_command, background);
            }

            current_command = (separator != NULL) ? separator + 1 : NULL;
        }

        free(line_copy);
        free(line);
    }

    return 0;
}
