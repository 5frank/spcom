#include <errno.h>
#include <stdint.h>
#include <uv.h>

#include "assert.h"
#include "common.h"
#include "log.h"
#include "opt.h"
#include "timeout.h"

static uv_timer_t _timeout_timer;
static int _timeout_opt_sec = 0;

static void _uvcb_on_timeout(uv_timer_t *handle)
{
    /* There is no EX_TIMEOUT in <sysexits.h> but EX_TEMPFAIL documented as
     * "[...] indicating something that is not really an error." and seems like
     * the best fit
     */
    SPCOM_EXIT(EX_TEMPFAIL, "Application timeout after %d sec",
               _timeout_opt_sec);
}

void timeout_init(void)
{
    int err;

    if (_timeout_opt_sec <= 0)
        return;

    err = uv_timer_init(uv_default_loop(), &_timeout_timer);
    assert_uv_ok(err, "uv_timer_init");

    uint64_t ms = (uint64_t)_timeout_opt_sec * 1000;
    err = uv_timer_start(&_timeout_timer, _uvcb_on_timeout, ms, 0);
    assert_uv_ok(err, "uv_timer_start");

    LOG_DBG("timeout set to %d sec", _timeout_opt_sec);
}

void timeout_stop(void)
{
    if (!_timeout_opt_sec)
        return;

    int err = uv_timer_stop(&_timeout_timer);
    // assume this is at exit - only debug log error
    if (err)
        LOG_UV_DBG(err, "uv_timer_stop");
}

static const struct opt_conf timeout_opts_conf[] = {
    {
        .name = "timeout",
      //.alias = "exit-after",
      .dest = &_timeout_opt_sec,
      .parse = opt_parse_int,
      .descr = "application timeout in seconds. useful for batch jobs."
    },
};

OPT_SECTION_ADD(timeout, timeout_opts_conf, ARRAY_LEN(timeout_opts_conf), 0);
