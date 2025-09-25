// include/parser.h
#ifndef PARSER_H
#define PARSER_H

#define MAX_ARGS 64

// Parses a command line string into arguments
int parse_command(char* input, char** args, char** input_file, char** output_file, int* append_mode);
int validate_syntax(const char* line);

#endif