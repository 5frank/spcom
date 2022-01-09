#ifndef SHELL_INCLUDE_H__
#define SHELL_INCLUDE_H__

#include <stdbool.h>

struct shell_opts_s {
    int cooked;
    int sticky;
};

struct shell_mode_s {
    /// init mode
    int (*init)(const struct shell_opts_s *opts);
    /// technically not a lock. used to keep prompts in some states
    void (*lock)(void);
    /// technically not a lock. used to keep prompts in some states
    void (*unlock)(void);
    /// load (enter) mode
    void (*enter)(void);
    /// save mode state and leave it cleanly
    void (*leave)(void);
    /// process char from stdin
    int (*input_putc)(char c);
};

int shell_init(void);
void shell_cleanup(void);

void shell_printf(int fd, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/// technically not a lock. used to keep prompts in some states
const void *shell_output_lock(void);

/// technically not a lock. used to keep prompts in some states
void shell_output_unlock(const void *x);

#endif
