#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <termios.h>

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
#include "shell_rl.h"

extern void shell_raw_cleanup(void);
extern const struct shell_mode_s *shell_mode_raw;
extern const struct shell_mode_s *shell_mode_cooked;
//extern const struct shell_mode_s *shell_mode_cmd;

static struct shell_opts_s _shell_opts = {
    .cooked = false,
};

const struct shell_opts_s *shell_opts = &_shell_opts;

static struct shell_s {
    bool initialized;
    uv_poll_t poll_handle;
    bool have_term_attr;
    struct termios term_attr;
    //char history[256];
    const struct shell_mode_s *current_mode;
    const struct shell_mode_s *default_mode;
    struct keybind_state kb_state;

    //uv_tty_t stdin_tty;

} shell_data = { 0 };

const void *shell_output_lock(void)
{
    const struct shell_mode_s *shm = shell_data.current_mode;
    if (!shm)
        return NULL; // shell not initialized yet

    if (!shm->lock)
        return NULL;

    return shm->lock();
}

void shell_output_unlock(const void *x)
{
    const struct shell_mode_s *shm = shell_data.current_mode;
    if (!shm)
        return; // shell not initialized yet

    if (shm->unlock)
        shm->unlock(x);
}

static void shell_set_mode(const struct shell_mode_s *new_m)
{
    const struct shell_mode_s *old_m = shell_data.current_mode;
    if (new_m == old_m)
        return;

    if (old_m)
        old_m->leave();

    new_m->enter();
    shell_data.current_mode = new_m;
}

static void shell_toggle_cmd_mode(void)
{
    const struct shell_mode_s *new_m;
#if 1
#warning "not cmd mode"
    if (shell_data.current_mode == shell_mode_cooked)
        new_m = shell_mode_raw;
    else
        new_m = shell_mode_cooked;
#else
    if (shell_data.current_mode == shell_mode_cmd)
        new_m = shell_data.default_mode;
    else
        new_m = shell_mode_cmd;
#endif

    shell_set_mode(new_m);
}

static int _write_all(int fd, const void *data, size_t size)
{
    const char *p = data;

    while (1) {
        int rc = write(fd, p, size);
        if (rc < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }

            return -errno;
        }

        if (rc >= size) {
            return 0;
        }

        p += rc;
        size -= rc;
    }
}

void shell_write(int fd, const void *data, size_t size)
{
    const void *state = shell_rl_save();
    int err = _write_all(fd, data, size);
    shell_rl_restore(state);

    assert(!err);
}

void shell_printf(int fd, const char *fmt, ...)
{
    va_list args;
    const void *state = shell_rl_save();

    va_start(args, fmt);
    vdprintf(fd, fmt, args);
    va_end(args);

    shell_rl_restore(state);
}

static void _stdin_read_char(void)
{

    const struct shell_mode_s *shm = shell_data.current_mode;
    assert(shm);

    // TODO should be safe to use getchar() here
    int c = shm->getchar();

    if (c == EOF) {
        SPCOM_EXIT(EX_IOERR, "stdin EOF");
        return;
    }

    /* capture input before readline as it will make it's own iterpretation.
     * rl_bind_key() could also be used but will not work in raw mode
     * */
    struct keybind_state *kb_state = &shell_data.kb_state;

    int action = keybind_eval(kb_state, c);

    //LOG_DBG("input: %d, action: %d", (int)c, action);

    switch (action) {
        case K_ACTION_CACHE:
            kb_state->cache[0] = c;
            break;

        case K_ACTION_PUTC:
            shm->insert(c);
            break;

        case K_ACTION_UNCACHE:
            shm->insert(kb_state->cache[0]);
            shm->insert(c);
            break;

        case K_ACTION_EXIT:
            SPCOM_EXIT(0, "user key");
            break;

        case K_ACTION_TOG_CMD_MODE:
            shell_toggle_cmd_mode();
            break;

        default:
            LOG_WRN("unkown action %d for c: %d", action, c);
            break;
    }
}

