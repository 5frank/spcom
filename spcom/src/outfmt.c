#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <stdio.h>
#include <time.h>

#include "vt_defs.h"
#include "log.h"
#include "shell.h"
#include "outfmt.h"


#define HEX_CHR_LUT_UPPER "0123456789ABCDEF"
#define HEX_CHR_LUT_LOWER "0123456789abcdef"

static struct outfmt_s 
{
    // end-of-line sequence
    struct eolseq_s {
        int matchcount;
        int seqlen;
        char seq[4];
    } eolseq;
    bool had_eol;

    // hex char lookup table
    const char *hexlut;
    int prev_c;
} _outfmt = {
    .prev_c = -1,
    .eolseq = {
        .seqlen = 1,
        .seq = {'\n', '\0'},
    },
    .hexlut = HEX_CHR_LUT_UPPER
};

struct outfmt_opts_s outfmt_opts = {
    .color = true,
    .colors = {
        .hexesc = VT_COLOR_RED
    },
};

/** 
 * premature optimization buffer (mabye) -
 * if shell is async/sticky we need to copy the readline state for
 * every write to stdout. also fputc is slow... 
 * must ensure a flush after tmpbuf operation(s).
 */ 
static struct tmpbuf_s {
    char data[64];
    size_t len;
} _tmpbuf;

static void tmpbuf_flush(void)
{
    struct tmpbuf_s *buf = &_tmpbuf;

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

static inline void tmpbuf_putc(char c)
{
    struct tmpbuf_s *buf = &_tmpbuf;
    size_t remains = sizeof(buf->data) - buf->len;
    if (remains < 1) 
        tmpbuf_flush();

    buf->data[buf->len] = c;
    buf->len++;
}

static void tmpbuf_write(const char *src, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        tmpbuf_putc(*src++);
    }
}

static void tmpbuf_puts(const char *s)
{
    while (*s) {
        tmpbuf_putc(*s++);
    }
}

/**
 * find end-of-line charachter sequence. 
 * @return numbers of EOL chars matched before a non-match, or a comlpete match occur. 
 * otherwise returns zero. i.e. if beginning of EOL sequence detected or nonmatch/
 */ 
static int eolseq_matches(struct eolseq_s *es, char c)
{
    int ret = 0;
    if (es->seq[es->matchcount] == c) {
        // have a match
        es->matchcount++;

        // a complete match. reset and return num matches
        if (es->matchcount >= es->seqlen) {
            ret = es->matchcount;
            es->matchcount = 0;
        }
    }
    else {
        // no match, reset matchcount but return number of matches "consumed" until this point
        ret = es->matchcount;
        es->matchcount = 0;
    }

    return ret;
}

static void print_hexesc(char c)
{
    // TODO check lots of optins here

    char buf[5];
    char *p = buf;
    // hex escape format, have color...
    const char *lut = _outfmt.hexlut;
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
        tmpbuf_puts(color);

    tmpbuf_puts(buf);

    if (color) 
        tmpbuf_puts(VT_COLOR_OFF);
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
    tmpbuf_puts(buf);

    sprintf(buf, "%09luZ:", now.tv_nsec);
    tmpbuf_puts(buf);
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
    int prev_c = _outfmt.prev_c;
    bool is_first_c = prev_c < 0;

    bool had_eol = _outfmt.had_eol;

    struct eolseq_s *eolseq = &_outfmt.eolseq;
    for (size_t i = 0; i < size; i++) {
        int c = *p++;
        // have first char after eol
        if (had_eol || is_first_c) {
            print_timestamp();
            had_eol = false;
        }

        // eol seq can be set to any sequence. check that first
        int n = eolseq_matches(eolseq, c);
        // n==0 c consumed by eol sequence, might be printed later on mismatch
        if (n > 0) {

            if (n >= eolseq->seqlen) {
                had_eol = true;
                tmpbuf_putc('\n'); // TODO
            }
            else if (n > 0) {
                // this was not a eol 
                tmpbuf_write(eolseq->seq, n);
            }
            continue;
        }

        if (isprint(c)) {
            tmpbuf_putc(c);
        }
        else if (1) { // TODO outfmt.opts.escape map..
            print_hexesc(c);
        }
    }
    tmpbuf_flush();

    _outfmt.had_eol = had_eol;
    _outfmt.prev_c = prev_c;
}

void out_drain_reset(void)
{}
