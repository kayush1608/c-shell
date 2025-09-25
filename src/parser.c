#include "parser.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>

// A more robust command parser that correctly handles quoted arguments.
int parse_command(char* input, char** args, char** input_file, char** output_file, int* append_mode) {
    int argc = 0;
    int background = 0;
    *input_file = NULL;
    *output_file = NULL;
    *append_mode = 0;

    char* p = input;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (!*p) break;

        if (*p == '&') {
            background = 1;
            *p = '\0';
            break;
        }

        if (*p == '<') {
            // For multiple input redirections, we need to check each one left-to-right
            // and fail on the first one that doesn't exist
            p++;
            while (*p && isspace((unsigned char)*p)) p++;
            char *fname_start = p;
            while (*p && !isspace((unsigned char)*p) && *p!='<' && *p!='>' && *p!='&') p++;
            char saved = *p;
            if (saved) *p = '\0';
            
            // Check if this input file exists before proceeding
            FILE *test_file = fopen(fname_start, "r");
            if (!test_file) {
                // File doesn't exist - this should cause the command to fail
                // Set input_file to the non-existent file so executor can handle the error
                *input_file = fname_start;
                return background;
            }
            fclose(test_file);
            
            // File exists, so this becomes our input file (later ones will override)
            *input_file = fname_start;
            
            if (saved) *p = saved;
            continue;
        }
        
        if (*p == '>') {
            int is_append = 0;
            if (*(p + 1) == '>') {
                is_append = 1;
            }
            // If an output redirection is already specified, ignore any further ones.
            // We still need to consume the operator and the following filename so
            // that it does not become a normal argument.
            if (*output_file != NULL) {
                p += (is_append ? 2 : 1);              // skip '>' or '>>'
                while (*p && isspace((unsigned char)*p)) p++; // skip spaces
                while (*p && !isspace((unsigned char)*p) && *p!='<' && *p!='>' && *p!='&')
                    p++;                                // skip filename
                continue;
            }

            // First (and only) output redirection: record it
            if (is_append) {
                *append_mode = 1;
                p += 2;
            } else {
                *append_mode = 0;
                p++;
            }
            while (*p && isspace((unsigned char)*p)) p++;
            *output_file = p;
            while (*p && !isspace((unsigned char)*p) && *p!='<' && *p!='>' && *p!='&') p++;
            if (*p) *p++ = '\0';
            continue;
        }

        if (argc < MAX_ARGS - 1) {
            args[argc++] = p;
        }

        char quote = 0;
        while (*p) {
            if (quote) {
                if (*p == quote) {
                    quote = 0;
                }
            } else {
                if (*p == '\'' || *p == '"') {
                    quote = *p;
                } else if (isspace((unsigned char)*p) || *p == '<' || *p == '>' || *p == '&') {
                    break;
                }
            }
            p++;
        }
        
        if (*p) {
            *p++ = '\0';
        }
    }
    args[argc] = NULL;

    for (int i = 0; i < argc; i++) {
        char* arg = args[i];
        size_t len = strlen(arg);
        if (len >= 2) {
            if ((arg[0] == '"' && arg[len - 1] == '"') || (arg[0] == '\'' && arg[len - 1] == '\'')) {
                arg[len - 1] = '\0';
                args[i] = arg + 1;
            }
        }
    }
    
    return background;
}

/**
 * CORRECTED FUNCTION
 * Validates syntax, allowing '&' as a valid command terminator.
 */
int validate_syntax(const char* line) {
    bool expect_command = true;
    const char* p = line;

    while (*p) {
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }

        if (*p == '\0') {
            break;
        }

        if (*p == '|' || *p == ';' || *p == '&') {
            if (expect_command) {
                return 0; // Error: Operator where command should be (e.g., "||")
            }
            expect_command = true;
            p++;
        } else {
            expect_command = false;
            while (*p && *p != '|' && *p != ';' && *p != '&') {
                if (*p == '\'' || *p == '"') {
                    char quote = *p++;
                    while (*p && *p != quote) {
                        p++;
                    }
                    if (*p) p++;
                } else {
                    p++;
                }
            }
        }
    }

    // If the line ends with an operator, we'd be expecting a command.
    // This is an error for '|' and ';', but not for '&'.
    if (expect_command) {
        const char *end = line + strlen(line) - 1;
        while(end >= line && isspace((unsigned char)*end)) {
            end--;
        }

        if (end < line) {
            return 1; // Empty or whitespace-only line is valid.
        }

        if (*end == '|' || *end == ';') {
            return 0; // Invalid if it ends with a pipe or semicolon.
        }
    }

    return 1; // Syntax is valid.
}
