#ifndef SHELL_INCLUDE_H__
#define SHELL_INCLUDE_H__

#include <stdbool.h>

struct shell_opts_s {
    int cooked;
    int sticky;
    int local_echo;
};
extern struct shell_opts_s shell_opts;

struct shell_mode_s {
    //struct readline_state *rlstate;
    /// init mode
    //int (*init)(void);
    /// technically not a lock. used to keep prompts in some states
    const void *(*lock)(void);
    /// technically not a lock. used to keep prompts in some states
    void (*unlock)(const void *);
    /// load (enter) mode
    void (*enter)(void);
    /// save mode state and leave it cleanly
    void (*leave)(void);
    /// read a char from stdin
    int (*getchar)(void);
    /// process char from stdin
    void (*insert)(int c);
};

int shell_init(void);
void shell_cleanup(void);

__attribute__((format(printf, 2, 3)))
void shell_printf(int fd, const char *fmt, ...);

void shell_write(int fd, const void *data, size_t size);

#endif