static void _on_stdin_data_avail(uv_poll_t *handle, int status, int events)
{
    if (status == UV_EBADF) {
        SPCOM_EXIT(EX_IOERR, "stdin EBADF");
        return;
    }

    if (status) {
        LOG_ERR("unexpected uv poll status %d", status);
        return;
    }

    if (!(events & UV_READABLE)) {
        LOG_WRN("unexpected events 0x%X", events);
        return;
    }

    // this callback will be called again if more data availabe
    _stdin_read_char();
}

static void shell_opq_write_done_cb(const struct opq_item *itm)
{
    assert(itm->u.data);
    free(itm->u.data);
}

int shell_init(void)
{
    int err = 0;

    _Static_assert(STDIN_FILENO == 0, "conflicting stdin fileno w libuv");

    assert(isatty(STDIN_FILENO)); // should already be checked

    opq_set_write_done_cb(&opq_rt, shell_opq_write_done_cb);

    err = tcgetattr(STDIN_FILENO, &shell_data.term_attr);
    if (err) {
        LOG_ERRNO(errno, "tcgetattr");
        return err;
    }
    shell_data.have_term_attr = true;

    uv_loop_t *loop = uv_default_loop();

    err = uv_poll_init(loop, &shell_data.poll_handle, STDIN_FILENO);
    assert_uv_ok(err, "uv_poll_init");

    err = uv_poll_start(&shell_data.poll_handle,
                        UV_READABLE,
                        _on_stdin_data_avail);
    assert_uv_ok(err, "uv_poll_start");

    shell_data.default_mode = (shell_opts->cooked)
        ? shell_mode_cooked
        : shell_mode_raw;
    shell_set_mode(shell_data.default_mode);

    shell_data.initialized = true;
    return 0;
}

/**
 * neither libuv nor libreadline seems to be able to restore the terminal
 * correctly (or they might be in conlfict with each other?). */
static int shell_restore_term(void)
{
    int err;

    const struct termios *org = &shell_data.term_attr;
    /* From man page: "Note that tcsetattr() returns success if any of the
       requested changes could be successfully carried out" */
    err = tcsetattr(STDIN_FILENO, TCSANOW, org);
    if (err) {
        LOG_ERRNO(errno, "tcsetattr");
        return err;
    }

    struct termios tmp = { 0 };
    err = tcgetattr(STDIN_FILENO, &tmp);
    if (err) {
        LOG_ERRNO(errno, "tcgetattr");
        return err;
    }

    bool success = !memcmp(&tmp, org, sizeof(struct termios));
    if (!success) {
        LOG_ERR("some terminal settings not restored");
        return -1;
    }

    return 0;
}

void shell_cleanup(void)
{
    if (shell_data.initialized) {
        shell_raw_cleanup();
        shell_rl_cleanup();
        shell_data.initialized = false;
    }

    // always restore if possible. last!
    if (shell_data.have_term_attr) {
        shell_restore_term();
    }
}

static int shell_opts_post_parse(const struct opt_section_entry *entry)
{
    // note: do not use LOG here

    return 0;
}

static const struct opt_conf shell_opts_conf[] = {
    {
        .name = "cooked",
        .shortname = 'C',
        .alias = "canonical",
        .dest = &_shell_opts.cooked,
        .parse = opt_parse_flag_true,
        .descr = "cooked (or canonical) mode - input from stdin are stored "
            "and can be edited in local line buffer before sent over serial "
            "port." " This opposed to default raw (or non-canonical) mode "
            "where all input from stdin is directly sent over serial port, "
            "(including most control keys such as ctrl-A)"
    },
    {
        .name = "sticky",
        .dest = &_shell_opts.sticky,
        .parse = opt_parse_flag_true,
        .descr = "sticky promt that keep input characters on same line."
            "Only applies when in coocked mode"
    },
    {
        .name = "echo",
        .alias = "lecho",
        .dest = &_shell_opts.local_echo,
        .parse = opt_parse_flag_true,
        .descr = "local echo input characters on stdout"
    },

};

OPT_SECTION_ADD(shell,
                shell_opts_conf,
                ARRAY_LEN(shell_opts_conf),
                shell_opts_post_parse);
