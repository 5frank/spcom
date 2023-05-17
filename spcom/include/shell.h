#ifndef SHELL_INCLUDE_H_
#define SHELL_INCLUDE_H_

#include <stdbool.h>

struct shell_opts_s {
    int cooked;
    int local_echo;
};

extern const struct shell_opts_s *shell_opts;

struct shell_mode_s {
    /// write to stdout or stderr
    void (*write)(int fd, const void *data, size_t size);
    /// init mode
    void (*init)(void);
    /// exit mode and cleanup
    void (*exit)(void);
    /// read a char from stdin
    int (*getchar)(void);
    /// process char from stdin
    void (*insert)(int c);
};

int shell_init(void);
void shell_cleanup(void);
#if 0
__attribute__((format(printf, 2, 3)))
void shell_printf(int fd, const char *fmt, ...);
#endif

void shell_write(int fd, const void *data, size_t size);

#endif
