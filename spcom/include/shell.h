#ifndef SHELL_INCLUDE_H__
#define SHELL_INCLUDE_H__

#include <stdbool.h>


int shell_init(void);
void shell_cleanup(void);


int shell_update(const void *user_input, size_t size);

void shell_printf(int fd, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));


struct shell_rls_s;
struct shell_rls_s *shell_rls_save(void);
void shell_rls_restore(struct shell_rls_s *rls);

#endif
