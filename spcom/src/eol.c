#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "assert.h"
#include "common.h"
#include "eol.h"
#include "opt.h"
#include "str.h"
#include "strto_r.h"

#define EOL_OPT_IGNORE_DEFAULT '\r'

static struct eol_opts {
    struct eol_seq eol_txrx;
    struct eol_seq eol_tx;
    struct eol_seq eol_rx;
    int ignore;
} _eol_opts = { .ignore = EOL_OPT_IGNORE_DEFAULT };
#if 0
static int eol_match_a_ignore_b(const struct eol_seq *es, int prev_c, int c)
{
    (void) prev_c;

    if (c == es->c_a)
        return EOL_C_MATCH;

    if (c == es->c_b)
        return EOL_C_IGNORE;

    return EOL_C_NOMATCH;
}
#endif

static int eol_match_a(const struct eol_seq *es, int prev_c, int c)
{
    (void)prev_c;
    return (c == es->c_a) ? EOL_C_MATCH : EOL_C_NOMATCH;
}

static int eol_match_ab(const struct eol_seq *es, int prev_c, int c)
{
    // char a = es->c_a;
    // char b = es->c_b;
    bool prev_matched = (prev_c == es->c_a);

    if (!prev_matched) {
        if (c == es->c_a)
            return EOL_C_STASH;
        else
            return EOL_C_NOMATCH;
    }
    else {
        if (c == es->c_b)
            return EOL_C_MATCH;
        else if (c == es->c_a)
            return EOL_C_POP_AND_STASH;
        else
            return EOL_C_POP;
    }
}

static int eol_match_a_or_b(const struct eol_seq *es, int prev_c, int c)
{
    (void)prev_c;
    return (c == es->c_a || c == es->c_b) ? EOL_C_MATCH : EOL_C_NOMATCH;
}

static struct eol_seq eol_tx_default = {
    .match_func = eol_match_a,
    .c_a = '\n',
};

const struct eol_seq *eol_tx = &eol_tx_default;

static struct eol_seq eol_rx_default = {
    .match_func = eol_match_a,
    .c_a = '\n',
    .ignore = EOL_OPT_IGNORE_DEFAULT,
};

const struct eol_seq *eol_rx = &eol_rx_default;

int eol_match(const struct eol_seq *es, int prev_c, int c)
{
    if (_eol_opts.ignore == c)
        return EOL_C_IGNORE;

    if (!es->match_func)
        return EOL_C_NOMATCH;

    return es->match_func(es, prev_c, c);
}

int eol_seq_cpy(const struct eol_seq *es, char *dst, size_t size)
{
    assert(es->match_func);
    assert(size >= 3);

    int len = (es->match_func == eol_match_ab) ? 2 : 1;
    *dst++ = es->c_a;
    if (len > 1) {
        *dst++ = es->c_a;
    }

    *dst = '\0';

    return len;
}

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
        if (strto_uc(s, &ep, 16, byte)) {
            return NULL;
        }
        return ep;
    }

    return NULL;
}

static int eol_opt_parse_str(const char *s, struct eol_seq *es)
{
    s = eol_opt_parse_next(s, &es->c_a);
    if (!s) {
        return -EINVAL;
    }

    switch (*s) {
        case '\0':
            es->match_func = eol_match_a;
            return 0;

        case ',':
            es->match_func = eol_match_ab;
            s++; // jump delmiter
            break;

        case '|':
            es->match_func = eol_match_a_or_b;
            s++; // jump delmiter
            break;

        // "crlf", or "0x130x10" have no delimiter
        default:
            es->match_func = eol_match_ab;
    }

    s = eol_opt_parse_next(s, &es->c_b);
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
    struct eol_seq *txrx = &_eol_opts.eol_txrx; //.
    if (txrx->match_func) {
        eol_tx = txrx;
        eol_rx = txrx;
    }
    struct eol_seq *tx = &_eol_opts.eol_tx;
    if (tx->match_func) {
        eol_tx = tx;
    }

    struct eol_seq *rx = &_eol_opts.eol_rx;
    if (rx->match_func) {
        eol_rx = rx;
    }

    return 0;
}

static int _parse_cb_txrx(const struct opt_conf *conf, char *s)
{
    return eol_opt_parse_str(s, &_eol_opts.eol_txrx);
}

static int _parse_cb_tx(const struct opt_conf *conf, char *s)
{
    return eol_opt_parse_str(s, &_eol_opts.eol_tx);
}

static int _parse_cb_rx(const struct opt_conf *conf, char *s)
{
    return eol_opt_parse_str(s, &_eol_opts.eol_rx);
}

static const struct opt_conf eol_opts_conf[] = {
    {
        .name = "eol",
        .parse = _parse_cb_txrx,
        .descr = "end of line tx and rx", // TODO
    },
    {
        .name = "eol-tx",
        .parse = _parse_cb_tx,
        .descr = "send (transmit) eol used after new line detected on stdin",
    },
    {
        .name = "eol-rx",
        .parse = _parse_cb_rx,
        .descr = "end of line rx", // TODO
    },
#if 0
    {
        .name = "eol-ignore", # see stty {i,o}nocr

        .parse = _parse_cb_ignore,
        .descr = "eol character ignored and not printed to stdout. "\
                 "Default: " STRINGIFY(EOL_OPT_IGNORE_DEFAULT) "."
    },
#endif
};

OPT_SECTION_ADD(main, eol_opts_conf, ARRAY_LEN(eol_opts_conf),
                eol_opt_post_parse);
