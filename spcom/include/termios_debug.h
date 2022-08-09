
#if TERMIOS_DEBUG_ENABLE
#include <termios.h>

static struct {
    struct termios before;
    struct termios after;
} shell_rl_debug;

static inline void ___debug_print_termios_attr(const struct termios *a,
        const struct termios *b)
{

#define _PRINT(FMT, ...) fprintf(stderr, FMT,  ##__VA_ARGS__)
    // tcflag_t  c_iflag; input modes
    _PRINT("c_iflag: 0x%X -> 0x%X, ", a->c_iflag, b->c_iflag);
    // tcflag_t  c_oflag   output modes
    _PRINT("c_oflag: 0x%X -> 0x%X, ", a->c_oflag, b->c_oflag);
    // tcflag_t  c_cflag     control modes
    _PRINT("c_cflag: 0x%X -> 0x%X, ", a->c_cflag, b->c_cflag);

    // tcflag_t  c_lflag     local modes
    _PRINT("c_lflag: 0x%X -> 0x%X\n", a->c_lflag, b->c_lflag);

    // cc_t c_cc[NCCS]  control chars
    // LOG_DBG("c_lflag: 0x%X  0x%X", a->c_lflag, b->c_lflag);
#undef _PRINT
}

#define ___TERMIOS_DEBUG_BEFORE() tcgetattr(0, &shell_rl_debug.before)
#define ___TERMIOS_DEBUG_AFTER(WHAT) do { \
    fprintf(stderr, "%s: ", WHAT); \
    tcgetattr(0, &shell_rl_debug.after); \
    ___debug_print_termios_attr(&shell_rl_debug.before,  &shell_rl_debug.after); \
} while(0)
#else

#define ___TERMIOS_DEBUG_BEFORE() do { } while (0)
#define ___TERMIOS_DEBUG_AFTER(WHAT) do { } while (0)
#endif
