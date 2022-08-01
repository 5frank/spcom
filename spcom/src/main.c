#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <stdbool.h>

#include "common.h"
#include "assert.h"
#include "opt.h"
#include "shell.h"
#include "cmd.h"
#include "timeout.h"
#include "outfmt.h"
#include "port.h"
#include "main_opts.h"

struct main_s {
    bool cleanup_done;
    uv_signal_t ev_sigint;
    uv_signal_t ev_sigterm;
};

static struct main_s g_main = { 0 };

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

#define CONFIG_USE_BACKTRACE 1

#if CONFIG_USE_BACKTRACE
#include <execinfo.h>

#define BACKTRACE_LEN 10

struct backtrace_data {
    int nsymbs;
    /// value must be freed
    char **symbs;
};


static void _backtrace_save(struct backtrace_data *bt) 
{
  void *stackptrs[BACKTRACE_LEN];
  // get entries from stack
  bt->nsymbs = backtrace(stackptrs, BACKTRACE_LEN);
  //fprintf(stderr, "Backtrace:\n");
  //backtrace_symbols_fd(stackptrs, size, STDERR_FILENO);
  bt->symbs = backtrace_symbols(stackptrs, bt->nsymbs);
}
#if 0
static void _backtrace_println(int fd, const char *str, size_t len)
{
    size_t len = strlen(str);
    while (len > 0) {
        ssize_t rc = write(fd, str, len);

        if ((rc == -1) && (errno != EINTR))
                break;

        str += (size_t) rc;
        len -= (size_t) rc;
    }
}
#endif

static void _backtrace_print(struct backtrace_data *bt)
{
    if (!bt || bt->nsymbs <= 0) {
        return;
    }

    fputs("Backtrace:\n", stderr);

    for (int i = 0; i < bt->nsymbs; i++) {
        char *s = bt->symbs[i];
        if (!s) {
            break;
        }
        // TODO use log module
        fputs(s, stderr);
        fputc('\n', stderr);
        s++;
    }
}

static void _backtrace_free(struct backtrace_data *bt)
{
    if (!bt || !bt->symbs) {
        return;
    }
    free(bt->symbs);
    bt->symbs = NULL;
}
#endif

void assert_fail(const char* expr, const char *filename, unsigned int line, const char *assert_func)
{
}

int spcom_exit(int exit_code,
               const char *file,
               unsigned int line,
               const char *fmt,
               ...)
{
    /* get backtrace first before we mess with stack */
#if CONFIG_USE_BACKTRACE
    struct backtrace_data *bt = NULL;
    struct backtrace_data bt_data;
    /** EX_SOFTWARE documented as ".. internal software error.." i.e. we
     * probably have a bug */
    if (exit_code == EX_SOFTWARE) {
        bt = &bt_data;
        _backtrace_save(bt);
    }
#endif

    /* ensure exit message(s) on separate line. could use '\r' to clear line
     * but then some data lost */
    outfmt_endline();

    int level = (exit_code == EX_OK) ? LOG_LEVEL_DBG : LOG_LEVEL_ERR;

    va_list args;
    va_start(args, fmt);
    // write to log first - looks better in case log output to stderr
    log_vprintf(level, file, line, fmt, args);
    va_end(args);

#if CONFIG_USE_BACKTRACE
    if (bt) {
        _backtrace_print(bt);
        _backtrace_free(bt);
    }
#endif

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
    // never gets here
    return exit_code;
}



#if 0
void _uvcb_stdin_alloc(uv_handle_t *handle, size_t size, uv_buf_t* buf)
{
  static char buf[1024];
  buf->base = buffer;
  buf->len = sizeof(buffer);
}

void _sercomcb_on_port_writable(int status)
{

    int err = uv_read_start(_main, stdin_hande, 
}
void _uvcb_stdin_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t* buf)
{
    if (nread < 0) {
        uv_close((uv_handle_t*)stream, NULL);
        return;
    }
    port_nb_write(buf->data, nread);
    // stop reading stdin until all data sent to serial port
    int err = uv_read_stop(stream, 

    char buf[BUFSIZ];
    fgets(buf, sizeof(buf), stdin);
    if (buf[strlen(buf)-1] == '\n') {
        // read full line
    } else {
        // line was truncated
    }
}

static void main_init_stdin_pipe(void) 
{
    uv_pipe_t stdin_pipe;
    /* Create a stream that reads from the pipe. */
    r = uv_pipe_init(uv_default_loop(), (uv_pipe_t *)&stdin_pipe, 0);
    ASSERT(r == 0);

    r = uv_pipe_open((uv_pipe_t *)&stdin_pipe, STDIN_FILENO);
    ASSERT(r == 0);

    r = uv_read_start((uv_stream_t *)&stdin_pipe, alloc_buffer, read_stdin);
    ASSERT(r == 0);
}
#endif
void _uvcb_on_signal(uv_signal_t *handle, int signum)
{
    printf("Signal received: %d\n", signum);
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
    assert_uv_z(err, "uv_loop_init");

    err = uv_signal_init(loop, &m->ev_sigint);
    assert_uv_z(err, "uv_signal_init");
    // SIGINT callback will not run on ctrl-c if tty in raw mode
    err = uv_signal_start(&m->ev_sigint, _uvcb_on_signal, SIGINT);
    assert_uv_z(err, "uv_signal_start");

    err = uv_signal_init(loop, &m->ev_sigterm);
    assert_uv_z(err, "uv_signal_init");
    err = uv_signal_start(&m->ev_sigterm, _uvcb_on_signal, SIGTERM);
    assert_uv_z(err, "uv_signal_start");

    if (isatty(STDIN_FILENO)) {
        err = shell_init();
        assert_z(err, "shell_init");
    }
    else {
        // TODO
        return SPCOM_EXIT(EX_UNAVAILABLE, "pipe input not supported yet");
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
            LOG_DBG("uv_loop_close err. %s", log_libuv_errstr(err, errno));
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
        version_print(opts->verbose);
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

