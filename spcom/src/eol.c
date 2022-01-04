#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "str.h"
#include "opt.h"
#include "utils.h"
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
    (void)es;
    if (c == '\r')
       return EOL_C_CONSUMED;

    if (c == '\n')
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
    .handler = eol_eval_lfnocr
};

struct eol_seq *eol_rx = &eol_seq_rx;
#if 0
void example(void)
{
    char cfwd[2];
    int ec = eol_eval(es, c, cfwd);
    switch (ec) {
        case EOL_C_NONE:
            strbuf_putc(c);
            break;

        case EOL_C_CONSUMED:
            break;

        case EOL_C_FOUND:
            strbuf_putc('\n');
            break;

        case EOL_C_FWD1:
            strbuf_putc(cfwd[0]);
            break;

        case EOL_C_FWD2:
            strbuf_putc(cfwd[0]);
            strbuf_putc(cfwd[1]);
            break;

        default:
            LOG_ERR("never!");
            strbuf_putc(c);
            break;
    }
}
#endif


static const struct str_kv eol_aliases[] = {
    { "CRLF",   "\r\n" },
    { "LFCR",   "\n\r" },
    { "CR",     "\r" },
    { "LF",     "\n" },
};

static int eol_sseq_to_bytes(char *s, uint8_t *bytes, int *n)
{
    int err = 0;
    char *toks[4]; // TODO ARRAY_LEN
    int ntoks = ARRAY_LEN(toks);
    err = str_split(s, ",", toks, &ntoks);
    if (err)
        return err;

    const int bcountmax = *n;
    *n = 0;

    for (int i = 0; i < ntoks; i++) {
        char *tok = toks[i];
        char c = *tok;

        if (c == '0') {
            uint8_t b = 0;
            err = str_0xhextou8(tok, &b, NULL);
            if (err)
                return err;

            if (*n >= bcountmax)
                return E2BIG;

            bytes[*n] = b;
            *n += 1;
            continue;
        }
        size_t nitems = ARRAY_LEN(eol_aliases);
        const struct str_kv *ea = str_kv_lookup(tok, eol_aliases, nitems);
        if (ea) {
            int slen = strlen(ea->val);
            if ((*n + slen) >= bcountmax)
                return E2BIG;

            memcpy(&bytes[*n], ea->val, slen);
            *n += slen;
            continue;
        }
    }

    return 0;
}

static int eol_parse_opts(int type, const char *s)
{
    int err = 0;
    char *sdup = strdup(s);
    assert(sdup);
    char *sp = sdup;

    char *toks[4]; // TODO ARRAY_LEN
    int ntoks = ARRAY_LEN(toks);
    err = str_split(sp, "|", toks, &ntoks);
    if (err)
        goto done;

    err = ((type != EOL_RX) && (ntoks != 1)) ? EINVAL : 0;
    if (err)
        goto done;

    err = (ntoks <= 0) ? EINVAL : 0;
    if (err)
        goto done;

    uint8_t bytes[4];
    for (int i = 0; i < ntoks; i++) {
        int nbytes = ARRAY_LEN(bytes);
        err = eol_sseq_to_bytes(toks[i], bytes, &nbytes);
        if (err)
            goto done;

        err = (nbytes <= 0) ? EINVAL : 0;
        if (err)
            goto done;
#if 0 // TODO
        err = eol_set(type, bytes, nbytes);
        if (err)
            goto done;
#endif
    }

done:
    free(sdup);

    return err;
}

static int eol_opt_post_parse(const struct opt_section_entry *entry)
{

    return 0;
}

static int parse_eol(const struct opt_conf *conf, char *s)
{
#if 0
    unsigned int flags = EOL_TX | EOL_RX;
    eol_opts.have_flags |= flags;
    return eol_parse_opts(flags, s);
#endif
    return -1;
}

static int parse_eol_tx(const struct opt_conf *conf, char *s)
{
#if 0
    unsigned int flags = EOL_TX;
    eol_opts.have_flags |= flags;
    return eol_parse_opts(flags, s);
#endif
    return -1;
}

static int parse_eol_rx(const struct opt_conf *conf, char *s)
{
#if 0
    unsigned int flags = EOL_RX;
    eol_opts.have_flags |= flags;
    return eol_parse_opts(flags, s);
#endif
    return -1;
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

OPT_SECTION_ADD(main, eol_opts_conf, ARRAY_LEN(eol_opts_conf), eol_opt_post_parse);
