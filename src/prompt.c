#include "prompt.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <limits.h>
#include <sys/utsname.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static char home_dir[PATH_MAX];

void init_prompt() {
    if (getcwd(home_dir, sizeof(home_dir)) == NULL) {
        perror("getcwd");
        strcpy(home_dir, "/");
    }
}

const char* get_home_dir() {
    return home_dir;
}

void print_prompt() {
    char cwd[PATH_MAX];
    struct utsname uname_data;
    struct passwd *pw = getpwuid(getuid());

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd");
        return;
    }
    if (uname(&uname_data) != 0) {
        perror("uname");
        return;
    }

    char display_path[PATH_MAX];
    const char *home_dir = get_home_dir();
    if (strncmp(cwd, home_dir, strlen(home_dir)) == 0) {
        snprintf(display_path, sizeof(display_path), "~%s", cwd + strlen(home_dir));
    } else {
        strncpy(display_path, cwd, sizeof(display_path));
        display_path[sizeof(display_path) - 1] = '\0';
    }

    printf("<%s@%s:%s> ", pw->pw_name, uname_data.nodename, display_path);
    fflush(stdout);
}