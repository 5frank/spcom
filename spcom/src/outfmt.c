#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <stdio.h>

#include "opt.h"
#include "vt_defs.h"
#include "str.h"
#include "log.h"
#include "eol.h"
#include "shell.h"
#include "charmap.h"
#include "strbuf.h"
#include "outfmt.h"
#include "assert.h"

static struct outfmt_s {
    bool linebufed;
    // had end-of-line char or sequence
    bool had_eol;
    int prev_c;
    char last_c_flushed;
} outfmt_data = {
    .prev_c = -1,
};

static struct outfmt_opts_s {
    bool color;
    struct {
        const char *remapped;
    } colors;
    int timestamp;
} outfmt_opts = {
    .color = false,
    .colors = {
        .remapped = NULL,
    },
};

static void outfmt_strbuf_flush(struct strbuf *sb)
{
    shell_write(0, sb->buf, sb->len);

    outfmt_data.last_c_flushed = sb->buf[sb->len];

    sb->len = 0;
}

/**
 * premature optimization buffer (mabye) -
 * if shell is async/sticky we need to copy the readline state for
 * every write to stdout. also fputc is slow...
 * must ensure a flush after strbuf operation(s).
 */
STRBUF_STATIC_INIT(outfmt_strbuf, 1024, outfmt_strbuf_flush);

static void _eol_rx_timeout_init(void)
{
#if CONFIG_OPT_EOL_RX_TIMEOUT
    if (!opt_timeout_sec)
        return;

    uv_loop_t *loop = uv_default_loop();
    assert(loop);

    err = uv_timer_init(loop, &uvt_timeout);
    assert_uv_ok(err, "uv_timer_init");

    uint64_t ms = (uint64_t) opt_timeout_sec * 1000;
    err = uv_timer_start(&uvt_timeout, _uvcb_on_timeout, ms, 0);
    assert_uv_ok(err, "uv_timer_start");

    LOG_DBG("timeout set to %d sec", opt_timeout_sec);
#endif
}

/* or use ts from moreutils? `apt install moreutils`
 *
 *  comand | ts '[%Y-%m-%d %H:%M:%S]'
 */
static void _print_timestamp(struct strbuf *sb)
{
    if (!outfmt_opts.timestamp)
        return;

    const size_t size_needed = STR_ISO8601_SHORT_SIZE;

    size_t dst_size = strbuf_remains(sb);
    if (dst_size < size_needed) {
        outfmt_strbuf_flush(sb);
        dst_size = strbuf_remains(sb);
        assert(dst_size >= size_needed);
    }

    int rc = str_iso8601_short(&sb->buf[sb->len], dst_size);
    assert(rc > 0);
    assert(rc <= dst_size);

    sb->len += rc;
}

static void _sb_remap_putc(struct strbuf *sb, int c)
{
    int repr_type = charmap_repr_type(charmap_rx, c);

    if (repr_type == CHARMAP_REPR_NONE) {
        strbuf_putc(sb, c);
        return;
    }
    if (repr_type == CHARMAP_REPR_IGNORE) {
        return;
    }
    // TODO add colors here
    const char *color = outfmt_opts.colors.remapped;
    if (color) {
        strbuf_puts(sb, color);
    }

    char *buf = strbuf_endptr(sb, CHARMAP_REPR_BUF_SIZE);

    int rc = charmap_remap(charmap_rx, c, buf);
    assert(rc >= 0);
    // remapped to size 1 or more
    sb->len += rc;

    if (color) {
        strbuf_puts(sb, VT_COLOR_OFF);
    }
}

/**
 * handle seq crlf:
 */
void outfmt_write(const void *data, size_t size)
{
    if (!size)
        return;

    const char *src = data;
    struct strbuf *sb = &outfmt_strbuf;
    struct outfmt_s *ofd = &outfmt_data;


    bool is_first_c = ofd->prev_c < 0;
    if (is_first_c)
        _print_timestamp(sb);

    for (size_t i = 0; i < size; i++) {
        int c = *src++;
        // timestamp on first char received _after_ eol
        if (ofd->had_eol) {
            _print_timestamp(sb);
            ofd->had_eol = false;
        }

        int ec = eol_match(eol_rx, ofd->prev_c, c);
        switch (ec) {

            case EOL_C_NOMATCH:
                _sb_remap_putc(sb, c);
                break;

            case EOL_C_MATCH:
                ofd->had_eol = true;
                /* outfmt putc no check, "raw" */
                strbuf_putc(sb, '\n');
                if (ofd->linebufed) {
                    outfmt_strbuf_flush(sb);
                    // TODO _eol_rx_timeout_stop();
                }
                break;

            case EOL_C_POP:
                _sb_remap_putc(sb, ofd->prev_c);
                _sb_remap_putc(sb, c);
                break;

            case EOL_C_POP_AND_STASH:
                _sb_remap_putc(sb, ofd->prev_c);
                break;

            case EOL_C_IGNORE:
            case EOL_C_STASH:
                // ofd->prev_c = c below
                break;

            default:
                assert(0);
                break;
        }
        ofd->prev_c = c;
    }

    if (ofd->linebufed) {
        if (sb->len > 0) {
            // TODO _eol_rx_timeout_start();
        }
    }
    else {
        outfmt_strbuf_flush(sb);
    }
}

void outfmt_endline(void)
{
    // TODO if color turn it off
    struct strbuf *sb = &outfmt_strbuf;
    struct outfmt_s *ofd = &outfmt_data;
    // flush in case data remains
    outfmt_strbuf_flush(sb);
    if (ofd->last_c_flushed == '\0') {
        return;
    }
    if (ofd->last_c_flushed != '\n') {
        // add newline to avoid text on same line as prompt after exit
        fputc('\n', stdout);
        ofd->last_c_flushed = 0;
    }
}


static int parse_timestamp_format(const struct opt_conf *conf, char *sval)
{
    return -1;
}

static int outfmt_opts_post_parse(const struct opt_section_entry *entry)
{
    // note: do not use LOG here
#if 0
    charmap_print_map("tx", charmap_tx);
    charmap_print_map("rx", charmap_rx);
#endif
    if (outfmt_opts.color) {
        outfmt_opts.colors.remapped = VT_COLOR_RED;
    }

    return 0;
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
#if CONFIG_OPT_EOL_RX_TIMEOUT
    {
        .name  = "--eol-rx-timeout",
        .dest  = &eol_opts.eol_rx_timeout,
        .parse = opt_ap_int,
        .descr = "Float in seconds. "\
                 "If some data received but no eol received within given time, "\
                 "the buffered data is written output anywas."\
                 "Default: " STRINGIFY(EOL_RX_TIMEOUT_DEFAULT) ". "\
                 "Applies in coocked (line buffered) mode only otherwise ignored. "\

    },
#endif
};

OPT_SECTION_ADD(outfmt,
                outfmt_opts_conf,
                ARRAY_LEN(outfmt_opts_conf),
                outfmt_opts_post_parse);

