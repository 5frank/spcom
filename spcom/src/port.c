
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>
#include <libserialport.h>

#include <unistd.h> // access

#include "common.h"
#include "misc.h"
#include "assert.h"
#include "opt.h"
#include "eol.h"
#include "log.h"
#include "cmd.h"
#include "opq.h"
#include "str.h"
#include "port_wait.h"
#include "port.h"

#if 1
static char __log_txrx_data[64];

#define __LOG_TXRX(TX_OR_RX, DATA, SIZE) do { \
    unsigned int _size = SIZE; \
    str_escape_nonprint(__log_txrx_data, \
            sizeof(__log_txrx_data), DATA, _size); \
    LOG_DBG("%s (%u) \"%s\"", TX_OR_RX, _size, __log_txrx_data); \
} while(0)
#else
#define __LOG_TXRX(TX_OR_RX, DATA, SIZE) do { } while (0)
#endif
/**
On windows only sockets can be polled with poll handles. On Unix any file
descriptor that would be accepted by poll(2) can be used.  
seems possible with (WSA)WaitForMultipleEvents. example a possible workaround:
https://github.com/libuv/help/issues/82
https://stackoverflow.com/questions/19955617/win32-read-from-stdin-with-timeout
*/
#ifdef _WIN32
#error "libuv poll do not work with `HANDLE`, only on sockets"
#endif

enum port_state_e {
    PORT_STATE_UNKNOWN = 0,
    /// waiting on port to be connected
    PORT_STATE_WAITING,
    /// Busy configure, writing etc
    PORT_STATE_BUSY,
    /// TXRX ready
    PORT_STATE_READY
};

static struct port_s {
    struct sp_port *port;
    struct sp_port_config *usr_config;
    struct sp_port_config *org_config;
    bool have_org_config;
    uv_poll_t poll_handle;
    uv_prepare_t prepare_handle;
    uv_timer_t t_sleep;
    uint64_t ts_lastc;
    size_t offset;
    struct opq_item *current_op;
    enum port_state_e state;
    port_rx_cb_fn *rx_cb;
    char eol[3];
    unsigned char eol_len;
} port_data = {0};


static void port_open(void);
void port_close(void);
void port_cleanup(void);
static void _uvcb_poll_event(uv_poll_t* handle, int status, int events);



static const char *port_state_to_str(int state)
{
    switch (state) {
        default:
        case PORT_STATE_UNKNOWN:
            return "<unknown>";
        case PORT_STATE_WAITING:
            return "waiting";
        case PORT_STATE_BUSY:
            return "busy";
        case PORT_STATE_READY:
            return "ready";
    }
}

static int port_exists(void)
{
    // wont fly on windows but uv_fs_pool_t doesent either (I think) so need
    // something else anyway
    // TODO use "abspath"
    return (access(port_opts.name, F_OK) == 0);
}

static void on_port_discovered(int err)
{
    port_open();
}

/// assume port_opts.stay is true
static void _port_panic_recover(void)
{
    /* trying to recover from a error that mostly likley caused errno to be
     * set. clearing it just in case... */
    if (errno) {
        LOG_DBG("errno cleared from %s", strerrorname_np(errno));
        errno = 0;
    }

    /* if we get here, device probably still "exists" as this process holds a
     * open file descriptor to it . i.e. device not "gone" until after close.
     */
    port_close();
    port_wait_start(on_port_discovered);
    port_data.state = PORT_STATE_WAITING;
}

