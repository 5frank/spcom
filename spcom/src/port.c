
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <uv.h>
#include <libserialport.h>

#include "assert.h"
#include "opts.h"
#include "utils.h"
#include "log.h"
#include "outfmt.h"
#include "cmd.h"
#include "opq.h"
#include "str.h"
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


struct port_opts_s port_opts = {
    /* assume negative values invalid. -1 used to check if options provided and 
     should be set or untouched. */
    .baudrate = -1,
    .databits = -1,
    .stopbits = -1,
    .parity = -1,
    .rts = -1,
    .cts = -1,
    .dtr = -1,
    .dsr = -1,
    //.xonxoff = -1,
    .flowcontrol = -1,
    .signal = -1
};

static struct port_s {
    struct sp_port *port;
    struct sp_port_config *org_config;
    struct sp_port_config *usr_config;
    uv_poll_t poll_handle;
    uv_prepare_t prepare_handle;
    uv_timer_t t_sleep;
    size_t offset;
    struct opq_item *current_op;
} _port = {0};

void port_cleanup(void);
static void _uvcb_poll_event(uv_poll_t* handle, int status, int events);


int port_wait_until_exists(const char *name, unsigned int timeout_sec) 
{ 
    // TODO use uv_fs_event_t or uv_fs_poll_t
    return 0;
}

static void _set_event_flags(int flags)
{
    // TODO will ever occur? 
    // flags |= UV_DISCONNECT;

    /* calling uv_poll_start() on active handle is ok. will update events mask */
    int err = uv_poll_start(&_port.poll_handle, flags, _uvcb_poll_event); 
    if (err)
        LOG_UV_ERR(err, "uv_poll_start");
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
    opq_free_head(&opq_rt, op); // TODO single queue
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
#if 0
static void op_write(struct opq_item *op)
{
    struct op_data *txb = &op->buf;
    size_t offset = _port.offset;
    // TODO check nbytes = sp_output_waiting > N and drain!?
    assert(txb->size > offset);
    const char *data = &txb->data[offset];
    size_t size = txb->size - offset;
    
    // TODO implement option --char-delay. write one char, set timer then return!?
    int rc = sp_nonblocking_write(_port.port, data, size);

    if (rc < 0) { 
         LOG_ERR("sp_nb_write rc=%d", rc);
         return; // TODO retry?
    }
    if (rc == 0) { 
         // i.e. EAGAIN retry on next writable event
         LOG_DBG("sp_nb_write rc=0 (EAGAIN?");
         return; 
    }
    if (rc < size) {
        LOG_DBG("sp_nb_write %d/%zu", rc, size);
        // incomplete write. try write remaining on next writable event
        offset += rc;
        return;
    }
    // TODO flush?
    assert(rc == size); // never!?
    // done with buf. free and pop/dequeu it
    op_done(op);

}

static void op_putc(struct opq_item *op)
{
    char c = op->val;
    const size_t size = 1;
    // TODO implement option --char-delay. write one char, set timer then return!?
    int rc = sp_nonblocking_write(_port.port, &c, size);

    if (rc == size) { 
        op_done(op);
        return; // TODO retry?
    }
    else if (rc == 0) { 
         // i.e. EAGAIN retry on next writable event
         LOG_DBG("sp_nb_write rc=0 (EAGAIN?");
         return; 
    } 
    else {
        LOG_ERR("sp_nb_write size=%zu, rc=%d", size, rc);
        // incomplete write. try write remaining on next writable event
        return;
    }
}
#endif
static bool update_write(const void *data, size_t size)
{
    
    size_t offset = _port.offset;
    // TODO check nbytes = sp_output_waiting > N and drain!?
    assert(size > offset);
    size = size - offset;

    const char *src = data;
    src += offset;

    // TODO implement option --char-delay. write one char, set timer then return!?
    int rc = sp_nonblocking_write(_port.port, data, size);

    if (rc == size) { 
        LOG_DBG("sp_nb_write %d/%zu", rc, size);
        return true;
    }
    else if (rc == 0) { 
         // i.e. EAGAIN retry on next writable event
         LOG_DBG("sp_nb_write rc=0 (EAGAIN?");
    } 
    else if (rc < 0) {
        LOG_ERR("sp_nb_write size=%zu, rc=%d", size, rc);
        // TODO panic!?
    }
    else {
        // incomplete write. try write remaining on next writable event
        LOG_DBG("sp_nb_write %d/%zu", rc, size);
        offset += rc;
    }
    return false; // not done
}
/** called before poll/select/whatever in event loop
 * check if there is read/write operations to be executed and
 * set "watchers" accordingly
 */ 
static void _on_prepare(uv_prepare_t* handle)
{
#if 1
    if (_port.current_op)
        return; // operation not done yet

    // TODO single q
    struct opq_item *op = opq_peek_head(&opq_rt);
    if (!op) {
        _tx_stop();
    }
    else {
        _port.offset = 0;
        _port.current_op = op;
        _tx_start();
    }
#endif
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
            done = update_write(op->buf.data, op->buf.size);
            break;

        case OP_PORT_PUTC:
            tmpc = op->val;
            done = update_write(&tmpc, 1);
            break;

        case OP_PORT_PUT_EOL:
            tmpc = '\n'; // TODO from config
            done = update_write(&tmpc, 1);
            break;

        case OP_PORT_SET_RTS:
            err = sp_set_rts(_port.port, op->val);
            if (err)
                LOG_SP_ERR(err, "sp_set_rts");
            done = true;
            break;

        case OP_PORT_SET_CTS:
            err = sp_set_cts(_port.port, op->val);
            if (err)
                LOG_SP_ERR(err, "sp_set_cts");
            done = true;
            break;

        case OP_PORT_SET_DTR:
            err = sp_set_dtr(_port.port, op->val);
            if (err)
                LOG_SP_ERR(err, "sp_set_dtr");
            done = true;
            break;

        case OP_PORT_SET_DSR:
            err = sp_set_dsr(_port.port, op->val);
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

        case OP_SLEEP: {
            uint64_t ms = (uint64_t) op->val * 1000;
            err = uv_timer_start(&_port.t_sleep, _on_sleep_done, ms, 1);
            LOG_DBG("sleeping %d ms", (unsigned int) ms);
            if (err)
                LOG_UV_ERR(err, "uv_timer_start");
            done = false;
            }
            break;

        case OP_EXIT:
            main_exit(EXIT_SUCCESS);
            // should not get here
            done = true;
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
    static char buf[32];
    int rc = sp_nonblocking_read(_port.port, buf, sizeof(buf));
    if (rc < 0) { 
        LOG_ERR("sp_nonblocking_read rc=%d", rc);
        return;
    }
    if (rc == 0) {
        LOG_DBG("sp_nonblocking_read rc=0 (EAGAIN)");
        // Try again later
        return;
    }
    LOG_DBG("read. %d bytes", rc);
    outfmt_write(buf, rc);
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
        LOG_UV_ERR(status, "status");
        return; // TODO cleanup an mabye wait
    }
    else if (status) {
        LOG_UV_ERR(status, "unexpected status");
    }
    //LOG_DBG("port event. status=%d, event_flags=0x%x", status, events);
    if (events & UV_READABLE) {
        _on_readable(handle);
    }

    if (events & UV_WRITABLE) {
        _on_writable(handle);
    }

    if (events & UV_DISCONNECT) {
        LOG_WRN("UV_DISCONNECT on port handle?");
    }

    //UV_PRIORITIZED
}

