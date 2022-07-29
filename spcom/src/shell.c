
#define _GNU_SOURCE
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <readline/readline.h>
#include <readline/history.h>
#include <uv.h>

#include "common.h"
#include "assert.h"
#include "cmd.h"
#include "opt.h"
#include "port.h"
#include "opq.h"
#include "keybind.h"
#include "vt_defs.h"
#include "shell.h"

extern const struct shell_mode_s *shell_mode_raw;
extern const struct shell_mode_s *shell_mode_cooked;
extern const struct shell_mode_s *shell_mode_cmd;

static struct shell_opts_s shell_opts = {
    .cooked = false,
};

static struct shell_s {
    bool initialized;
    //char history[256];
    const struct shell_mode_s *current_mode;
    const struct shell_mode_s *default_mode;
    struct keybind_state kb_state;

    uv_tty_t stdin_tty;
    char stdin_rd_buf[32];

    uv_tty_t stdout_tty;
    uv_tty_t stderr_tty;
    int prev_c;

} shell = { 0 };

const void *shell_output_lock(void)
{
    const struct shell_mode_s *shm = shell.current_mode;
    if (!shm)
        return NULL; // shell not initialized yet

    if (shm->lock)
        shm->lock();

    return shm;
}

void shell_output_unlock(const void *x)
{
    const struct shell_mode_s *shm_x = x;
    if (!shm_x)
        return; // shell not initialized yet

    if (shm_x != shell.current_mode)
        LOG_WRN("shell mode changed during output lock");

    // unlock anyways
    const struct shell_mode_s *shm = shell.current_mode;
    if (shm->unlock)
        shm->unlock();
}

static void shell_set_mode(const struct shell_mode_s *new_m)
{
    const struct shell_mode_s *old_m = shell.current_mode;
    if (new_m == old_m)
        return;

    if (old_m)
        old_m->leave();

    new_m->enter();
    shell.current_mode = new_m;
}

static void shell_tog_cmd_mode(void)
{
    const struct shell_mode_s *new_m;
    if (shell.current_mode == shell_mode_cmd)
        new_m = shell.default_mode;
    else
        new_m = shell_mode_cmd;

    shell_set_mode(new_m);
}

static void shell_input_putc(int prev_c, int c)
{
    // need to catch ctrl keys before libreadline

    /* capture input before readline as it will make it's own iterpretation. also
     * not possible to bind the escape */
    // TODO forward some ctrl:s
    /** UP: 91 65
     * DOWN: 91, 66
     */
    const struct shell_mode_s *shm = shell.current_mode;
    struct keybind_state *kb_state = &shell.kb_state;

    int action = keybind_eval(kb_state, c);

    //LOG_DBG("input: %d, action: %d", (int)c, action);

    switch (action) {
        case K_ACTION_CACHE:
            kb_state->cache[0] = c;
            break;

        case K_ACTION_PUTC:
            shm->input_putc(c);
            break;

        case K_ACTION_UNCACHE:
            shm->input_putc(kb_state->cache[0]);
            shm->input_putc(c);
            break;

        case K_ACTION_EXIT:
            SPCOM_EXIT(0, "user key");
            break;

        case K_ACTION_TOG_CMD_MODE:
            shell_tog_cmd_mode();
            break;

        default:
            LOG_ERR("unkown action %d", action);
            break;
    }
}

void shell_printf(int fd, const char *fmt, ...)
{
    va_list args;
    const void *lock = shell_output_lock();
    va_start(args, fmt);
    vdprintf(fd, fmt, args);
    va_end(args);

    shell_output_unlock(lock);
}

/** as unbuffered stdin and single threaded - use single small static buffer */
static void _uvcb_stdin_stalloc(uv_handle_t *handle, size_t size, uv_buf_t *buf)
{
    buf->base = shell.stdin_rd_buf;
    buf->len  = sizeof(shell.stdin_rd_buf);
}

static void _uvcb_stdin_read(uv_stream_t *tty_in, ssize_t nread,
                         const uv_buf_t *buf)
{
    if (nread < 0) {
        LOG_ERR("stdin read %zi", nread); // TODO exit application?
        // uv_close((uv_handle_t*) tty_in, NULL);
        return;
    }
    else if (nread == 0) {
        /* not an error.  nread might be 0, which does not indicate an error or
         * EOF. This is equivalent to EAGAIN or EWOULDBLOCK under read(2). this
         * callback will run again automagicaly */
        return;
    }

    const char *sp = buf->base;
    size_t size = nread;
    int prev_c = shell.prev_c;
    for (size_t i = 0; i < size; i++) {
        int c = *sp++;
        shell_input_putc(prev_c, c);
        prev_c = c;
    }

    shell.prev_c = prev_c;
}

