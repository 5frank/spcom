
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#define _GNU_SOURCE // TODO portable strcasestr
#include <string.h>

#include "eol.h"
#include "str.h"
#include "utils.h"
#if 0
const struct str_kvi *str_kvi_lookup(const char *s, const struct str_kvi *map, size_t maplen)
{
   for (int i = 0; i < maplen; i++) {
        const struct str_kvi *kv = &map[i];
       if (!strcasecmp(s, kv->key)) {
           return kv;
       }
   }

   return NULL;
}
#endif

#define ESC_STR_MAX_LEN 7

static const char *ascii_cntrl_names[] = {
   //0,     1,     2,     3,     4,     5,     6,     7
    "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL", 
    "BS",  "HT",  "LF",  "VT",  "FF",  "CR",  "SO",  "SI",
    "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
    "CAN", "EM",  "SUB", "ESC", "FS",  "GS",  "RS",  "US",
};

static const char *val_to_cntrl_str(char c)
{
    unsigned char n = c;
    if (n < ARRAY_LEN(ascii_cntrl_names))
        return ascii_cntrl_names[n];

    if (n == 127)
        return "DEL";

    return NULL;
}


static int str_to_cntrl_val(const char *s, int *r)
{
    for (int i = 0; i < ARRAY_LEN(ascii_cntrl_names); i++) {
        if (!strcasecmp(s, ascii_cntrl_names[i])) {
            *r = i;
            return 0;
        }
    }
    if (!strcasecmp(s, "DEL")) {
        *r = 127;
        return 0;
    }

    return -1;
}

struct char_map {
    struct char_map_item {
        char s[ESC_STR_MAX_LEN + 1];
    } map[UCHAR_MAX + 1];
};


static int verify_hexfmt(const char *fmt)
{
    if (!fmt)
        return 1;

    char test_vals[] = { 0x0, 0xAB, 0xFF };
    char buf[ESC_STR_MAX_LEN + 1];
    char hexstr[4];

    for (int i = 0; i < ARRAY_LEN(test_vals); i++) {
        int rc = snprintf(buf, sizeof(buf), fmt, test_vals[i]);
        if (rc <= 0)
            return EINVAL;

        if (rc >= sizeof(buf))
            return E2BIG;

        rc = snprintf(hexstr, sizeof(hexstr), "%x", test_vals[i]);
        assert(rc >= 1);
        assert(rc <= 2);

        char *m = str_strcasestr(buf, hexstr);
        if (!m)
            return EINVAL;
    }
    return 0;
}

const char *charmap_get(const struct char_map *cm, char c)
{
    unsigned char i = c;

    if (!cm)
        return NULL;

    const char *s = cm->map[i].s;
    if (*s == '\0')
        return NULL;

    return s;
}

#if 0
struct hexformater {
    char prefix[4];
    char suffix[4];
    char pad;
    const char *lut;
};

int mk_hexformater(const char *fmt)
{
    const char *src = fmt;
    char *dst = pref
    int i = 0;
    while(1) {
        c = *src++;
        if (c == '%')
            break;

        if (c == '\0')
            return EINVAL;

        if (!isprint(c))
            return EINVAL;

        hf->prefix[i] = c;
        i++;
        if (i >= sizeof(hf->prefix))
            return E2BIG;
    }
    c = *src++;
    if (c == ' ' || c == '0') {
        c = *src++;
        if (c != 2)
            return EINVAL;
            }

}
#endif

struct charmap_opts {
    int use_ascii_name;
    int use_hexfmt;
    char hexfmt[8];
};
struct charmap_opts charmap_opts = {0};
/*
       int isalnum(int c);
       int isalpha(int c);
       int iscntrl(int c);
       int isdigit(int c);
       int isgraph(int c);
       int islower(int c);
       int isprint(int c);
       int ispunct(int c);
       int isspace(int c);
       int isupper(int c);
       int isxdigit(int c);
       int isascii(int c);
       int isblank(int c);
*/

/**
 *
--char-map=cntrl:hex
--char-map=lf:hex
--char-map=cr:null # or none
--char-map=noprint:hex
--char-map=bel:bs
--char-map=0xa:0xb
*/


enum char_class_id {
    CHAR_CLASS_CNTRL = UCHAR_MAX + 1,
    CHAR_CLASS_NONPRINT,
    CHAR_CLASS_NONASCII,
};

enum char_repr_id {
    CHAR_REPR_DEFAULT = 0,
    CHAR_REPR_IGNORE,
    CHAR_REPR_HEX,
    CHAR_REPR_CNTRLNAME,
    CHAR_REPR_STRLITERAL,
};

static const struct str_kvi cclass_id_strmap[] = {
    { "cntrl", CHAR_CLASS_CNTRL },
    { "ctrl", CHAR_CLASS_CNTRL },
    { "nonprint", CHAR_CLASS_NONPRINT },
    { "!isprint", CHAR_CLASS_NONPRINT },
};

static int str_to_cclass_id(const char *s, int *id)
{
    const size_t nmemb = ARRAY_LEN(cclass_id_strmap);
    return str_kvi_getval(s, id, cclass_id_strmap, nmemb);
}

