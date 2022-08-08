
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
#include "shell_rl.h"

extern void shell_raw_cleanup(void);
extern const struct shell_mode_s *shell_mode_raw;
extern const struct shell_mode_s *shell_mode_cooked;
//extern const struct shell_mode_s *shell_mode_cmd;

struct shell_opts_s shell_opts = {
    .cooked = false,
};

static struct shell_s {
    bool initialized;
    uv_poll_t poll_handle;

    //char history[256];
    const struct shell_mode_s *current_mode;
    const struct shell_mode_s *default_mode;
    struct keybind_state kb_state;

    //uv_tty_t stdin_tty;

} shell_data = { 0 };

const void *shell_output_lock(void)
{
    return shell_rl_save();
}

void shell_output_unlock(const void *x)
{
    shell_rl_restore(x);
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
        new_m = shell_data.default_mode;
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

void shell_printf(int fd, const char *fmt, ...)
{
    va_list args;
    const void *lock = shell_output_lock();
    va_start(args, fmt);
    vdprintf(fd, fmt, args);
    va_end(args);

    shell_output_unlock(lock);
}

static void _raw_insert_char(char c)
{
    opq_enqueue_val(&opq_rt, OP_PORT_PUTC, c);

    if (shell_opts.local_echo)
        putc(c, stdout);
}

static void _stdin_read_char(void)
{

    const struct shell_mode_s *shm = shell_data.current_mode;
    assert(shm);

    /* it seems that if libreadline initalized in any way, stdin should _only_ be read
     * by libreadline. i.e. must use `rl_getc()`
     * 
     * libreadline "remaps" '\n' to '\r'. this is done through some
     * tty settings, so even if `fgetc(stdin)` "normaly" returns '\n' for enter
     * key - after libreadline internaly have called rl_prep_term(), the return
     * value will be '\r'.
     * */
    int c = shm->getchar();

    if (c == EOF) {
        SPCOM_EXIT(EX_IOERR, "stdin EOF");
        return;
    }

    // need to catch ctrl keys before libreadline

    /* capture input before readline as it will make it's own iterpretation. 
     * rl_bind_key() could also be used
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

static void _on_stdin_data_avail(uv_poll_t* handle, int status, int events)
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


    shell_data.default_mode = (shell_opts.cooked)
        ? shell_mode_cooked
        : shell_mode_raw;

    shell_set_mode(shell_data.default_mode);

    uv_loop_t *loop = uv_default_loop();

    err = uv_poll_init(loop, &shell_data.poll_handle, STDIN_FILENO);
    assert_uv_ok(err, "uv_poll_init");

    err = uv_poll_start(&shell_data.poll_handle, UV_READABLE, _on_stdin_data_avail);
    assert_uv_ok(err, "uv_poll_start");

    shell_data.initialized = true;
    return 0;
}


void shell_cleanup(void)
{
    if (!shell_data.initialized)
        return;

    int err = 0;
    // if (shell_data.history[0] != '\0')
    //write_history(shell_data.history);

    shell_raw_cleanup();
    shell_rl_cleanup();

    shell_data.initialized = false;
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
    {
        .name = "echo",
        .alias = "lecho",
        .dest = &shell_opts.local_echo,
        .parse = opt_ap_flag_true,
        .descr = "local echo input characters on stdout"
    },

};

OPT_SECTION_ADD(shell,
                shell_opts_conf,
                ARRAY_LEN(shell_opts_conf),
                shell_opts_post_parse);
