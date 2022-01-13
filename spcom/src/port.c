
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>
#include <libserialport.h>

#include <unistd.h> // access

#include "assert.h"
#include "opt.h"
#include "utils.h"
#include "eol.h"
#include "log.h"
#include "cmd.h"
#include "opq.h"
#include "str.h"
#include "port_wait.h"
#include "port.h"

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
extern struct port_opts_s port_opts;


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
} _port = {0};


static int port_open(void);
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
    LOG_DBG("wait done");
    port_open();
}

/// oh no! check port exists, if not wait for it if allowed, else die.
static void port_panic(const char *msg)
{

    if (!port_opts.stay) {
        // TODO user error
        LOG_ERR("port error - %s", msg);
        main_exit(EXIT_FAILURE);
        return;
    }
    /* if we get here, device probably still "exists" as this process holds a
     * open file descriptor to it . i.e. device not "gone" until after close.
     */ 
    LOG_DBG("port panic - %s", msg);

    /* trying to recover from a error that mostly likley caused errno to be
     * set. clearing it just in case... */
    if (errno) {
        errno = 0;
    }

    port_close();
    port_wait_start(on_port_discovered);
    _port.state = PORT_STATE_WAITING;
}

static void _set_event_flags(int flags)
{
    // will thix ever occur? 
    flags |= UV_DISCONNECT;

    /* calling uv_poll_start() on active handle is ok. will update events mask */
    int err = uv_poll_start(&_port.poll_handle, flags, _uvcb_poll_event); 
    assert_uv_z(err, "uv_poll_start");
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
    opq_free_head(&opq_rt, op);
    _port.offset = 0;
    _port.current_op = NULL;
}

static void _on_sleep_done(uv_timer_t* handle)
{
    LOG_DBG("op d sleep done");
    assert(_port.current_op);
    assert(_port.current_op->op_code == OP_SLEEP);
    op_done(_port.current_op);
}

static bool update_write(const void *buf, size_t bufsize)
{
    struct port_s *p = &_port;

    if (!bufsize)
        return true;

    // TODO check nbytes = sp_output_waiting > N and drain!?

    assert(p->offset < bufsize);
    size_t size = bufsize - p->offset;
    const char *src = ((const char *)buf) + p->offset;

    int rc;

    const int chardelay = port_opts.chardelay;
    if (chardelay) {
        uint64_t ts_now = uv_now(uv_default_loop());
        uint64_t dt = ts_now - p->ts_lastc;

        if (dt < chardelay)
            return false; // try on next loop iteration

        // done false below as rc not equal to size until last char
        rc = sp_nonblocking_write(p->port, src, 1);

        // no delay on EAGIAN rc == 0
        if (rc != 0)
            p->ts_lastc = ts_now;
    }
    else {
        rc = sp_nonblocking_write(p->port, src, size);
    }


    bool done;
    if (rc == size) {
        LOG_DBG("sp_nb_write %d/%zu", rc, size);
        done = true;
    }
    else if (rc == 0) {
        // i.e. EAGAIN retry on next writable event
        LOG_DBG("sp_nb_write rc=0 (EAGAIN?");
        done = false;
    }
    else if (rc < 0) {
        LOG_ERR("sp_nb_write size=%zu, rc=%d", size, rc);
        port_panic("write error");
        done = false; // should not get here
    }
    else {
        // incomplete write. try write remaining on next writable event
        LOG_DBG("sp_nb_write %d/%zu", rc, size);
        p->offset += rc;
        done = false;
    }

    return done;
}
/** called before poll/select/whatever in event loop
 * check if there is read/write operations to be executed and
 * set "watchers" accordingly
 */