static const struct str_kvi crepr_id_strmap[] = {
    { "hex", CHAR_REPR_HEX },
    { "none", CHAR_REPR_IGNORE },
    { "ignore", CHAR_REPR_IGNORE },
};

static int str_to_crepr_id(const char *s, int *id)
{
    const size_t nmemb = ARRAY_LEN(crepr_id_strmap);
    return str_kvi_getval(s, id, crepr_id_strmap, nmemb);
}

static int str_0xhexbytetoi(const char *s, int *res)
{
        uint8_t u8tmp = 0;
        int err = str_0xhextou8(s, &u8tmp, NULL);
        if (err)
            return err;
        *res = u8tmp;

        return 0;
}
#if 0
static void charmap_set_cntrl_name(struct char_map *cm, char c)
{
    unsigned char i = c;
    const char *s = val_to_cntrl_str(c);
    assert(s);
    strcpy(cm->map[i], s);
}

static void charmap_set_hex(struct char_map *cm, char c)
{
    unsigned char i = c;
    const char *fmt = charmap_opts.hexfmt;
    struct char_map_item *cmi = cm->items[i];

    const int maxlen = sizeof(cmi->s);
    int rc = snprintf(cmi->s, maxlen, fmt, c);
    assert(rc > 0);
    assert(rc < maxlen);
}
#endif

static int charmap_set_char_repr(struct char_map *cm, int c, int creprid)
{
    unsigned char i = c;
    struct char_map_item *cmi = &cm->map[i];

    switch (creprid) {
        default:
        case CHAR_REPR_DEFAULT:
            cmi->s[0] = c;
            cmi->s[1] = '\0';
            break;
        case CHAR_REPR_IGNORE: {
            cmi->s[0] = '\0';
        }
        break;

        case CHAR_REPR_HEX: {
            const char *fmt = charmap_opts.hexfmt;
            const int maxlen = sizeof(cmi->s);
            int rc = snprintf(cmi->s, maxlen, fmt, c);
            assert(rc > 0);
            assert(rc < maxlen);
        }
        break;

        case CHAR_REPR_CNTRLNAME: {
            const char *s = val_to_cntrl_str(c);
            if (s)
                strcpy(cmi->s, s);
        }
        break;
    }
    return 0;
}
static int is_in_cclass(int c, int cclassid)
{
    switch(cclassid) {
        case CHAR_CLASS_CNTRL:
            return iscntrl(c);

        case CHAR_CLASS_NONPRINT:
            return !isprint(c);

        case CHAR_CLASS_NONASCII:
            return !isascii(c);

        default:
            return false;
    }
}

static int charmap_set_class_repr(struct char_map *cm, int cclassid,  int creprid)
{
    int err;
    assert(cclassid >= 0);
    if (cclassid <= UCHAR_MAX) {
        char c = cclassid;
        return charmap_set_char_repr(cm, c, creprid);
    }

    for (int c = 0; c <= UCHAR_MAX; c++) {
        if (!is_in_cclass(c, cclassid))
            continue;

        err = charmap_set_char_repr(cm, c, creprid);
        if (err)
            return err;
    }

    return 0;
}

static int charmap_parse_opts_class(const char *skey, int *key)
{
    int err;
    err = str_0xhexbytetoi(skey, key);
    if (!err)
        return 0;

    err = str_to_cntrl_val(skey, key);
    if (!err)
        return 0;

    err = str_to_cclass_id(skey, key);
    if (!err)
        return 0;

    return EINVAL;
}

static int charmap_parse_opts_repr(const char *sval, int *val)
{
    int err;

    err = str_0xhexbytetoi(sval, val);
    if (!err)
        return 0;

    err = str_to_cntrl_val(sval, val);
    if (!err)
        return 0;

    if (sval[0] == '"') {
        //TODO 
    }

    err = str_to_crepr_id(sval, val);
    if (!err)
        return 0;

    return EINVAL;
}

static int charmap_parse_kv(struct char_map *cm, const struct str_kv *kv)
{
    int err = 0;

    int classid = 0;
    err = charmap_parse_opts_class(kv->key, &classid);
    if (err)
        return err;

    int reprid = 0;
    err = charmap_parse_opts_repr(kv->val, &reprid);
    if (err)
        return err;

    err = charmap_set_class_repr(cm, classid, reprid);
    if (err)
        return err;

    return 0;
}

int charmap_parse_opts(int type, const char *s)
{
    struct char_map cmap;
    struct char_map *cm = &cmap;

    int err = 0;
    char *sdup = strdup(s);
    assert(sdup);

    struct str_kv kvlist[8] = { 0 };
    int n = ARRAY_LEN(kvlist);
    err = str_split_kv_list(sdup, kvlist, &n);
    if (err)
        goto done;

    for (int i = 0; i < n; i++) {
        struct str_kv *kv = &kvlist[i];
        err = charmap_parse_kv(cm, kv);
        if (err)
            goto done;

    }

done:

    free(sdup);
    return err;
}
