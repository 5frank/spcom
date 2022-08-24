#if CONFIG_TERMIOS_DEBUG

#include <errno.h>
#include <termios.h>
#include "log.h"

struct ___termios_debug_data {
    struct termios before;
    int errnum;
};

static struct ___termios_debug_data ___termios_debug_data = {
    .errnum = -1
};

void ___termios_debug_before(void)
{
    struct ___termios_debug_data *data = &___termios_debug_data;

    int err = tcgetattr(0, &data->before);
    data->errnum = err ? errno : 0;
}

static const char *___termios_debug_cc_to_str(const struct termios *attr)
{
    static const char *hexlut = "0123456789ABCDEF";
    static char buf[ARRAY_LEN(attr->c_cc) * 3 + 1];

    char *p = &buf[0];
    *p = '\0';

    for (int i = 0; i < ARRAY_LEN(attr->c_cc); i++) {
        unsigned char byte = attr->c_cc[i];
        *p++ = hexlut[(byte >> 4) & 0x0F];
        *p++ = hexlut[(byte >> 0) & 0x0F];
        *p++ = ',';
    }

    p--; // overwrite last ','
    *p = '\0';
    return buf;
}

void ___termios_debug_after(const char *what)
{

//#define _PRINTLN(FMT, ...) fprintf(stderr, FMT "\n",  ##__VA_ARGS__)
#define _PRINTLN(FMT, ...) \
        log_printf(LOG_LEVEL_DBG, NULL, 0, FMT,  ##__VA_ARGS__)

    struct ___termios_debug_data *data = &___termios_debug_data;

    int errnum = data->errnum;
    data->errnum = -1; // set to zero or positive in before func
    if (errnum) {
        _PRINTLN("tcgetattr (before %s) errnum %d\n", what, errnum);
        data->errnum = 0;
        return;
    }

    struct termios after;
    int err = tcgetattr(0, &after);
    if (err) {
        _PRINTLN("tcgetattr (after %s) errnnum %d\n", what, errno);
        return;
    }

    const struct termios *tb = &data->before;
    const struct termios *ta = &after;

    _PRINTLN("termios settings: %s", what);

    _PRINTLN("         iflag   oflag   cflag  lflag [cc]");
    _PRINTLN("before:  %04X    %04X    %04X   %04X  [%s]",
                 tb->c_iflag, tb->c_oflag, tb->c_cflag, tb->c_lflag,
                 ___termios_debug_cc_to_str(tb));

    _PRINTLN("after:   %04X    %04X    %04X   %04X  [%s]",
                 ta->c_iflag, ta->c_oflag, ta->c_cflag, ta->c_lflag,
                 ___termios_debug_cc_to_str(ta));

    // tcflag_t  c_iflag; input modes
    // tcflag_t  c_oflag  output modes
    // tcflag_t  c_cflag  control modes
    // tcflag_t  c_lflag  local modes
    // cc_t c_cc[NCCS]  control chars
#undef _PRINTLN

}

#endif /* CONFIG_TERMIOS_DEBUG */
