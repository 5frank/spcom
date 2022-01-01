#ifndef SHELL_INCLUDE_H__
#define SHELL_INCLUDE_H__

#include <stdbool.h>


//enum shell_promt_e { SHELL_PROMT_DEFAULT = 0, SHELL_PROMT_CMD_MODE = 1 };
/** note that stdin is always in "raw" mode (when isatty(stdin)), 
 * these modes describes the behavior of user input to _this_ shell.
 */
enum shell_mode_e {
    /** not initalized */
    SHELL_M_NONE = 0,
    /** raw, or transparent. line buffer at remote (if any).
     * could alternative be called "half cooked" as stdin and stdout still processed.
     * example CTRL-D might be "consumed" by the application and not forwarded to serial port.
     */ 
    SHELL_M_RAW,
    /** Canonical. local line buffer */
    SHELL_M_CANON,
    /** command mode.  */
    SHELL_M_CMD,
    __NUM_MODES
};


int shell_init(void);
void shell_cleanup(void);


int shell_update(const void *user_input, size_t size);

void shell_printf(int fd, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));


struct shell_rls_s;
struct shell_rls_s *shell_rls_save(void);
void shell_rls_restore(struct shell_rls_s *rls);

#endif
