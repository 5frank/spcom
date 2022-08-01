#include <uv.h>

#include "common.h"
#include "assert.h"
#include "opq.h"

static struct {
    uv_pipe_t stdin_pipe;
    char buf[1024];
} ipipe_data;

static void _uvcb_stdin_alloc(uv_handle_t *handle, size_t size, uv_buf_t* buf)
{
  buf->base = ipipe_data.buf;
  buf->len = sizeof(ipipe_data.buf);
}

static void _uvcb_stdin_read(uv_stream_t *stream, ssize_t size, const uv_buf_t* buf)
{
    if (size < 0) {
        uv_close((uv_handle_t*)stream, NULL);
        SPCOM_EXIT(EX_OK, "pipe closed");
        return;
    }

    opq_enqueue_write(&opq_rt, buf->base, size);

    // stop reading stdin until all data sent to serial port
    int err = uv_read_stop(stream);
    // uv_read_stop() will always succeed according to doc
    (void) err;
}

static void _on_port_write_done(const struct opq_item *itm)
{
    assert(itm->u.data);
    uv_pipe_t *pipe = &ipipe_data.stdin_pipe;

    int err = uv_read_start((uv_stream_t *)pipe,
                         _uvcb_stdin_alloc, _uvcb_stdin_read);
    switch(err) {
        case 0:
            break;
        case UV_EINVAL:
            uv_close((uv_handle_t*)pipe, NULL);
            SPCOM_EXIT(EX_OK, "pipe closed");
            break;
        case UV_EALREADY:
        default:
            LOG_WRN("unexpected uv_read_start retval %d", err);
            break;
    }
}

void ipipe_init(void)
{
    int err;

    opq_set_write_done_cb(&opq_rt, _on_port_write_done);

    uv_pipe_t *pipe = &ipipe_data.stdin_pipe;
    /* Create a stream that reads from the pipe. */
    err = uv_pipe_init(uv_default_loop(), pipe, 0);
    assert_uv_z(err, "uv_pipe_init");

    err = uv_pipe_open(pipe, STDIN_FILENO);
    assert_uv_z(err, "uv_pipe_open");

    err = uv_read_start((uv_stream_t *)pipe,
                         _uvcb_stdin_alloc, _uvcb_stdin_read);
    assert_uv_z(err, "uv_read_start");
}

