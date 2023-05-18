#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// deps
#include <uv.h>
// local
#include "assert.h"
#include "cmd.h"
#include "common.h"
#include "main_opts.h"
#include "misc.h"
#include "opt.h"
#include "outfmt.h"
#include "port.h"
#include "port_info.h"
#include "shell.h"
#include "timeout.h"

#ifndef PRE_DEFS_INCLUDE_H_
#warning "no pre_defs.h"
#endif

#ifndef _GNU_SOURCE
#warning "no _GNU_SOURCE"
#endif
struct main_data {
    bool cleanup_done;
    uv_signal_t ev_sigint;
    uv_signal_t ev_sigterm;
    int signum;
};

static struct main_data main_data = { 0 };

int inpipe_init(void);

static void main_cleanup(void);

static void on_uv_close(uv_handle_t *handle)
{
    if (handle) {
        // TODO? free handle resources?
    }
}
static void on_uv_walk(uv_handle_t *handle, void *arg)
{
    uv_close(handle, on_uv_close);
}

static void main_uv_cleanup(void)
{
    int err;
    uv_loop_t *loop = uv_default_loop();

    if (!uv_loop_alive(loop)) {
        LOG_DBG("uv_loop_alive=false");
        return;
    }

    err = uv_loop_close(loop);
    if (err) {
        LOG_UV_DBG(err, "uv_loop_close");

        if (err == UV_EBUSY) {
            uv_walk(loop, on_uv_walk, NULL);
        }
    }

    /* TODO? mabye use uv_library_shutdown() only or not at all? */

#if 0 && UV_VERSION_GT_OR_EQ(1, 38)
    /*
    notes on uv_library_shutdownÖ
        - may only be called once.
        - should not be used when there are still event loops or I/O requests
            active.
        - do not use any libuv functions after called
    */
    uv_library_shutdown();
#endif
}

void spcom_exit(int exit_code, const char *file, unsigned int line,
                const char *fmt, ...)
{
    /* ensure exit message(s) on separate line. could use '\r' to clear line
     * but then some data lost */
    outfmt_endline();

    int level = (exit_code == EX_OK) ? LOG_LEVEL_DBG : LOG_LEVEL_ERR;

    char msg[64];
    va_list args;

    va_start(args, fmt);
    int rc = vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    if (rc < 0) {
        msg[0] = '\0';
    }
    else if (rc >= sizeof(msg)) {
        char *p = &msg[sizeof(msg) - 1];
        *p-- = '\0';
        *p-- = '.';
        *p-- = '.';
        *p-- = '.';
    }

    log_printf(level, file, line, "spcom exit '%s'", msg);

    main_cleanup();

    exit(exit_code);
}

/** note: SIGINT callback will not run on ctrl-c if tty in raw mode */
void _uvcb_on_signal(uv_signal_t *handle, int signum)
{
    LOG_DBG("Signal received: %d", signum);
    /** TODO? is there a prettier way to exit from signal handler?
     * can not call SPCOM_EXIT here as it runs from "interrupt context"?  */

    /* store first signal received. (might not be needed when uv_signal_stop
     * used?)  */
    if (!main_data.signum) {
        main_data.signum = signum;
    }

    /* uv_signal_stop - callback will no longer be called. */
    uv_signal_stop(handle);

    /* calling uv_stop should/will cause uv_run to exit with return unkown
     * error code */
    uv_stop(uv_default_loop());
}

static void main_init(void)
{
    struct main_data *m = &main_data;
    int err = 0;

    uv_loop_t *loop = uv_default_loop();

    err = uv_loop_init(loop);
    assert_uv_ok(err, "uv_loop_init");

    err = uv_signal_init(loop, &m->ev_sigint);
    assert_uv_ok(err, "uv_signal_init");

    err = uv_signal_start(&m->ev_sigint, _uvcb_on_signal, SIGINT);
    assert_uv_ok(err, "uv_signal_start");

    err = uv_signal_init(loop, &m->ev_sigterm);
    assert_uv_ok(err, "uv_signal_init");
    err = uv_signal_start(&m->ev_sigterm, _uvcb_on_signal, SIGTERM);
    assert_uv_ok(err, "uv_signal_start");

    if (isatty(STDIN_FILENO)) {
        err = shell_init();
        assert_z(err, "shell_init");
    }
    else {
        err = inpipe_init();
        assert_z(err, "inpipe_init");
    }

    timeout_init();

    // rx callback == outfmt_write
    port_init(outfmt_write);
}

static void main_cleanup(void)
{
    struct main_data *m = &main_data;
    LOG_DBG("cleaning up...");

    if (m->cleanup_done) {
        LOG_DBG("already cleaned up");
        return;
    }

    timeout_stop();

    shell_cleanup();
    port_cleanup();

    /* uv handles might be closed here. must be after modules that uses them!*/
    main_uv_cleanup();

    outfmt_endline();
    log_cleanup(); // last before exit

    m->cleanup_done = true;
}

static bool main_do_early_exit_opts(void)
{
    // implicit priorty order of show_x options here if multiple flags provided

    if (main_opts->show_help) {
        opt_show_help();
        return true;
    }

    if (main_opts->show_version) {
        misc_print_version(main_opts->verbose);
        return true;
    }

    if (main_opts->show_list) {
        port_info_print_list(main_opts->verbose);
        return true;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int err = 0;

    err = opt_parse_args(argc, argv);
    if (err) {
        exit(EX_USAGE);
        return err;
    }

    log_init();

    bool early_exit = main_do_early_exit_opts();
    if (early_exit) {
        log_cleanup();
        return 0;
    }

    main_init();

    uv_loop_t *loop = uv_default_loop();
    err = uv_run(loop, UV_RUN_DEFAULT);
    /* log err but must always run SPCOM_EXIT handler for cleanup */
    if (err) {
        if (main_data.signum) {
            SPCOM_EXIT(EX_OK, "Signal received: %d", main_data.signum);
        }
        else {
            SPCOM_EXIT(EX_SOFTWARE, "uv_run unhandled error %s",
                       misc_uv_err_to_str(err));
        }
    }

    SPCOM_EXIT(EX_OK, "main");
    // should never get here
    return -1;
}
