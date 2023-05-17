#include <stdio.h>

#include <uv.h>

#include "common.h"
#include "shell.h"
#include "log.h"
#include "opq.h"
#include "assert.h"

static struct {
    bool initialized;
    uv_tty_t stdin_tty;
} sh_raw_data;;

static void sh_raw_init(void)
{
    int err;

    if (sh_raw_data.initialized) {
        return;
    }

    uv_tty_t *p_tty = &sh_raw_data.stdin_tty;

    err = uv_tty_init(uv_default_loop(), p_tty, STDIN_FILENO, 0);
    assert_uv_ok(err, "uv_tty_init");

#if UV_VERSION_GT_OR_EQ(1, 33)
    // enable ansi escape sequence(s) on some windows shells
    uv_tty_set_vterm_state(UV_TTY_SUPPORTED);
    uv_tty_vtermstate_t vtermstate;
    uv_tty_get_vterm_state(&vtermstate);
    (void) vtermstate;
    // fallback to SetConsoleMode(handle, ENABLE_VIRTUAL_TERMINAL_PROCESSING);?
#endif

#if 0 // TODO use
    // readline messes with this
    err = uv_tty_reset_mode();
    if (err)
        LOG_UV_ERR(err, "uv_tty_reset_mode");
#endif

    ___TERMIOS_DEBUG_BEFORE();
    err = uv_tty_set_mode(p_tty, UV_TTY_MODE_RAW);
    ___TERMIOS_DEBUG_AFTER("uv_tty_set_mode RAW");

    if (err)
        LOG_UV_ERR(err, "uv_tty_set_mode raw");

    // line buffering off
    setvbuf(stdout, NULL, _IONBF, 0);

    sh_raw_data.initialized = true;
}

static void sh_raw_exit(void)
{
    if (!sh_raw_data.initialized) {
        return;
    }

    uv_tty_t *p_tty = &sh_raw_data.stdin_tty;

    int err = uv_tty_set_mode(p_tty, UV_TTY_MODE_NORMAL);
    if (err)
        LOG_UV_ERR(err, "uv_tty_set_mode NORMAL");

    // line buffering on - i.e. back to "normal"
    setvbuf(stdout, NULL, _IOLBF, 0);

#if 0
    if (!uv_is_active((uv_handle_t *)p_tty)) {
        LOG_DBG("uv_tty_t stdint not active");
        return;
    }
#endif

#if 0
    ___TERMIOS_DEBUG_BEFORE();
    err = uv_tty_set_mode(p_tty, UV_TTY_MODE_NORMAL);
    ___TERMIOS_DEBUG_AFTER("uv_tty_set_mode_NORMAL");

    if (err)
        LOG_UV_ERR(err, "uv_tty_set_mode normal");
#else
    // this returns EBADF if tty(s) already closed
    ___TERMIOS_DEBUG_BEFORE();
    err = uv_tty_reset_mode();
    ___TERMIOS_DEBUG_AFTER("uv_tty_reset_mode");

    if (err)
        LOG_UV_ERR(err, "uv_tty_reset_mode");
#endif

    sh_raw_data.initialized = false;
}

static void sh_raw_insertchar(int c)
{
    opq_enqueue_val(&opq_rt, OP_PORT_PUTC, c);

    if (shell_opts->local_echo)
        putc(c, stdout);
}

static int sh_raw_getchar(void)
{
#if 1
    char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n != 1) // TODO check errno
        return EOF;
    return c;
#else
    return fgetc(stdin);
#endif

}

static const struct shell_mode_s sh_mode_raw = {
    .init    = sh_raw_init,
    .exit    = sh_raw_exit,
    .insert  = sh_raw_insertchar,
    .getchar = sh_raw_getchar
};

/// exposed const pointer
const struct shell_mode_s *shell_mode_raw = &sh_mode_raw;
