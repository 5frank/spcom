#include <stdint.h>
#include <uv.h>
#include <errno.h>

#include "log.h"
#include "common.h"
#include "assert.h"
#include "opt.h"
#include "timeout.h"

static uv_timer_t uvt_timeout;
static int opt_timeout_sec = 0;

static void _uvcb_on_timeout(uv_timer_t* handle)
{
    /* There is no EX_TIMEOUT in <sysexits.h> but EX_TEMPFAIL documented as
     * "[...] indicating something that is not really an error." and seems like
     * the best fit 
     */
    SPCOM_EXIT(EX_TEMPFAIL, "Application timeout after %d sec", opt_timeout_sec);
}

void timeout_init(void)
{
    int err;

    if (!opt_timeout_sec)
        return;

    uv_loop_t *loop = uv_default_loop();
    assert(loop);

    err = uv_timer_init(loop, &uvt_timeout);
    assert_uv_z(err, "uv_timer_init");

    uint64_t ms = (uint64_t) opt_timeout_sec * 1000;
    err = uv_timer_start(&uvt_timeout, _uvcb_on_timeout, ms, 0);
    assert_uv_z(err, "uv_timer_start");

    LOG_DBG("timeout set to %d sec", opt_timeout_sec);
}

void timeout_stop(void)
{
    if (!opt_timeout_sec)
        return;

    int err = uv_timer_stop(&uvt_timeout);
    // assume this is at exit - only debug log error
    if (err)
        LOG_DBG("uv_timer_stop err=%d", err);
}

static const struct opt_conf timeout_opts_conf[] = {
    {
        .name = "timeout",
        //.alias = "exit-after",
        .dest = &opt_timeout_sec,
        .parse = opt_ap_int,
        .descr = "application timeout in seconds. useful for batch jobs."
    },
};

OPT_SECTION_ADD(timeout, timeout_opts_conf, ARRAY_LEN(timeout_opts_conf), 0);

