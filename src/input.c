#include "parser.h"
#include "input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* read_input() {
    char *line = NULL;
    size_t bufsize = 0;
    if (getline(&line, &bufsize, stdin) == -1) {
        if (feof(stdin)) {
            printf("logout\n");
            exit(EXIT_SUCCESS);
        } else {
            perror("getline");
            exit(EXIT_FAILURE);
        }
        if(validate_syntax(line) == 0) {
            fprintf(stderr, "Command not found!\n");
            free(line);
            return NULL;
        }
    }
    return line;
}