static void _on_prepare(uv_prepare_t* handle)
{
    if (_port.current_op)
        return; // operation not done yet

    // TODO single q
    struct opq_item *op = opq_peek_head(&opq_rt);
    if (!op) {
        _tx_stop();
        return;
    }

    // load/start operation prior sleep timer start
    _port.current_op = op;
    // OP_EXIT is the only accepted op_code when port not ready
    if (op->op_code == OP_EXIT) {
        main_exit(EXIT_SUCCESS);
        return;
    }


    if (_port.state != PORT_STATE_READY) {
        LOG_ERR("%s not ready. state: %s",
                port_opts.name, port_state_to_str(_port.state));

        // implicilty set _port.current_op = NULL;
        op_done(op);
        return;
    }

    if (op->op_code == OP_SLEEP) {
        //if (_port.sleep_active) {
        //uv_timer_get_due_in
        uint64_t ms = (uint64_t) op->u.si_val * 1000;
        int err = uv_timer_start(&_port.t_sleep, _on_sleep_done, ms, 0);
        LOG_DBG("sleeping %d ms", (unsigned int) ms);
        assert_uv_z(err, "uv_timer_start");
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
    struct opq_item *op = _port.current_op;
    if (!op)  {
        _tx_stop();
        return;
    }

    switch(op->op_code) {

        case OP_PORT_WRITE:
            done = update_write(op->u.hdata, op->size);
            break;

        case OP_PORT_PUTC:
            tmpc = op->u.si_val;
            done = update_write(&tmpc, 1);
            break;

        case OP_PORT_PUT_EOL: {
            const struct eol_seq *es = eol_tx;
            done = update_write(es->seq, es->len);
            }
            break;

        case OP_PORT_SET_RTS:
            err = sp_set_rts(_port.port, op->u.si_val);
            if (err)
                LOG_SP_ERR(err, "sp_set_rts");
            done = true;
            break;

        case OP_PORT_SET_CTS:
            err = sp_set_cts(_port.port, op->u.si_val);
            if (err)
                LOG_SP_ERR(err, "sp_set_cts");
            done = true;
            break;

        case OP_PORT_SET_DTR:
            err = sp_set_dtr(_port.port, op->u.si_val);
            if (err)
                LOG_SP_ERR(err, "sp_set_dtr");
            done = true;
            break;

        case OP_PORT_SET_DSR:
            err = sp_set_dsr(_port.port, op->u.si_val);
            if (err)
                LOG_SP_ERR(err, "sp_set_dsr");
            done = true;
            break;

        case OP_PORT_DRAIN:
            // TODO is this "safe"? see libserialport doc.
            err = sp_drain(_port.port);
            if (err)
                LOG_SP_ERR(err, "sp_drain");
            done = true;
            break;

        case OP_PORT_FLUSH:
            // TODO always flush IO?
            err = sp_flush(_port.port, SP_BUF_BOTH);
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
    int rc = sp_nonblocking_read(_port.port, buf, sizeof(buf));
    if (rc < 0) {
        LOG_ERR("sp_nonblocking_read rc=%d", rc);
        port_panic("read error");
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
    LOG_DBG("read. %zu bytes", size);
    _port.rx_cb(buf, size);
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
        LOG_DBG("poll event EBADF - device disconnected!?");
        port_panic("EBADF");
        return;
    }
    else if (status) {
        LOG_ERR("unexpected uv poll status %d", status);
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
        port_panic("UV_DISCONNECT");
    }
}

int port_set_config(void)
{
    // TODO is user change config in cmd mode it should also be stored for reopen
#define CONFIG_ERROR(ERR, WHY) (LOG_SP_ERR(ERR, WHY), ERR)
    struct sp_port *p = _port.port;
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

static int port_open(void)
{
/* error macros should only be used from open/init as cleanup() called.
 * (could also used goto but less detailed error message. no line number for example)
 */
#define RET_SP_ERROR(ERR, WHY) (LOG_SP_ERR(ERR, WHY), port_cleanup(), ERR)
#define RET_UV_ERROR(ERR, WHY) (LOG_UV_ERR(ERR, WHY), port_cleanup(), ERR)
    int err = 0;
    const char *devname = port_opts.name;
    if (!devname) {
        LOG_ERR("No device name provided");
        return -1;
    }

    uv_loop_t *loop = uv_default_loop();
    opq_reset(&opq_rt);

    LOG_DBG("Opening port '%s'", devname);
    err = sp_get_port_by_name(devname, &_port.port);
    if (err)
        return RET_SP_ERROR(err, "sp_get_port_by_name");

    struct sp_port *p = _port.port;
    err = sp_open(p, SP_MODE_READ_WRITE);
    if (err)
        return RET_SP_ERROR(err, "sp_open");

    // get os defualts. must be _after_ open
    err = sp_get_config(p, _port.org_config);
    if (err)
        return RET_SP_ERROR(err, "sp_get_config");
    _port.have_org_config = true;

    port_set_config();

    // get fd on unix a HANDLE on windows
    uv_os_fd_t fd = -1; // or uv_file ?
    err = sp_get_port_handle(p, &fd);
    if (err)
        return RET_SP_ERROR(err, "sp_get_port_handle");

    // saftey check
    uv_handle_type htype = uv_guess_handle(fd);
    LOG_DBG("uv_handle_type='%s'=%d",
            log_uv_handle_type_to_str(htype), (int) htype);

    err = uv_prepare_init(loop, &_port.prepare_handle);
    if (err)
        return RET_UV_ERROR(err, "uv_prepare_init");

    err = uv_prepare_start(&_port.prepare_handle, _on_prepare);
    (void) err; // cant fail according to doc

    // use uv_poll_t as custom read write from libserialport
    err = uv_poll_init(loop, &_port.poll_handle, fd);
    if (err)
        return RET_UV_ERROR(err, "uv_poll_init");

    int events = UV_READABLE | UV_WRITABLE;
    err = uv_poll_start(&_port.poll_handle, events, _uvcb_poll_event);
    if (err)
        return RET_UV_ERROR(err, "uv_poll_start");

    _port.state = PORT_STATE_READY;

    return 0;
}

void port_close(void) 
{
    int err;

    struct sp_port *p = _port.port;
    if (!p) 
        return;

    err = uv_timer_stop(&_port.t_sleep);
    (void)err;
    //LOG_UV_ERR(err, "uv_poll_stop");

    err = uv_prepare_stop(&_port.prepare_handle);
    (void) err; // cant fail

    if (uv_is_active((uv_handle_t *) &_port.poll_handle)) {
        err = uv_poll_stop(&_port.poll_handle);
        if (err)
            LOG_UV_ERR(err, "uv_poll_stop");
    }

    // TODO ensure error messages for "dumped" commands printed
    opq_free_all(&opq_rt);
    _port.offset = 0;
    _port.current_op = NULL;

    if (_port.have_org_config) {
        assert(_port.org_config);
        LOG_DBG("Restoring port settings");
        err = sp_set_config(p, _port.org_config);
        // TODO if /dev/ttyUSB0 unplugged, print more usefull message
        //  can not restore. no error message?
        if (err)
            LOG_SP_ERR(err, "sp_set_config");
    }

    err = sp_close(p);
    if (err)
        LOG_SP_ERR(err, "sp_close");

    sp_free_port(p);
    _port.port = NULL;

}

void port_cleanup(void)
{
    if (_port.org_config) {
         sp_free_config(_port.org_config);
        _port.org_config = NULL;
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

    if (!_port.port)
        return -1;

    if (_port.state != PORT_STATE_READY) {
        LOG_ERR("%s not ready. state: %s",
                port_opts.name, port_state_to_str(_port.state));

        return -1;
    }

    int remains = size;
    const char *p = data;
    while (remains > 0) {
        int rc = sp_nonblocking_write(_port.port, p, remains);

        if (rc < 0) {
            LOG_ERR("sp_nb_write rc=%d", rc);
            port_panic("write error");
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
    if (_port.state != PORT_STATE_READY) {
        LOG_ERR("%s not ready. state: %s",
                port_opts.name, port_state_to_str(_port.state));

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
        int rc = sp_output_waiting(_port.port);
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


int port_init(port_rx_cb_fn *rx_cb)
{
    int err;
    if (!port_opts.name)
        return opt_error(NULL, "no serial port device");

    _port.rx_cb = rx_cb;
    // allocate some resources
    err = sp_new_config(&_port.org_config);
    assert_sp_z(err, "sp_new_config");
    assert(_port.org_config);

    uv_loop_t *loop = uv_default_loop();

    err = uv_timer_init(loop, &_port.t_sleep);
    assert_uv_z(err, "uv_timer_init");

    _port.ts_lastc = uv_now(loop);

    port_wait_init(port_opts.name);

    if (!port_exists()) {
        if (port_opts.wait || port_opts.stay) {
            port_wait_start(on_port_discovered);
            _port.state = PORT_STATE_WAITING;
            return 0;
        }
        else {
            LOG_ERR("No such device '%s'", port_opts.name);
            return -EBADF;
        }
    }

    return port_open();
}