int port_set_config(void)
{
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

int port_open(void)
{
/* error macros should only be used from open/init as cleanup() called.
 * (could also used goto but less detailed error message. no line number for example)
 */
#define RET_SP_ERROR(ERR, WHY) (LOG_SP_ERR(ERR, WHY), port_cleanup(), ERR)
#define RET_UV_ERROR(ERR, WHY) (LOG_UV_ERR(ERR, WHY), port_cleanup(), ERR)

    int err = 0;
    opq_reset(&opq_rt);

    LOG_DBG("Opening port '%s'", port_opts.name);
    err = sp_get_port_by_name(port_opts.name, &_port.port);
    if (err)
        return RET_SP_ERROR(err, "sp_get_port_by_name");

    struct sp_port *p = _port.port;
    err = sp_open(p, SP_MODE_READ_WRITE);
    if (err)
        return RET_SP_ERROR(err, "sp_open");

    // malloc
    assert(!_port.org_config);
    err = sp_new_config(&_port.org_config);
    assert(!err);
    assert(_port.org_config);

    // get os defualts. must be _after_ open
    err = sp_get_config(p, _port.org_config);
    if (err)
        return RET_SP_ERROR(err, "sp_get_config");

    port_set_config();

    // get fd on unix a HANDLE on windows
    uv_os_fd_t fd = -1; // or uv_file ?
    err = sp_get_port_handle(p, &fd);
    if (err)
        return RET_SP_ERROR(err, "sp_get_port_handle");

    // saftey check
    uv_handle_type htype = uv_guess_handle(fd);
    LOG_DBG("uv_handle_type='%s'=%d", log_uv_handle_type_to_str(htype), (int) htype);
    uv_loop_t *loop = uv_default_loop();

    err = uv_prepare_init(loop, &_port.prepare_handle);
    if (err)
        return RET_UV_ERROR(err, "uv_prepare_init");

    // TODO could have a separate init...
    err = uv_timer_init(loop, &_port.t_sleep);
    assert_uv_z(err, "uv_timer_init");

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

    return 0;

}

void port_cleanup(void) 
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

    err = uv_poll_stop(&_port.poll_handle);
    if (err)
        LOG_UV_ERR(err, "uv_poll_stop");

    opq_free_all(&opq_rt);

    if (_port.org_config) {
        LOG_DBG("Restoring port settings");
        err = sp_set_config(p, _port.org_config);
        // TODO if /dev/ttyUSB0 unplugged, print more usefull message
        //  can not restore. no error message?
        if (err)
            LOG_SP_ERR(err, "sp_set_config");

         sp_free_config(_port.org_config);
        _port.org_config = NULL;
    }

    err = sp_close(p);
    if (err)
        LOG_SP_ERR(err, "sp_close");

    sp_free_port(p);
    _port.port = NULL;

}

void port_close(void)
{
    port_cleanup();
}

int port_write(const void *data, size_t size) 
{
    if (!_port.port)
        return -1;
#if 0 
    // TODO use op queue
    int err = txqueue_push(&_txqueue, data, size);
    assert(!err);
    //start if not alreay started
    _tx_start();
    return 0;
#else
    int remains = size;
    const char *p = data;
    while (remains > 0) {
        int rc = sp_nonblocking_write(_port.port, p, remains);

        if (rc < 0) {
             LOG_ERR("sp_nb_write rc=%d", rc);
             return -1; // TODO retry?
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
    return size;
#endif
}

int port_putc(int c)
{
    unsigned char b = c;
    return port_write(&b, 1);
    // TODO use double buffer and put on quque
}


int port_write_line(const char *line)
{

}
