#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "assert.h"
#include "common.h"
#include "str.h"
#include "opt.h"
#include "eol.h"

#define EOL_RX_TIMEOUT_DEFAULT 1


static struct eol_opts {
    int eol_rx_timeout_ms;
    struct eol_seq eol_txrx;
    struct eol_seq eol_tx;
    struct eol_seq eol_rx;
} eol_opts = {
    .eol_rx_timeout_ms = 1000 * EOL_RX_TIMEOUT_DEFAULT
};

static int eol_match_a_ignore_b(const struct eol_seq *es, int prev_c, int c)
{
    (void) prev_c;

    if (c == es->seq[0])
        return EOL_C_MATCH;

    if (c == es->seq[1])
        return EOL_C_IGNORE;

    return EOL_C_NOMATCH;
}

static int eol_match_a(const struct eol_seq *es, int prev_c, int c)
{
    (void) prev_c;
    return (c == es->seq[0]) ? EOL_C_MATCH : EOL_C_NOMATCH;
}

static int eol_match_a_then_b(const struct eol_seq *es, int prev_c, int c)
{
    //char a = es->seq[0];
    //char b = es->seq[1];
    bool prev_matched = (prev_c == es->seq[0]);

    if (!prev_matched) {
        if (c == es->seq[0])
            return EOL_C_STASH;
        else
            return EOL_C_NOMATCH;
    }
    else {
        if (c == es->seq[1])
            return EOL_C_MATCH;
        else if (c == es->seq[0])
            return EOL_C_POP_AND_STASH;
        else
            return EOL_C_POP;
    }

}

static int eol_match_a_or_b(const struct eol_seq *es, int prev_c, int c)
{
    (void) prev_c;
    return (c == es->seq[0] || c == es->seq[1]) ? EOL_C_MATCH : EOL_C_NOMATCH;
}

int eol_seq_cpy(const struct eol_seq *es, char *dst, size_t size)
{
    assert(es->match_func);
    assert(size >= 3);

    int len = (es->match_func == eol_match_a_then_b) ? 2 : 1;
    *dst++ = es->seq[0];
    if (len > 1) {
        *dst++ = es->seq[0];
    }

    *dst = '\0';

    return len;
}

static struct eol_seq eol_tx_default = {
    .match_func = eol_match_a,
    .seq = { '\n' },
    .len = 1
};

struct eol_seq *eol_tx = &eol_tx_default;

static struct eol_seq eol_rx_default = {
    .match_func = eol_match_a_ignore_b,
    .seq = { '\n', '\r' },
    .len = 1
};

struct eol_seq *eol_rx = &eol_rx_default;

/// returns NULL on error
static const char *eol_opt_parse_next(const char *s, unsigned char *byte)
{
    if (!s || strlen(s) < 2) {
        return NULL;
    }

    if (!strcasecmp(s, "LF")) {
        *byte = '\n';
        return s + 2;
    }
    if (!strcasecmp(s, "CR")) {
        *byte = '\r';
        return s + 2;
    }

    if (str_startswith(s, "0x")) {
        s += 2; // jump "0X"
        const char *ep = NULL;
        uint8_t tmp = 0;
        if (str_hextou8(s, &tmp, &ep)) {
            return NULL;
        }
        *byte = tmp;
        return ep;
    }

    return NULL;
}

static int eol_opt_parse_str(const char *s, struct eol_seq *es)
{
    s = eol_opt_parse_next(s, &es->seq[0]);
    if (!s) {
        return -EINVAL;
    }

    switch (*s) {
        case '\0':
            es->match_func = eol_match_a;
            return 0;

        case ',':
            es->match_func = eol_match_a_then_b;
            s++; // jump delmiter
            break;

        case '|':
            es->match_func = eol_match_a_or_b;
            s++; // jump delmiter
            break;

        // "crlf", or "0x130x10" have no delimiter
        default:
            es->match_func = eol_match_a_then_b;
    }

    s = eol_opt_parse_next(s, &es->seq[1]);
    if (!s) {
        return -EINVAL;
    }

    if (*s != '\0') {
        return -EINVAL; // trailing characters
    }

    return 0;
}

static int eol_opt_post_parse(const struct opt_section_entry *entry)
{
    // note: do not use LOG here
    struct eol_seq *txrx = &eol_opts.eol_txrx; //.
    if (txrx->match_func) {
        eol_tx = txrx;
        eol_rx = txrx;
    }
    struct eol_seq *tx = &eol_opts.eol_tx;
    if (tx->match_func) {
        eol_tx = tx;
    }

    struct eol_seq *rx = &eol_opts.eol_rx;
    if (rx->match_func) {
        eol_rx = rx;
    }

    return 0;
}

static int eol_opt_parse_txrx(const struct opt_conf *conf, char *s)
{
    return eol_opt_parse_str(s, &eol_opts.eol_txrx);
}

static int eol_opt_parse_tx(const struct opt_conf *conf, char *s)
{
    return eol_opt_parse_str(s, &eol_opts.eol_tx);
}

static int eol_opt_parse_rx(const struct opt_conf *conf, char *s)
{
    return eol_opt_parse_str(s, &eol_opts.eol_rx);
}

static const struct opt_conf eol_opts_conf[] = {
    {
        .name = "eol",
        .parse = eol_opt_parse_txrx,
        .descr = "end of line crlf", // TODO
    },
    {
        .name = "eol-tx",
        .parse = eol_opt_parse_tx,
        .descr = "end of line crlf", // TODO
    },
    {
        .name = "eol-rx",
        .parse = eol_opt_parse_rx,
        .descr = "end of line crlf", // TODO
    },
#if 0 // TODO implement
    {
        .name  = "--eol-rx-timeout",
        .parse = opt_ap_int,
        .dest  = &eol_opts.eol_rx_timeout,
        .descr = "Float in seconds. "\
                 "If some data received but no eol received within given time, "\
                 "the buffered data is written output anywas."\
                 "Default: " STRINGIFY(EOL_RX_TIMEOUT_DEFAULT) ". "\
                 "Applies in coocked (line buffered) mode only otherwise ignored. "\

    },
#endif
};

OPT_SECTION_ADD(main,
                eol_opts_conf,
                ARRAY_LEN(eol_opts_conf),
                eol_opt_post_parse);
