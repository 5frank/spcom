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
#include "outfmt.h"


#define HEX_CHR_LUT_UPPER "0123456789ABCDEF"
#define HEX_CHR_LUT_LOWER "0123456789abcdef"

static struct outfmt_s 
{
    // end-of-line sequence
    bool had_eol;

    //enum eol_e eol_opt;
    // hex char lookup table
    const char *hexlut;
    int prev_c;
} outfmt = {
    .prev_c = -1,
    .hexlut = HEX_CHR_LUT_UPPER
};

static struct outfmt_opts_s {
    const char* hexfmt; // TODO
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
} _strbuf;

static void strbuf_flush(void)
{
    struct strbuf_s *buf = &_strbuf;

    if (!buf->len)
        return;

    struct shell_rls_s *rls = shell_rls_save();

    size_t rc = fwrite(buf->data, buf->len, 1, stdout);

    shell_rls_restore(rls);

    if (rc == 0) {
        // TODO log error. retry?
    }
    else if (rc != buf->len) {

        // TODO log error. retry?
    }
    buf->len = 0;
}

static inline void strbuf_putc(char c)
{
    struct strbuf_s *buf = &_strbuf;
    size_t remains = sizeof(buf->data) - buf->len;
    if (remains < 1) 
        strbuf_flush();

    buf->data[buf->len] = c;
    buf->len++;
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

static void print_hexesc(char c)
{
    // TODO check lots of optins here

    char buf[5];
    char *p = buf;
    // hex escape format, have color...
    const char *lut = outfmt.hexlut;
    unsigned char b = c;

#if 1
    *p++ = '\\';
    *p++ = 'x';
    *p++ = lut[b >> 4];
    *p++ = lut[b & 0x0f];
    *p = '\0';
#else
    *p++ = '[';
    *p++ = lut[b >> 4];
    *p++ = lut[b & 0x0f];
    *p++ = ']';
    *p = '\0';
#endif
    const char *color = outfmt_opts.colors.hexesc;
    if (color)
        strbuf_puts(color);

    strbuf_puts(buf);

    if (color) 
        strbuf_puts(VT_COLOR_OFF);
}
/* or use ts from moreutils? `apt install moreutils`
 * 
 *  comand | ts '[%Y-%m-%d %H:%M:%S]'
 */
static void print_timestamp(void)
{
    /* gettimeofday marked obsolete in POSIX.1-2008 and recommends
     * clock_gettime.  clock_gettime in C11 In C11 way use timespec_get(&tms,
     * TIME_UTC)) 
     *
     * ... or use uv_gettimeofday?
     */
    if (!outfmt_opts.timestamp)
        return;

    struct timespec now;
    int err = clock_gettime(CLOCK_REALTIME, &now);
    if (err) {
        // should not occur. TODO check errno?
        return;
    }
    struct tm tm;
    gmtime_r(&now.tv_sec, &tm);
    static char buf[32];

    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S.", &tm);
    strbuf_puts(buf);

    sprintf(buf, "%09luZ:", now.tv_nsec);
    strbuf_puts(buf);
}

static void strbuf_esc_putc(int c)
{
    if (isprint(c)) {
        strbuf_putc(c);
    }
    else if (1) { // TODO outfmt.opts.escape map..
        print_hexesc(c);
    }
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
    bool had_eol = outfmt.had_eol;
    char cfwd[2];

    for (size_t i = 0; i < size; i++) {
        int c = *p++;
        // timestamp on first char received _after_ eol
        if (is_first_c || had_eol) {
            print_timestamp();
        }

        int ec = eol_eval(eol_rx, c, cfwd);
        switch (ec) {

            case EOL_C_NONE:
                strbuf_esc_putc(c);
                break;

            case EOL_C_CONSUMED:
                break;

            case EOL_C_FOUND:
                strbuf_putc('\n');
                break;

            case EOL_C_FWD1:
                strbuf_esc_putc(cfwd[0]);
                break;

            case EOL_C_FWD2:
                strbuf_esc_putc(cfwd[0]);
                strbuf_esc_putc(cfwd[1]);
                break;

            default:
                LOG_ERR("never!");
                strbuf_esc_putc(c);
                break;
        }
#if 0
        had_eol = eol_rx_check(c);

        if (had_eol) {
            // TODO optionaly print escaped CR 
            strbuf_putc('\n');
        }
        else if (isprint(c)) {
            strbuf_putc(c);
        }
        else if (1) { // TODO outfmt.opts.escape map..
            print_hexesc(c);
        }
#endif
        prev_c = c;
    }
    strbuf_flush();

    outfmt.had_eol = had_eol;
    outfmt.prev_c = prev_c;
}

void out_drain_reset(void)
{}

static const struct opt_conf outfmt_opts_conf[] = {
    {
        .name = "timestamp",
        .dest = &outfmt_opts.timestamp,
        .parse = opt_ap_flag_true,
        .descr = "prepend timestamp on every line",
    },
    {
        .name = "color",
        .dest = &outfmt_opts.color,
        .parse = opt_ap_flag_true,
        .descr = "enable color output",
    },
};

OPT_SECTION_ADD(outfmt, outfmt_opts_conf, ARRAY_LEN(outfmt_opts_conf), NULL);
