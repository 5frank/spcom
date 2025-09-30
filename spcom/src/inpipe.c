/** inpipe - libuv wrapper when reading from piped input */
// std
#include <stdlib.h>
// deps
#include <uv.h>
// local
#include "assert.h"
#include "common.h"
#include "opq.h"

#ifdef __GLIBC__
// _flbf
#include <stdio_ext.h>
#define HAVE_FLBF 1
#endif

// at least 2048
#define IPIPE_BUF_SIZE LINE_MAX

static struct {
    uv_pipe_t stdin_pipe;
    char *buf;
} inpipe_data;

static void _uvcb_alloc(uv_handle_t *handle, size_t size, uv_buf_t *buf)
{
    buf->base = inpipe_data.buf;
    buf->len = IPIPE_BUF_SIZE;
}

static void _uvcb_read(uv_stream_t *stream, ssize_t size, const uv_buf_t *buf)
{
    if (size < 0) {
        uv_close((uv_handle_t *)stream, NULL);
        SPCOM_EXIT(EX_OK, "pipe closed");
        return;
    }
    if (size == 0) {
        // size zero same as EAGAIN?
        LOG_DBG("read cb 0 size");
        return;
    }

    char *line = buf->base;
    bool put_eol = false;

    // assuming stdin pipe is line buffered
    if (line[size - 1] == '\n') {
        // drop eol and replace it with configured char or sequence
        size--;
        put_eol = true;
    }

    // ok to enqueue write even if port not open yet
    opq_enqueue_write(&opq_rt, line, size);
    if (put_eol) {
        opq_enqueue_val(&opq_rt, OP_PORT_PUT_EOL, 1);
    }
    /* stop reading stdin until all data sent to serial port, otherwise memory
     * usage will grow as serial port output is most likely much slower then
     * reading stdin. */
    int err = uv_read_stop(stream);
    // uv_read_stop() will always succeed according to doc
    (void)err;
}

static void _on_port_write_done(const struct opq_item *itm)
{
    assert(itm->u.data);
    uv_pipe_t *pipe = &inpipe_data.stdin_pipe;

    int err = uv_read_start((uv_stream_t *)pipe, _uvcb_alloc, _uvcb_read);
    switch (err) {
        case 0:
            break;
        case UV_EINVAL:
            uv_close((uv_handle_t *)pipe, NULL);
            SPCOM_EXIT(EX_OK, "pipe closed");
            break;
        case UV_EALREADY:
        default:
            LOG_WRN("unexpected uv_read_start retval %d", err);
            break;
    }
}

int inpipe_init(void)
{
    int err;
#if HAVE_FLBF
    // __flbf returns nonzero if stream is line buffered.
    if (!__flbf(stdin)) {
        LOG_WRN("stdin pipe is not line buffered");
    }
#endif

    opq_set_free_cb(&opq_rt, _on_port_write_done);

    // allocate once and reuse. 
    inpipe_data.buf = malloc(IPIPE_BUF_SIZE);
    assert(inpipe_data.buf);

    uv_pipe_t *pipe = &inpipe_data.stdin_pipe;
    /* Create a stream that reads from the pipe. */
    err = uv_pipe_init(uv_default_loop(), pipe, 0);
    assert_uv_ok(err, "uv_pipe_init");

    err = uv_pipe_open(pipe, STDIN_FILENO);
    assert_uv_ok(err, "uv_pipe_open");

    err = uv_read_start((uv_stream_t *)pipe, _uvcb_alloc, _uvcb_read);
    assert_uv_ok(err, "uv_read_start");
    return err;
}
