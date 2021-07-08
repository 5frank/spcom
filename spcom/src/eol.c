
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
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

int eol_config(int type, const void *data, size_t len)
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

struct eol_alias {
    const char *name;
    const char *val;
};

static const struct eol_alias eol_aliases[] = {
    { "CRLF",   "\r\n" },
    { "LFCR",   "\n\r" },
    { "CR",     "\r" },
    { "LF",     "\n" },
};

static const struct eol_alias *parse_lookup_alias(const char *s) 
{
    for (int i = 0; i < ARRAY_LEN(eol_aliases); i++) {
        const struct eol_alias *p = &eol_aliases[i];
        if (!strcasecmp(s, p->name))
            return p;
    }

    return NULL;
}


int eol_parse_opts(int type, const char *s)
{
    int err = 0;
    if (!s)
        return EINVAL;

    int count = 0;
    while (1) {
        char c = *s;
        if (c == '\0') {
            break;
        }

        if (c == ',') {
            s++;
            continue;
        }

        if (c == '0') {
            uint8_t b = 0;
            const char *ep = NULL;
            err = str_0xhextou8(s, &b, &ep);
            if (err)
                return err;

            err = eol_config(type, &b, 1);
            if (err)
                return err;
            s = ep;
            count++;
            continue;
        }

        const struct eol_alias *ea = parse_lookup_alias(s);
        if (ea) {
            err = eol_config(type, ea->val, strlen(ea->val));
            if (err)
                return err;
            s += strlen(ea->name);
            count++;
            continue;
        }

        return EINVAL;
    }

    if (!count)
        return EINVAL;

    return 0;
}

const struct eol_seq *eol_tx_get(void)
{
    return &g_eol_tx.seq;
}
