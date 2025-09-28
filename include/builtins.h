// include/builtins.h
#ifndef BUILTINS_H
#define BUILTINS_H

// Takes parsed arguments, not the raw command string
int handle_builtin(char **args);
int handle_builtin_check(char *args); // returns 1 if builtin, 0 otherwise

#endif