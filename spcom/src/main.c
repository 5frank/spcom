#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <stdbool.h>

#include "assert.h"
#include "opt.h"
#include "shell.h"
#include "utils.h"
#include "cmd.h"
#include "timeout.h"
#include "outfmt.h"
#include "port.h"
#include "main.h"

struct main_s {
    int exit_status;
    bool cleanup_done;
    uv_signal_t ev_sigint;
    uv_signal_t ev_sigterm;
};

static struct main_s g_main = { 0 };

void main_cleanup(void);
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
#if 0
#include <execinfo.h>
static void print_backtrace(void) 
{
  void *stackptrs[10];
  // get entries from stack
  size_t size = backtrace(stackptrs, 10);
  fprintf(stderr, "Backtrace:\n");
  backtrace_symbols_fd(stackptrs, size, STDERR_FILENO);
}
#else
static void print_backtrace(void) {}
#endif

void main_exit(int status) 
{
    if (status != EXIT_SUCCESS)
        print_backtrace();

    uv_loop_t *loop = uv_default_loop();
#if 1
    int result = uv_loop_close(loop);
    if (result == UV_EBUSY) {
        uv_walk(loop, on_uv_walk, NULL);
    }
#else

    g_main.exit_status = status;
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
    exit(status);
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
    // main_exit(EXIT_SUCCESS); // TODO
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

int main_init(void)
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

    err = timeout_init(main_exit);
    assert(!err);

    // rx callback == outfmt_write
    err = port_init(outfmt_write);
    if (err) {
        LOG_ERR("port_init err=%d", err);
        return err;
    }
    return 0;
}

void main_cleanup(void)
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

    outfmt_cleanup();
    log_cleanup(); // last before exit

    m->cleanup_done = true;
}

int main(int argc, char *argv[])
{
    int err = 0;

    err = opt_parse_args(argc, argv);
    if (err)
        return err;

    err = log_init();
    if (err)
        return err;

    err = main_init();
    if (err) {
        LOG_ERR("main_init failed");
        main_cleanup();
        return EXIT_FAILURE;
    }

    uv_loop_t *loop = uv_default_loop();
    err = uv_run(loop, UV_RUN_DEFAULT);

    if (err) {
        LOG_UV_ERR(err, "uv_run");
    }

    main_cleanup();

    return g_main.exit_status;
}

