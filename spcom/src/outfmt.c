#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include "opt.h"
#include "vt_defs.h"
#include "log.h"
#include "eol.h"
#include "shell.h"
#include "charmap.h"
#include "outfmt.h"

static struct outfmt_s 
{
    // end-of-line sequence
    bool had_eol;

    int prev_c;
    char last_c;
} outfmt = {
    .prev_c = -1,
};

static struct outfmt_opts_s {
    bool color;
    struct {
        const char *hexesc;
    } colors;
    int timestamp;
} outfmt_opts = {
    .color = true,
    .colors = {
        .hexesc = VT_COLOR_RED
    },
};

/**
 * premature optimization buffer (mabye) -
 * if shell is async/sticky we need to copy the readline state for
 * every write to stdout. also fputc is slow...
 * must ensure a flush after strbuf operation(s).
 */
static struct strbuf_s {
    char data[128];
    size_t len;
    char last_c_flushed;
} _strbuf;

static void strbuf_flush(void)
{
    struct strbuf_s *sb = &_strbuf;

    if (!sb->len)
        return;

    const void *lock = shell_output_lock();

    size_t rc = fwrite(sb->data, sb->len, 1, stdout);

    shell_output_unlock(lock);


    if (rc == 0) {
        // TODO log error. retry?
    }
    else if (rc != sb->len) {
        // TODO log error. retry?
    }

    sb->last_c_flushed = sb->data[sb->len];

    sb->len = 0;
}

static inline void strbuf_putc(char c)
{
    struct strbuf_s *sb = &_strbuf;
    size_t remains = sizeof(sb->data) - sb->len;
    if (remains < 1)
        strbuf_flush();

    sb->data[sb->len] = c;
    sb->len++;
}

static void strbuf_write(const char *src, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        strbuf_putc(*src++);
    }
}

static void strbuf_puts(const char *s)
{
    while (*s) {
        strbuf_putc(*s++);
    }
}

/* or use ts from moreutils? `apt install moreutils`
 *
 *  comand | ts '[%Y-%m-%d %H:%M:%S]'
 */
static void print_timestamp(void)
{
    int err;
    /* gettimeofday marked obsolete in POSIX.1-2008 and recommends
     * ... or use uv_gettimeofday?
     */
    if (!outfmt_opts.timestamp)
        return;
    struct timespec now;
#if 0 // TODO use uv_gettimeofday - more portable
    uv_timeval64_t uvtv;
    err = uv_gettimeofday(&uvtv);
    if (err) {
        LOG_ERR("uv_gettimeofday err=%d", err);
        return;
    }
    now.tv_sec = uvtv.tv_sec;
    now.tv_nsec = uvtv.tv_usec * 1000;

#else
    err = clock_gettime(CLOCK_REALTIME, &now);
    if (err) {
        // should not occur. TODO check errno?
        return;
    }
#endif
    struct tm tm;
    gmtime_r(&now.tv_sec, &tm);
    static char buf[32];

    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    strbuf_puts(buf);

    // do not have nanosecond precision 
    sprintf(buf, ".%03luZ: ", now.tv_nsec / 1000000);

    strbuf_puts(buf);
}

static void outfmt_putc(int c)
{
    if (!charmap_rx) {
        strbuf_putc(c);
    }

    char buf[CHARMAP_REPR_BUF_SIZE] = { 0 };
    char *color = NULL;
    int rc = charmap_remap(charmap_rx, c, buf, &color);
    if (rc < 0) // drop
        return;

    if (rc == 0) // not remapped
        strbuf_putc(c);

    if (rc == 1) // remapped to size == 1
        strbuf_putc(buf[0]);

    // remapped to size == rc.
    strbuf_write(buf, rc);
}

static inline void outfmt_nocheck_putc(int c)
{
    strbuf_putc(c);
}
/**
 * handle seq crlf:
 * -always replace with \n on stdout
 */
void outfmt_write(const void *data, size_t size)
{
    if (!size)
        return;
    const char *p = data;
    int prev_c = outfmt.prev_c;

    bool is_first_c = prev_c < 0;
    if (is_first_c)
        print_timestamp();

    bool had_eol = outfmt.had_eol;
    char cfwd[2];

    for (size_t i = 0; i < size; i++) {
        int c = *p++;
        // timestamp on first char received _after_ eol
        if (had_eol) {
            print_timestamp();
            had_eol = false;
        }

        int ec = eol_eval(eol_rx, c, cfwd);
        switch (ec) {

            case EOL_C_NONE:
                outfmt_putc(c);
                break;

            case EOL_C_CONSUMED:
                break;

            case EOL_C_FOUND:
                had_eol = true;
                outfmt_nocheck_putc('\n');
                break;

            case EOL_C_FWD1:
                outfmt_putc(cfwd[0]);
                break;

            case EOL_C_FWD2:
                outfmt_putc(cfwd[0]);
                outfmt_putc(cfwd[1]);
                break;

            default:
                LOG_ERR("never!");
                outfmt_putc(c);
                break;
        }
        prev_c = c;
    }
    strbuf_flush();

    outfmt.had_eol = had_eol;
    outfmt.prev_c = prev_c;
}

void outfmt_cleanup(void)
{
    const struct strbuf_s *sb = &_strbuf;

    if (sb->last_c_flushed != '\n') {
        // add newline to avoid text on same line as prompt after exit
        fputc('\n', stdout);
    }
}


static int parse_timestamp_format(const struct opt_conf *conf, char *sval)
{
    return -1;
}

static const struct opt_conf outfmt_opts_conf[] = {
    {
        .name = "timestamp",
        .dest = &outfmt_opts.timestamp,
        .parse = opt_ap_flag_true,
        .descr = "prepend timestamp on every line",
    },
#if 0 // TODO {long, short, +TZ, +ns, +ms }
    {
        .name = "iso8601-format",
        .dest = &outfmt_opts.timestamp,
        .parse = opt_ap_flag_true,
        .descr = "ISO 8601 format for timestamp",
    },
#endif
    {
        .name = "color",
        .dest = &outfmt_opts.color,
        .parse = opt_ap_flag_true,
        .descr = "enable color output",
    },
};

OPT_SECTION_ADD(outfmt, outfmt_opts_conf, ARRAY_LEN(outfmt_opts_conf), NULL);
