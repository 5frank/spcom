#ifndef SHELL_INCLUDE_H__
#define SHELL_INCLUDE_H__

#include <stdbool.h>

#include <readline/readline.h>
#include <readline/history.h>

struct shell_mode_s {
    const char *prompt;
    // struct keybind_s *keybind; TODO
    struct readline_state *rlstate;

    int (*m_init)(struct shell_mode_s *m);
    /// enter mode handler
    int (*m_enter)(struct shell_mode_s *m);
    /// leave mode handler
    int (*m_leave)(struct shell_mode_s *m);
    int (*m_output_write)(struct shell_mode_s *m, const void *data, size_t size);
    int (*m_input_write)(struct shell_mode_s *m, const void *data, size_t size);
};

int shell_init(void);
void shell_cleanup(void);


int shell_update(const void *user_input, size_t size);

void shell_printf(int fd, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));


struct shell_rls_s;
struct shell_rls_s *shell_rls_save(void);
void shell_rls_restore(struct shell_rls_s *rls);

/**
 * @defgroup common shell utils
 * convert to/from reg val
 * @{
 */
/** 
 * TODO would one of these work?:
 *    extern int rl_read_key (void);
 *    extern int rl_getc (FILE *);
 *    extern int rl_stuff_char (int);
 *    extern int rl_execute_next (int
 *
 * implementing a "rl_putc()" - alternatives:
 * 
 * 1. Setting `rl_pending_input` to a value makes it the next keystroke read.
 * sort of a "rl_putc()" also need to update reader with
 * rl_callback_read_char();!?
 *
 * 2.Set `FILE * rl_instream` to `fmemopen` or a pipe, then write to it.
 *
 * 3. modify `rl_line_buffer` - not recomended as it is also modifed by input  
 */ 
static inline void shell_rl_putc(int c)
{
    rl_pending_input = c;
    rl_callback_read_char();
    rl_redisplay(); //needed?
}

/** @} */
#endif
