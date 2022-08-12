#include <stdio.h>

#include <uv.h>

#include "common.h"
#include "shell.h"
#include "opq.h"

struct {
    bool initialized;
    uv_tty_t stdin_tty;
} shell_raw;;

static void sh_raw_init(void)
{
    int err;
    uv_tty_t *p_tty = &shell_raw.stdin_tty;

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

    shell_raw.initialized = true;
}

static void sh_raw_enter(void)
{
    int err;

    if (!shell_raw.initialized) {
        sh_raw_init();
    }
#if 1
    // readline messes with this
    err = uv_tty_reset_mode();
    if (err)
        LOG_UV_ERR(err, "uv_tty_reset_mode");
#endif

    uv_tty_t *p_tty = &shell_raw.stdin_tty;

    ___TERMIOS_DEBUG_BEFORE();
    err = uv_tty_set_mode(p_tty, UV_TTY_MODE_RAW);
    ___TERMIOS_DEBUG_AFTER("uv_tty_set_mode_RAW");

    if (err)
        LOG_UV_ERR(err, "uv_tty_set_mode raw");

    // line buffering off
    setvbuf(stdout, NULL, _IONBF, 0);
}

static void sh_raw_leave(void)
{
    uv_tty_t *p_tty = &shell_raw.stdin_tty;

    int err = uv_tty_set_mode(p_tty, UV_TTY_MODE_NORMAL);
    if (err)
        LOG_UV_ERR(err, "uv_tty_set_mode normal");

    // line buffering on - i.e. back to "normal"
    setvbuf(stdout, NULL, _IOLBF, 0);
}

static void sh_raw_insertchar(int c)
{
    opq_enqueue_val(&opq_rt, OP_PORT_PUTC, c);

    if (shell_opts.local_echo)
        putc(c, stdout);
}

static int sh_raw_getchar(void)
{
    return fgetc(stdin);
}

void shell_raw_cleanup(void)
{
    int err;

    if (!shell_raw.initialized)
        return;

    uv_tty_t *p_tty = &shell_raw.stdin_tty;
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

    /* no need to close stdin
     * `uv_close((uv_handle_t*) p_tty, NULL);`
     */
}

static const struct shell_mode_s sh_mode_raw = {
    .enter  = sh_raw_enter,
    .leave  = sh_raw_leave,
    .insert = sh_raw_insertchar,
    .getchar = sh_raw_getchar
};

/// exposed const pointer
const struct shell_mode_s *shell_mode_raw = &sh_mode_raw;
