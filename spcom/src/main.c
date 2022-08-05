#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <stdbool.h>

#include "common.h"
#include "misc.h"
#include "assert.h"
#include "opt.h"
#include "shell.h"
#include "cmd.h"
#include "timeout.h"
#include "outfmt.h"
#include "port.h"
#include "port_info.h"
#include "main_opts.h"

struct main_s {
    bool cleanup_done;
    uv_signal_t ev_sigint;
    uv_signal_t ev_sigterm;
};

static struct main_s g_main = { 0 };

int ipipe_init(void);

static void main_cleanup(void);

static void on_uv_close(uv_handle_t* handle)
{
    if (handle) {
        // TODO free handle resources
    }
}
static void on_uv_walk(uv_handle_t* handle, void* arg)
{
    uv_close(handle, on_uv_close);
}

void spcom_exit(int exit_code,
                const char *file,
                unsigned int line,
                const char *fmt,
                ...)
{
    /* ensure exit message(s) on separate line. could use '\r' to clear line
     * but then some data lost */
    outfmt_endline();

    int level = (exit_code == EX_OK) ? LOG_LEVEL_DBG : LOG_LEVEL_ERR;

    va_list args;
    va_start(args, fmt);
    // write to log first - looks better in case log output to stderr
    log_vprintf(level, file, line, fmt, args);
    va_end(args);

    uv_loop_t *loop = uv_default_loop();
#if 1
    int result = uv_loop_close(loop);
    if (result == UV_EBUSY) {
        uv_walk(loop, on_uv_walk, NULL);
    }
#else

    g_main.exit_status = exit_code;
    if (uv_loop_alive(loop)) {
        LOG_DBG("loop alive - stopping it. exit from callback?");
        uv_stop(g_main.loop);
        return;
    }
    if (g_main.loop) {
        LOG_ERR("loop not alive but not null");
    }
#endif
    //TODO use uv_library_shutdown()!?
    main_cleanup();
    exit(exit_code);
}

void _uvcb_on_signal(uv_signal_t *handle, int signum)
{
    LOG_DBG("Signal received: %d", signum);
    //uv_signal_stop(handle);
    uv_stop(uv_default_loop());
    // spcom_exit(EXIT_SUCCESS); // TODO
}


#if 0
void uv_loop_delete(uv_loop_t* loop) {
  uv_ares_destroy(loop, loop->channel);
  ev_loop_destroy(loop->ev);
#if __linux__
  if (loop->inotify_fd == -1) return;
  ev_io_stop(loop->ev, &loop->inotify_read_watcher);
  close(loop->inotify_fd);
  loop->inotify_fd = -1;
#endif
#if HAVE_PORTS_FS
  if (loop->fs_fd != -1)
    close(loop->fs_fd);
#endif
}
#endif

static int main_init(void)
{
    struct main_s *m = &g_main;
    int err = 0;
    uv_loop_t *loop = uv_default_loop();

    err = uv_loop_init(loop);
    assert_uv_ok(err, "uv_loop_init");

    err = uv_signal_init(loop, &m->ev_sigint);
    assert_uv_ok(err, "uv_signal_init");
    // SIGINT callback will not run on ctrl-c if tty in raw mode
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
        err = ipipe_init();
        assert_z(err, "ipipe_init");
        //return SPCOM_EXIT(EX_UNAVAILABLE, "pipe input not supported yet");
    }

    timeout_init();

    // rx callback == outfmt_write
    err = port_init(outfmt_write);
    if (err) {
        // assume errora already logged
        return err;
    }
    return 0;
}

static void main_cleanup(void)
{
    int err = 0;
    struct main_s *m = &g_main;
    uv_loop_t *loop = uv_default_loop();
    LOG_DBG("cleaning up...");

    if (m->cleanup_done) {
        LOG_DBG("already cleaned up");
        return;
    }

    timeout_stop();

    shell_cleanup();
    port_cleanup();
    if (uv_loop_alive(loop)) {
        err = uv_loop_close(loop);
        // only log debug on error as going to exit
        if (err) {
            LOG_DBG("uv_loop_close err. %s", misc_uv_err_to_str(err));
        }
    }

    outfmt_endline();
    log_cleanup(); // last before exit

    m->cleanup_done = true;
}

static bool main_do_early_exit_opts(void)
{
    const struct main_opts_s *opts = main_opts_get();

    // implicit priorty order of show_x options here if multiple flags provided

    if (opts->show_help) {
        opt_show_help();
        return true;
    }

    if (opts->show_version) {
        misc_print_version(opts->verbose);
        return true;
    }

    if (opts->show_list) {
        port_info_print_list(opts->verbose);
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

    err = main_init();
    if (err) {
        main_cleanup();
        return EXIT_FAILURE;
    }

    uv_loop_t *loop = uv_default_loop();
    err = uv_run(loop, UV_RUN_DEFAULT);
    /* log err but must always run SPCOM_EXIT handler for cleanup */
    if (err) {
        LOG_UV_ERR(err, "uv_run");
        SPCOM_EXIT(EX_SOFTWARE, "main");
    }


    SPCOM_EXIT(EX_OK, "main");
    // should never get here
    return -1;
}

