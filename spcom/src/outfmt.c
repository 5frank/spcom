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
#include "strbuf.h"
#include "outfmt.h"
#include "assert.h"

static struct outfmt_s 
{
    // end-of-line sequence
    bool had_eol;

    int prev_c;
    char last_c;
    char last_c_flushed;
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

#if 0
static struct strbuf {
    char data[128];
    size_t len;
    char last_c_flushed;
} _strbuf;
#endif

static void outfmt_strbuf_flush(const struct strbuf *sb)
{
    const void *lock = shell_output_lock();

    size_t rc = fwrite(sb->buf, sb->len, 1, stdout);

    shell_output_unlock(lock);


    if (rc == 0) {
        // TODO log error. retry?
    }
    else if (rc != sb->len) {
        // TODO log error. retry?
    }

    outfmt.last_c_flushed = sb->buf[sb->len];
}

/**
 * premature optimization buffer (mabye) -
 * if shell is async/sticky we need to copy the readline state for
 * every write to stdout. also fputc is slow...
 * must ensure a flush after strbuf operation(s).
 */
STRBUF_STATIC_INIT(outfmt_strbuf, 1024, outfmt_strbuf_flush);

/* or use ts from moreutils? `apt install moreutils`
 *
 *  comand | ts '[%Y-%m-%d %H:%M:%S]'
 */
static void print_timestamp(struct strbuf *sb)
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

    //char buf[sizeof("19700101T010203Z.123456789") + 2];

    const size_t size_needed = sizeof("19700101T010203Z.123456789") + 2;

    size_t dst_size = strbuf_remains(sb);
    if (dst_size < size_needed) {
        strbuf_flush(sb);
        dst_size = strbuf_remains(sb);
        assert(dst_size >= size_needed);
    }
    char *dst = &sb->buf[sb->len];

    size_t rc = strftime(dst, dst_size, "%Y-%m-%dT%H:%M:%S", &tm);
    if (rc == 0) {
        // content of buf undefined
        *dst = '\0';
        return;
    }

    sb->len += rc;

    // do not have nanosecond precision
    strbuf_printf(sb, ".%03luZ: ", now.tv_nsec / 1000000);

}

static void outfmt_sb_putc(struct strbuf *sb, int c)
{
    if (!charmap_rx) {
        strbuf_putc(sb, c);
        return;
    }

    char buf[CHARMAP_REPR_BUF_SIZE] = { 0 };
    char *color = NULL;
    int rc = charmap_remap(charmap_rx, c, buf, &color);
    if (rc < 0) // drop
        return;

    if (rc == 0) {
        // not remapped
        strbuf_putc(sb, c);
    }
    else {
        // remapped to size 1 or more
        strbuf_write(sb, buf, rc);
    }
}

/**
 * handle seq crlf:
 * -always replace with \n on stdout
 */
void outfmt_write(const void *data, size_t size)
{
    struct strbuf *sb = &outfmt_strbuf;

    if (!size)
        return;

    const char *p = data;
    int prev_c = outfmt.prev_c;

    bool is_first_c = prev_c < 0;
    if (is_first_c)
        print_timestamp(sb);

    bool had_eol = outfmt.had_eol;
    char cfwd[2];

    for (size_t i = 0; i < size; i++) {
        int c = *p++;
        // timestamp on first char received _after_ eol
        if (had_eol) {
            print_timestamp(sb);
            had_eol = false;
        }

        int ec = eol_eval(eol_rx, c, cfwd);
        switch (ec) {

            case EOL_C_NONE:
                outfmt_sb_putc(sb, c);
                break;

            case EOL_C_CONSUMED:
                break;

            case EOL_C_FOUND:
                had_eol = true;
                /* outfmt putc no check, "raw" */

                strbuf_putc(sb, '\n');
                break;

            case EOL_C_FWD1:
                outfmt_sb_putc(sb, cfwd[0]);
                break;

            case EOL_C_FWD2:
                outfmt_sb_putc(sb, cfwd[0]);
                outfmt_sb_putc(sb, cfwd[1]);
                break;

            default:
                LOG_ERR("never!");
                outfmt_sb_putc(sb, c);
                break;
        }
        prev_c = c;
    }

    strbuf_flush(sb);

    outfmt.had_eol = had_eol;
    outfmt.prev_c = prev_c;
}

void outfmt_endline(void)
{
    // TODO if color turn it off
    struct strbuf *sb = &outfmt_strbuf;
    // flush in case data remains
    strbuf_flush(sb);

    if (outfmt.last_c_flushed != '\n') {
        // add newline to avoid text on same line as prompt after exit
        fputc('\n', stdout);
        outfmt.last_c_flushed = 0;
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
#if 0 // TODO options:{long, short, +TZ, +ns, +ms }?
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
