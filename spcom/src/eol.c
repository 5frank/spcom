
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include "eol.h"
#include "str.h"
#include "utils.h"



static struct eol_rx_s {
    struct eol_seq hist;
    struct eol_seq seqs[4];
} g_eol_rx = { 0 };

static struct eol_tx_s {
    struct eol_seq seq;
} g_eol_tx = { 0 };


static void eol_seq_msk_putc(struct eol_seq *es, char c)
{
    uint8_t b = c;
    es->seqmsk <<= 8;
    es->seqmsk |= b;

    es->cmpmsk <<= 8;
    es->cmpmsk |= 0xFF;
}

static int eol_seq_putc(struct eol_seq *es, char c)
{
    uint8_t b = c;

    if (es->len >= sizeof(es->seq))
        return ENOMEM;

    es->seq[es->len] = b;
    es->len++;

    eol_seq_msk_putc(es, c);

    return 0;
}

static int eol_seq_set(struct eol_seq *es, const void *data, size_t len)
{
    const char *s = data;
    for (int i = 0; i < len; i++) {
        int err = eol_seq_putc(es, s[i]);
        if (err)
            return err;
    }

    return 0;
}

int eol_rx_check(char c)
{
    struct eol_seq *hist = &g_eol_rx.hist;

    eol_seq_msk_putc(hist, c);

    for (int i = 0; i < ARRAY_LEN(g_eol_rx.seqs); i++) {
        struct eol_seq *es = &g_eol_rx.seqs[i];
        if (es->cmpmsk == 0) {
            return false; // i.e. false. done. assume last N uninitialized
        }
        if (hist->cmpmsk < es->cmpmsk) {
            continue; // to short
        }

        if ((es->cmpmsk & hist->seqmsk) == es->seqmsk) 
            return es->len;
    }
    return false;
}

void eol_rx_check_reset(void) 
{
    struct eol_seq *hist = &g_eol_rx.hist;
    memset(hist, 0, sizeof(*hist));
}

static int eol_add_rx(const void *data, size_t len)
{
    struct eol_seq *es = NULL;
    for (int i = 0; i < ARRAY_LEN(g_eol_rx.seqs); i++) {
        struct eol_seq *tmp = &g_eol_rx.seqs[i];
        if (tmp->cmpmsk == 0) {
            es = tmp;
            break;
        }
    }

    if (!es)
        return -ENOMEM;

    int err = eol_seq_set(es, data, len);
    if (err)
        return err;

    return 0;
}

static int eol_set_tx(const void *data, size_t len)
{
    struct eol_seq *es = &g_eol_tx.seq;
    if (es->len)
        return -1; // already set

    int err = eol_seq_set(es, data, len);
    if (err) 
        return err;

    return 0;
}

int eol_set(int type, const void *data, size_t len)
{
    int err = 0;
    if (!type || !data || !len)
        return EINVAL;

    if (type & EOL_TX) {
        err = eol_set_tx(data, len);
        if (err)
            return err;
    }

    if (type & EOL_RX) {
        err = eol_add_rx(data, len);
        if (err)
            return err;
    }

    return 0;
}


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

int eol_parse_opts(int type, const char *s)
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

        err = eol_set(type, bytes, nbytes);
        if (err)
            goto done;
    }

done:
    free(sdup);

    return err;
}

const struct eol_seq *eol_tx_get(void)
{
    return &g_eol_tx.seq;
}

int eol_init(void)
{
    return 0;
}
