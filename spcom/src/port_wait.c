#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h> // AT_* defs
#include <unistd.h>
#include <libgen.h> // dirname, basename

#include <uv.h>

#include "log.h"
#include "assert.h"
#include "port_wait.h"


#ifndef CONFIG_PORT_WAIT_PERMISSION_TIMEOUT_MS
#define CONFIG_PORT_WAIT_PERMISSION_TIMEOUT_MS 5000
#endif

struct port_wait_s {
    bool initialized;
    port_wait_cb *cb;
    // buf_... needed as posix `dirname()` and `basename()` modifies input
    char *buf_dirname;
    char *buf_basename;
    char *abspath;
    char *dirname;
    char *basename;
    uv_fs_event_t fsevent_handle;
};

static struct port_wait_s port_wait;

/**
 * this timeout needed to avoid a "false" infinite wait when permission will
 * never be granted. Example when user not in group dialout.
 */
static uv_timer_t _permission_timer;

static void _permission_timeout_cb(uv_timer_t *handle)
{
    LOG_DBG("Timeout waiting for R/W access after %u ms",
            CONFIG_PORT_WAIT_PERMISSION_TIMEOUT_MS);

    SPCOM_EXIT(EX_NOPERM, "Missing serial port r/w permisson");
}

static void _permission_timeout_start_once(void)
{
    unsigned int msec = CONFIG_PORT_WAIT_PERMISSION_TIMEOUT_MS;

    if (!msec) {
        return; // disabled
    }

    if (uv_is_active((uv_handle_t *)&_permission_timer)) {
        return; // already started
    }

    int err = uv_timer_start(&_permission_timer,
                             _permission_timeout_cb,
                             msec,
                             0);

    assert_uv_ok(err, "uv_timer_start");

    LOG_DBG("permission timeout started, %u ms", msec);
}

static void _permission_timeout_stop(void)
{
    // should be safe to call stop regardless of timer is running or not
    int err = uv_timer_stop(&_permission_timer);
    if (err) {
        LOG_UV_ERR(err, "uv_timer_stop");
    }
}

static void _permission_timeout_init(void)
{
    int err = uv_timer_init(uv_default_loop(), &_permission_timer);
    assert_uv_ok(err, "uv_timer_init");
}

/**
 * after device is (re)connected, this callback will most probably be called
 * several times for the same `filename`:
 * - first when device fd is gone (after the old one closed)
 * - device fd is back (but not yet readable and/or writable)
 * - device is readable and/or writable
 */
static void _on_dir_entry_change(uv_fs_event_t *handle,
                                 const char *filename,
                                 int events,
                                 int status)
{
    struct port_wait_s *pw = &port_wait;
    // filename is NULL sometimes. excpected?
    if (!filename)
        return;

    if (strcmp(pw->basename, filename))
        return;

    LOG_DBG("%s events=0x%x, status=%d", filename, events, status);

    // the uv_fs_event options are limited. need to manually check r/w access

    if (access(pw->abspath, F_OK)) {
        //device disapeared. this is expected directly after close
        LOG_DBG("device fd gone");
        return;
    }

    _permission_timeout_start_once();

    if (access(pw->abspath, R_OK | W_OK)) {
        if (errno != EACCES) {
            // indicates bad path or other error
            LOG_WRN("unexpected errno value '%d'!=EACCES", errno);
        }
        LOG_DBG("'%s' have no r/w access yet", pw->abspath);
        return;
    }

    int err = uv_fs_event_stop(handle);
    if (err)
        LOG_UV_ERR(err, "uv_fs_event_stop");

    _permission_timeout_stop();

    assert(pw->cb);
    pw->cb(0);
}

void port_wait_start(port_wait_cb *cb)
{
    assert(cb);
    struct port_wait_s *pw = &port_wait;

    assert(pw->initialized);

    LOG_DBG("Watching directory '%s'", pw->dirname);
    pw->cb = cb;
    int err = uv_fs_event_start(&pw->fsevent_handle,
                                _on_dir_entry_change,
                                pw->dirname,
                                0); // UV_FS_EVENT_RECURSIVE);
    assert_uv_ok(err, "uv_fs_event_start");

}

void port_wait_stop(void)
{
    int err;
    struct port_wait_s *pw = &port_wait;

    _permission_timeout_stop();

    if (uv_is_active((uv_handle_t *)&pw->fsevent_handle)) {
        err = uv_fs_event_stop(&pw->fsevent_handle);
        if (err)
            LOG_UV_ERR(err, "uv_fs_event_stop");
    }
}

void port_wait_cleanup(void)
{
    struct port_wait_s *pw = &port_wait;

    if (!pw->initialized)
        return;

    port_wait_stop();

    if (pw->abspath) {
        free(pw->abspath);
        pw->abspath = NULL;
    }

    if (pw->buf_dirname) {
        free(pw->buf_dirname);
        pw->buf_dirname = NULL;
    }

    if (pw->buf_basename) {
        free(pw->buf_basename);
        pw->buf_basename = NULL;
    }

    pw->initialized = false;
}

int port_wait_init(const char *name)
{
    int err;
    struct port_wait_s *pw = &port_wait;
    uv_loop_t *loop = uv_default_loop();

    err = uv_fs_event_init(loop, &pw->fsevent_handle);
    assert_uv_ok(err, "uv_fs_event_init");

    _permission_timeout_init();

    /* posix versions of dirname() and basename() might return a modifed
     * version of its parameter.
     *
     * note: cant use realpath(name, NULL) on a path that doesnt exist yet
    */
    pw->abspath = strdup(name);
    assert(pw->abspath);

    pw->buf_dirname = strdup(pw->abspath);
    assert(pw->buf_dirname);

    pw->dirname = dirname(pw->buf_dirname);
    assert(pw->dirname);

    pw->buf_basename = strdup(pw->abspath);
    assert(pw->buf_basename);

    pw->basename = basename(pw->abspath);
    assert(pw->basename);


    pw->initialized = true;

    return 0;
}