/// oh no! check port exists, if not wait for it if allowed, else die.
#define PORT_PANIC(EXIT_CODE, FMT, ...) do { \
    if (port_opts.stay) { \
        LOG_DBG(FMT, ##__VA_ARGS__); \
        _port_panic_recover(); \
    } \
    else { \
        SPCOM_EXIT(EXIT_CODE, FMT, ##__VA_ARGS__); \
    } \
} while (0)

static void _set_event_flags(int flags)
{
    // will this ever occur?
    flags |= UV_DISCONNECT;

    /* calling uv_poll_start() on active handle is ok. will update events mask */
    int err = uv_poll_start(&port_data.poll_handle, flags, _uvcb_poll_event);
    assert_uv_ok(err, "uv_poll_start");
}

static inline void _tx_start(void)
{
    // always intereseted in read event
    _set_event_flags(UV_READABLE | UV_WRITABLE);
}

static inline void _tx_stop(void)
{
    // always intereseted in read event but not writable
    _set_event_flags(UV_READABLE);
}

static void op_done(struct opq_item *op)
{
    opq_release_head(&opq_rt, op);
    port_data.offset = 0;
    port_data.current_op = NULL;
}

static void _on_sleep_done(uv_timer_t* handle)
{
    LOG_DBG("op d sleep done");
    assert(port_data.current_op);
    assert(port_data.current_op->op_code == OP_SLEEP);
    op_done(port_data.current_op);
}

static bool update_write(const void *buf, size_t bufsize)
{
    struct port_s *p = &port_data;

    if (!bufsize)
        return true;

    // TODO check nbytes = sp_output_waiting > N and drain!?

    assert(p->offset < bufsize);

    size_t remains = bufsize - p->offset;
    const char *src = ((const char *)buf) + p->offset;
    int rc;
    size_t size;
    uint64_t ts_now = 0;
    const int chardelay = port_opts.chardelay;

    if (chardelay) {
        ts_now = uv_now(uv_default_loop());
        uint64_t dt = ts_now - p->ts_lastc;

        if (dt < chardelay) {
            return false; // try on next loop iteration
        }

        size = 1;
    }
    else {
        size = remains;
    }

    rc = sp_nonblocking_write(p->port, src, size);

    if (rc < 0) {
        PORT_PANIC(EX_IOERR, "sp_nonblocking_write size=%zu, rc=%d", size, rc);
        return false; // should not get here
    }

    if (rc == 0) {
        // i.e. EAGAIN retry on next writable event
        LOG_DBG("sp_nonblocking_write rc=0 (EAGAIN?");
        return false;
    }

    if (chardelay) {
        p->ts_lastc = ts_now;
    }

    __LOG_TXRX("TX", src, size);

    if (rc < remains) {
        // incomplete write. try write remaining on next writable event
        //LOG_DBG("sp_nonblocking_write %d/%zu", rc, size);
        p->offset += rc;
        return false;
    }

    // success complete write
    return true; // done!
}
/** called before poll/select/whatever in event loop
 * check if there is read/write operations to be executed and
 * set "watchers" accordingly
 */
static void _on_prepare(uv_prepare_t* handle)
{
    if (port_data.current_op)
        return; // operation not done yet

    // TODO single q
    struct opq_item *op = opq_acquire_head(&opq_rt);
    if (!op) {
        _tx_stop();
        return;
    }

    // load/start operation prior sleep timer start
    port_data.current_op = op;
    // OP_EXIT is the only accepted op_code when port not ready
    if (op->op_code == OP_EXIT) {
        SPCOM_EXIT(EX_OK, "op exit");
        return;
    }

    if (port_data.state != PORT_STATE_READY) {
        LOG_ERR("%s not ready. state: %s",
                port_opts.name, port_state_to_str(port_data.state));

        // implicilty set port_data.current_op = NULL;
        op_done(op);
        return;
    }

    if (op->op_code == OP_SLEEP) {
        //if (port_data.sleep_active) {
        //uv_timer_get_due_in
        uint64_t ms = (uint64_t) op->u.val * 1000;
        int err = uv_timer_start(&port_data.t_sleep, _on_sleep_done, ms, 0);
        LOG_DBG("sleeping %d ms", (unsigned int) ms);
        assert_uv_ok(err, "uv_timer_start");
        return;
    }

    _tx_start(); // enable _on_writable()
}

static void _on_writable(uv_poll_t* handle)
{
    (void) handle;
    int err = 0;
    char tmpc;
    bool done = true;
    struct opq_item *op = port_data.current_op;
    if (!op)  {
        _tx_stop();
        return;
    }

    switch(op->op_code) {

        case OP_PORT_WRITE:
            done = update_write(op->u.data, op->size);
            break;

        case OP_PORT_PUTC:
            tmpc = op->u.val;
            done = update_write(&tmpc, 1);
            break;

        case OP_PORT_PUT_EOL:
            done = update_write(port_data.eol, port_data.eol_len);
            break;

        case OP_PORT_SET_RTS:
            err = sp_set_rts(port_data.port, op->u.val);
            if (err)
                LOG_SP_ERR(err, "sp_set_rts");
            done = true;
            break;

        case OP_PORT_SET_CTS:
            err = sp_set_cts(port_data.port, op->u.val);
            if (err)
                LOG_SP_ERR(err, "sp_set_cts");
            done = true;
            break;

        case OP_PORT_SET_DTR:
            err = sp_set_dtr(port_data.port, op->u.val);
            if (err)
                LOG_SP_ERR(err, "sp_set_dtr");
            done = true;
            break;

        case OP_PORT_SET_DSR:
            err = sp_set_dsr(port_data.port, op->u.val);
            if (err)
                LOG_SP_ERR(err, "sp_set_dsr");
            done = true;
            break;

        case OP_PORT_DRAIN:
            // TODO is this "safe"? see libserialport doc.
            err = sp_drain(port_data.port);
            if (err)
                LOG_SP_ERR(err, "sp_drain");
            done = true;
            break;

        case OP_PORT_FLUSH:
            // TODO always flush IO?
            err = sp_flush(port_data.port, SP_BUF_BOTH);
            if (err)
                LOG_SP_ERR(err, "sp_flush");
            done = true;
            break;

        case OP_SLEEP:
        case OP_EXIT:
            done = false;
            break;

        default:
            LOG_ERR("unknown op_code %d", op->op_code);
            done = true;
            break;
    }

    if (done) {
        op_done(op);
    }
}

static void _on_readable(uv_poll_t* handle)
{
    // TODO bufsize?
    static char buf[255];
    int rc = sp_nonblocking_read(port_data.port, buf, sizeof(buf));
    if (rc < 0) {
        PORT_PANIC(EX_IOERR, "sp_nonblocking_read rc=%d", rc);
        return;
    }
    if (rc == 0) {
        LOG_DBG("sp_nonblocking_read rc=0 (EAGAIN)");
        // Try again later
        return;
    }
    if (rc == sizeof(buf)) {
        // read more on next uv loop
        LOG_DBG("read=buf_size");
    }

    size_t size = rc;
    __LOG_TXRX("RX", buf, size);
    port_data.rx_cb(buf, size);
}

/*
 * note on `uv_poll`:
 """ It is possible that poll handles occasionally signal that a file descriptor is
readable or writable even when it isn’t. The user should therefore always be
prepared to handle EAGAIN or equivalent when it attempts to read from or write
to the fd."""

on unix, sp_nonblocking_{read, write} return 0 if read fails and EAGAIN is set
*/
static void _uvcb_poll_event(uv_poll_t* handle, int status, int events)
{
    // This will typicaly occur if user pulls the cabel for /dev/ttyUSB
    if (status == UV_EBADF) {
        PORT_PANIC(EX_OSFILE, "poll event EBADF - device disconnected!?");
        return;
    }
    else if (status) {
        LOG_WRN("unexpected uv poll status %d", status);
    }
    //LOG_DBG("port event. status=%d, event_flags=0x%x", status, events);
    if (events & UV_READABLE) {
        _on_readable(handle);
    }

    if (events & UV_WRITABLE) {
        _on_writable(handle);
    }

    if (events & UV_DISCONNECT) {
        // probaly never
        PORT_PANIC(EX_UNAVAILABLE, "UV_DISCONNECT");
    }
}

int port_set_config(void)
{
    // TODO is user change config in cmd mode it should also be stored for reopen
#define CONFIG_ERROR(ERR, WHY) (LOG_SP_ERR(ERR, WHY), ERR)
    struct sp_port *p = port_data.port;
    struct port_opts_s *o = &port_opts;
    if (!p)
        return -1;
    int err;

    if (o->baudrate >= 0) {
        err = sp_set_baudrate(p, o->baudrate);
        if (err)
            return CONFIG_ERROR(err, "sp_set_baudrate");
    }

    if (o->databits >= 0) {
        err = sp_set_bits(p, o->databits);
        if (err)
            return CONFIG_ERROR(err, "sp_set_(data)bits");
    }

    if (o->parity >= 0) {
        err = sp_set_parity(p, o->parity);
        if (err)
            return CONFIG_ERROR(err, "sp_set_parity");
    }

    if (o->stopbits >= 0) {
        err = sp_set_stopbits(p, o->stopbits);
        if (err)
            return CONFIG_ERROR(err, "sp_set_stopbits");
    }

    if (o->flowcontrol >= 0) {

        int flowcontrol = FLOW_TO_SP_FLOWCONTROL(o->flowcontrol);
        err = sp_set_flowcontrol(p, flowcontrol);
        if (err)
            return CONFIG_ERROR(err, "sp_set_flowcontrol");

        // libserialport default when flow is XONOFF is txrx 
        if (flowcontrol == SP_FLOWCONTROL_XONXOFF) {
            int xonxoff = FLOW_TO_SP_XONXOFF(o->flowcontrol);
            if (xonxoff) {
                err = sp_set_xon_xoff(p, xonxoff);
                if (err)
                    return CONFIG_ERROR(err, "sp_set_xonxoff");
            }
        }
    }

    return 0;
}


static void port_open(void)
{
    /* any assert failure will trigger cleanup - no need to do it here */

    int err = 0;

    uv_loop_t *loop = uv_default_loop();
    // TODO when, if ever, should operation queue be cleares/reseted? not here!!!

    LOG_DBG("Opening port '%s'", port_opts.name);

    err = sp_get_port_by_name(port_opts.name, &port_data.port);
    assert_sp_ok(err, "sp_get_port_by_name");

    struct sp_port *p = port_data.port;
    err = sp_open(p, SP_MODE_READ_WRITE);
    assert_sp_ok(err, "sp_open");

    // get os defualts. must be _after_ open
    err = sp_get_config(p, port_data.org_config);
    assert_sp_ok(err, "sp_get_config");
    port_data.have_org_config = true;

    port_set_config();

    // get fd on unix a HANDLE on windows
    uv_os_fd_t fd = -1; // or uv_file ?
    err = sp_get_port_handle(p, &fd);
    assert_sp_ok(err, "sp_get_port_handle");

    // saftey check
    uv_handle_type htype = uv_guess_handle(fd);
    LOG_DBG("uv_handle_type='%s'=%d",
            misc_uv_handle_type_to_str(htype), (int) htype);

    err = uv_prepare_init(loop, &port_data.prepare_handle);
    assert_uv_ok(err, "uv_prepare_init");

    err = uv_prepare_start(&port_data.prepare_handle, _on_prepare);
    (void) err; // cant fail according to doc

    // use uv_poll_t as custom read write from libserialport
    err = uv_poll_init(loop, &port_data.poll_handle, fd);
    assert_uv_ok(err, "uv_poll_init");

    int events = UV_READABLE | UV_WRITABLE;
    err = uv_poll_start(&port_data.poll_handle, events, _uvcb_poll_event);
    assert_uv_ok(err, "uv_poll_start");

    port_data.state = PORT_STATE_READY;

}

void port_close(void)
{
    int err;

    if (!port_data.port) {
        return;
    }

    err = uv_timer_stop(&port_data.t_sleep);
    (void)err;
    //LOG_UV_ERR(err, "uv_poll_stop");

    err = uv_prepare_stop(&port_data.prepare_handle);
    (void) err; // cant fail

    if (uv_is_active((uv_handle_t *) &port_data.poll_handle)) {
        err = uv_poll_stop(&port_data.poll_handle);
        if (err)
            LOG_UV_ERR(err, "uv_poll_stop");
    }

    // TODO ensure error messages for "dumped" commands printed
    opq_release_all(&opq_rt);
    port_data.offset = 0;
    port_data.current_op = NULL;

    if (port_data.have_org_config) {
        assert(port_data.org_config);
        LOG_DBG("Restoring port settings");
        err = sp_set_config(port_data.port, port_data.org_config);
        // TODO if /dev/ttyUSB0 unplugged, print more usefull message
        //  can not restore. no error message?
        if (err) {
            LOG_SP_ERR(err, "sp_set_config");
        }
        port_data.have_org_config = false;
    }

    err = sp_close(port_data.port);
    if (err)
        LOG_SP_ERR(err, "sp_close");

    sp_free_port(port_data.port);
    port_data.port = NULL;

}

void port_cleanup(void)
{
    if (port_data.org_config) {
         sp_free_config(port_data.org_config);
        port_data.org_config = NULL;
    }

    port_wait_cleanup();
}

int port_write(const void *data, size_t size)
{
#if 1
    return -1;
#else
    /* TODO I think there is a good reason for not
     * calling sp_nonblocking_write() directly here but I forgot if or why */

    if (!port_data.port)
        return -1;

    if (port_data.state != PORT_STATE_READY) {
        LOG_ERR("%s not ready. state: %s",
                port_opts.name, port_state_to_str(port_data.state));

        return -1;
    }

    int remains = size;
    const char *p = data;
    while (remains > 0) {
        int rc = sp_nonblocking_write(port_data.port, p, remains);

        if (rc < 0) {
            LOG_ERR("sp_nb_write rc=%d", rc);
            port_panic(EX_IOERR, "write error");
            return -1;
        }
        if (rc == 0) {
             // i.e. EAGAIN retry on next writable event
             LOG_DBG("sp_nb_write rc=0 (EAGAIN?");
             continue;
        }
        if (rc < size) {
            LOG_DBG("sp_nb_write %d/%zu", rc, size);
            // incomplete write. try write remaining on next writable event
            p += rc;
            remains -= rc;
        }
        assert(rc == size);
        break;
    }

    return 0;
#endif
}

int port_putc(int c)
{
#if 1
    if (port_data.state != PORT_STATE_READY) {
        LOG_ERR("%s not ready. state: %s",
                port_opts.name, port_state_to_str(port_data.state));

        return -1;
    }
    return opq_enqueue_val(&opq_rt, OP_PORT_PUTC, c);
#else
    unsigned char b = c;
    return port_write(&b, 1);
#endif
}

int port_write_line(const char *line)
{
    return -1; // TODO
}

int port_drain(uint32_t timeout_ms)
{
    /* Quote from libsp doc:
     * "If your program runs on Unix, defines its own signal handlers, and
     * needs to abort draining the output buffer when when these are called,
     * then you should not use this function. It repeats system calls that
     * return with EINTR. To be able to abort a drain from a signal handler,
     * you would need to implement your own blocking drain by polling the
     * result of sp_output_waiting()." */
#if 0 //TODO
    while(1) {
        int rc = sp_output_waiting(port_data.port);
        if (rc == 0) {
            break;
        }
        if (rc < 0) {
            LOG_SP_ERR(rc, "sp_output_waiting");
            return rc;
        }
        sleep_ms(1);
    }

    return 0;
#else
    return -1;
#endif
}


void port_init(port_rx_cb_fn *rx_cb)
{
    int err;

    if (!port_opts.name) {
        SPCOM_EXIT(EX_USAGE, "No port or device name provided");
    }

    port_data.eol_len = eol_seq_cpy(eol_tx,
                                    port_data.eol,
                                    sizeof(port_data.eol));

    port_data.rx_cb = rx_cb;
    // allocate some resources
    err = sp_new_config(&port_data.org_config);
    assert_sp_ok(err, "sp_new_config");
    assert(port_data.org_config);

    uv_loop_t *loop = uv_default_loop();

    err = uv_timer_init(loop, &port_data.t_sleep);
    assert_uv_ok(err, "uv_timer_init");

    port_data.ts_lastc = uv_now(loop);

    if (port_opts.wait) {
        port_wait_init(port_opts.name);
    }

    if (port_exists()) {
        port_open();
    }
    else {
        if (port_opts.wait) {
            port_wait_start(on_port_discovered);
            port_data.state = PORT_STATE_WAITING;
        }
        else {
            SPCOM_EXIT(EX_USAGE, "No such device '%s'", port_opts.name);
        }
    }
}
