#include <stdbool.h>
#include <string.h>
#include <fcntl.h> // AT_* defs
#include <unistd.h>
#include <libgen.h> // dirname, basename

#include <uv.h>

#include "log.h"
#include "assert.h"
#include "port_wait.h"

struct port_wait_s {
    bool have_init;
    port_wait_cb *cb;
    // buf_... needed as posix `dirname()` and `basename()` modifies input
    char *buf_dirname;
    char *buf_basename;
    char *abspath;
    char *dirname;
    char *basename;
    uv_timer_t rw_timer;
    uv_fs_event_t fsevent_handle;
};

static struct port_wait_s port_wait;

static void _on_rw_access_timeout(uv_timer_t* handle)
{
    LOG_ERR("Timeout waiting for R/W access");
    // TODO?
}

/**
 * this timeout needed to avoid a "false" infinitive
 * wait when permission will never be granted. Example when user not in group
 * dialout.
 */
static void _start_rw_timeout(unsigned int ms)
{
    struct port_wait_s *pw = &port_wait;
    int err = uv_timer_start(&pw->rw_timer, _on_rw_access_timeout, ms, 0);
    assert_uv_z(err, "uv_timer_start");
}

/**
 * after device is (re)connected, this callback will most probably be called
 * several times for the same `filename`:
 * - first when device fd is gone (after the old one closed)
 * - device fd is back (but not yet readable and/or writable)
 * - device is readable and/or writable
 */
void _on_dir_entry_change(uv_fs_event_t* handle, const char* filename, int events, int status)
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
    // start rw timer here?

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

    LOG_DBG("wait done");

    assert(pw->cb);
    pw->cb(0);
}


#if 0
// after device close, need to wait on it to be removed
static int _wait_no_such_path(int timeout_sec)
{
    struct port_wait_s *pw = &port_wait;

    const int ms_inter = 10;
    struct timespec ts = {
        .tv_sec = 0,
        .tv_nsec = (long int) ms_inter * 1000000
    };

    int ms_timeout = timeout_sec * 1000;
    for (int n = 0; n <= ms_timeout; n += ms_inter) {

        if (access(pw->abspath, F_OK)) {
            // i.e. no such path 
            if (!errno) {
               LOG_WRN("access did not set errno?");
            }
            else if (errno != EACCES) {
                // indicates bad path or other error
                LOG_WRN("unexpected errno set by access '%d'!=EACCES", errno);
                return -errno;
            }
            return 0;
        } 

        int err = nanosleep(&ts, NULL);
        if (err) {
            // this should not occur in a single threaded application
            LOG_SYS_ERR(err, "nanosleep");
            return err;
        }
    }

    LOG_ERR("Timeout waiting for access(\"%s\", F_OK)!=0", pw->abspath);
    return -1;
}
#endif

void port_wait_start(port_wait_cb *cb)
{
    assert(cb);

    struct port_wait_s *pw = &port_wait;
    LOG_DBG("Watching directory '%s'", pw->dirname);
    pw->cb = cb;
    int err = uv_fs_event_start(&pw->fsevent_handle,
                            _on_dir_entry_change,
                            pw->dirname,
                            0); // UV_FS_EVENT_RECURSIVE);
    assert_uv_z(err, "uv_fs_event_start");

    LOG_INF("Waiting for %s ...", pw->abspath);
    //_port.state = PORT_STATE_WAITING;
}

void port_wait_stop(void)
{
    int err;
    struct port_wait_s *pw = &port_wait;

    if (uv_is_active((uv_handle_t *) &pw->fsevent_handle)) {
        err = uv_fs_event_stop(&pw->fsevent_handle);
        if (err)
            LOG_UV_ERR(err, "uv_fs_event_stop");
    }

    if (uv_is_active((uv_handle_t *) &pw->rw_timer)) {
        err = uv_timer_stop(&pw->rw_timer);
        if (err)
            LOG_UV_ERR(err, "uv_timer_stop");
    }
}

void port_wait_cleanup(void)
{
    struct port_wait_s *pw = &port_wait;

    if (!pw->have_init)
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

    pw->have_init = false;
}

int port_wait_init(const char *name)
{
    int err;
    struct port_wait_s *pw = &port_wait;
    uv_loop_t *loop = uv_default_loop();

    err = uv_fs_event_init(loop, &pw->fsevent_handle);
    assert_uv_z(err, "uv_fs_event_init");

    err = uv_timer_init(loop, &pw->rw_timer);
    assert_uv_z(err, "uv_timer_init");

    /* posix versions of dirname() and basename() might return a modifed
     * version of its parameter.  */
    // TODO cant use realpath on a path that doesnt exist yet
    //pw->realpath = realpath(name, NULL); // TODO free
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

    pw->have_init = true;

    return 0;
}
