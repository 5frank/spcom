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

enum eol_type_e {
    EOL_TX = 1 << 0,
    EOL_RX = 1 << 1
};

static int eol_eval_sz1(struct eol_seq *es, char c, char *cfwd)
{
    if (es->seq[0] == c) {
        return EOL_C_FOUND;
    }

    return EOL_C_NONE;
}

int eol_eval_sz2(struct eol_seq *es, char c, char *cfwd)
{
    // most common case
    if (es->index == 0) {
        if (es->seq[0] == c) {
            es->index = 1;
            return EOL_C_CONSUMED;
        }

        // forward as not part of EOL sequence
        return EOL_C_NONE;
    }
    // else if es->index == 1

    if (es->seq[1] == c) {
        es->index = 0; // reset
        return EOL_C_FOUND;
    }

    // if not complete match - check if c matches first char in seq
    if (es->seq[0] == c) {
        es->index = 0; // reset
        cfwd[0] = es->seq[1];
        return EOL_C_FWD1;
    }

    // forward as not part of EOL sequence
    return EOL_C_NONE;
}

static int eol_eval_lfnocr(struct eol_seq *es, char c, char *cfwd)
{
    if (c == '\r')
        return EOL_C_CONSUMED;

    if (c == '\n')
        return EOL_C_FOUND;

    return EOL_C_NONE;
}

static int eol_eval_or(struct eol_seq *es, char c, char *cfwd)
{
    char a = es->seq[0];
    char b = es->seq[1];

    if (c == a || c == b)
        return EOL_C_FOUND;

    return EOL_C_NONE;
}

static struct eol_seq eol_seq_tx = {
    .handler = eol_eval_sz1, // should never be used
    .seq = { '\n' },
    .len = 1,
};

struct eol_seq *eol_tx = &eol_seq_tx;

static struct eol_seq eol_seq_rx = {
    .handler = eol_eval_lfnocr,
};

struct eol_seq *eol_rx = &eol_seq_rx;

static int eol_set(struct eol_seq *es, const uint8_t *bytes, size_t len)
{
    if (len > ARRAY_LEN(es->seq))
        return E2BIG;

    switch (len) {
        case 0:
            return ENOMSG;
            break;
        case 1:
            es->handler = eol_eval_sz1;
            es->seq[0] = bytes[0];
            break;
        case 2:
            es->handler = eol_eval_sz2;
            es->seq[0] = bytes[0];
            es->seq[1] = bytes[1];
            break;
        default:
            return -1;
            break;
    }

    es->len = len;
    es->index = 0;
    return 0;
}

static const struct str_map eol_aliases[] = {
    STR_MAP_STR("CRLF",   "\r\n" ),
    STR_MAP_STR("LFCR",   "\n\r" ),
    STR_MAP_STR("CR",     "\r" ),
    STR_MAP_STR("LF",     "\n" ),
};

static int parse_modify_str_seq(struct eol_seq *es, char *s)
{
    int err = 0;

    uint8_t bytes[4];
    const int len_max = ARRAY_LEN(bytes);
    int len = 0;

    // add margin for trailing commas
    char *toks[ARRAY_LEN(bytes) + 2];
    int ntoks = ARRAY_LEN(toks);
    err = str_split(s, ",", toks, &ntoks);
    if (err)
        return err;


    for (int i = 0; i < ntoks; i++) {
        char *tok = toks[i];
        char c = *tok;

        if (c == '0') {
            uint8_t b = 0;
            err = str_0xhextou8(tok, &b, NULL);
            if (err)
                return err;

            if (len >= len_max)
                return E2BIG;

            bytes[len] = b;
            len += 1;
            continue;
        }
        size_t nitems = ARRAY_LEN(eol_aliases);
        const struct str_map *ea = str_map_lookup(tok, eol_aliases, nitems);
        if (ea) {
            int slen = strlen(ea->val.v_str);
            if ((len + slen) >= len_max)
                return E2BIG;

            memcpy(&bytes[len], ea->val.v_str, slen);
            len += slen;
            continue;
        }
    }

    return eol_set(es, bytes, len);

    return 0;
}

static int parse_str_seq(int flags, const char *s)
{
    int err = 0;
    struct eol_seq tmp;

    char *sdup = strdup(s);
    assert(sdup);

    err = parse_modify_str_seq(&tmp, sdup);
    if (err)
        goto done;

    if (flags & EOL_TX)
        eol_seq_tx = tmp;

    if (flags & EOL_RX)
        eol_seq_rx = tmp;

done:
    free(sdup);
    return err;
}

static int eol_opt_post_parse(const struct opt_section_entry *entry)
{
    // note: do not use LOG here
    return 0;
}

static int parse_eol(const struct opt_conf *conf, char *s)
{
    return parse_str_seq(EOL_TX | EOL_RX, s);
}

static int parse_eol_tx(const struct opt_conf *conf, char *s)
{
    return parse_str_seq(EOL_TX, s);
}

static int parse_eol_rx(const struct opt_conf *conf, char *s)
{
    return parse_str_seq(EOL_RX, s);
}

static const struct opt_conf eol_opts_conf[] = {
    {
        .name = "eol",
        .parse = parse_eol,
        .descr = "end of line crlf", // TODO
    },
    {
        .name = "eol-tx",
        .parse = parse_eol_tx,
        .descr = "end of line crlf", // TODO
    },
    {
        .name = "eol-rx",
        .parse = parse_eol_rx,
        .descr = "end of line crlf", // TODO
    },
};

OPT_SECTION_ADD(main,
                eol_opts_conf,
                ARRAY_LEN(eol_opts_conf),
                eol_opt_post_parse);