int shell_init(void)
{
    int err = 0;

    // always initalize readline even if raw. need it for command mode
    err = rl_initialize();
    assert_z(err, "rl_initialize");

    shell.default_mode = (shell_opts.cooked)
        ? shell_mode_cooked
        : shell_mode_raw;

    err = shell.default_mode->init(&shell_opts);
    if (err)
        return err;

    err = shell_mode_cmd->init(&shell_opts);
    if (err)
        return err;

    _Static_assert(STDIN_FILENO == 0, "conflicting stdin fileno w libuv");

    assert(isatty(STDIN_FILENO)); // should already be checked

    uv_loop_t *loop = uv_default_loop();
    uv_tty_t *p_tty = &shell.stdin_tty;

    err = uv_tty_init(loop, p_tty, STDIN_FILENO, 0);
    assert_uv_z(err, "uv_tty_init");

    err = uv_tty_set_mode(p_tty, UV_TTY_MODE_RAW);
    assert_uv_z(err, "uv_tty_set_mode raw");

    assert(uv_is_readable((uv_stream_t *)p_tty));

    err = uv_read_start((uv_stream_t *)p_tty,
                        _uvcb_stdin_stalloc,
                        _uvcb_stdin_read);
    assert_uv_z(err, "uv_read_start");

#if UV_VERSION_GT_OR_EQ(1, 33)
    // enable ansi escape sequence(s) on some windows shells
    uv_tty_set_vterm_state(UV_TTY_SUPPORTED);
    uv_tty_vtermstate_t vtermstate;
    uv_tty_get_vterm_state(&vtermstate);
    (void) vtermstate;
    // fallback to SetConsoleMode(handle, ENABLE_VIRTUAL_TERMINAL_PROCESSING);?
#endif

    shell_set_mode(shell.default_mode);

    shell.initialized = true;
    return 0;
}

void shell_cleanup(void)
{
    if (!shell.initialized)
        return;

    int err = 0;
    // if (shell.history[0] != '\0')
    //write_history(shell.history);

#if 0
    // this returns EBADF if tty(s) already closed
    err = uv_tty_reset_mode();
    if (err)
        LOG_UV_ERR(err, "tty mode reset");

#else
    uv_tty_t *p_tty = &shell.stdin_tty;
    if (uv_is_active((uv_handle_t *)p_tty)) {
        err = uv_tty_set_mode(p_tty, UV_TTY_MODE_NORMAL);
        if (err)
            LOG_UV_ERR(err, "tty mode normal stdin");
        uv_close((uv_handle_t*) p_tty, NULL);
    }
#endif

    rl_message("");
    rl_callback_handler_remove();

    shell.initialized = false;
}

static int shell_opts_post_parse(const struct opt_section_entry *entry)
{
    // note: do not use LOG here

    if (shell_opts.sticky)
        shell_opts.cooked = true;

    return 0;
}

static const struct opt_conf shell_opts_conf[] = {
    {
        .name = "cooked",
        .shortname = 'C',
        .alias = "canonical",
        .dest = &shell_opts.cooked,
        .parse = opt_ap_flag_true,
        .descr = "cooked (or canonical) mode - input from stdin are stored "
            "and can be edited in local line buffer before sent over serial "
            "port." " This opposed to default raw (or non-canonical) mode "
            "where all input from stdin is directly sent over serial port, "
            "(including most control keys such as ctrl-A)"
    },
    {
        .name = "sticky",
        .dest = &shell_opts.sticky,
        .parse = opt_ap_flag_true,
        .descr = "sticky promt that keep input characters on same line."
            "This option will implicitly set cooked (canonical) mode"
    },
#if 0 // TODO
    {
        .name = "echo",
        .alias = "lecho",
        .dest = &shell_opts.echo,
        .parse = opt_ap_flag_true,
        .descr = "local echo input characters on stdout"
    },
#endif
};

OPT_SECTION_ADD(shell,
                shell_opts_conf,
                ARRAY_LEN(shell_opts_conf),
                shell_opts_post_parse);
