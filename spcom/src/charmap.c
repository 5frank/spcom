
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

#define REPR_BUF_SIZE 8

enum char_class_id {
    CHAR_CLASS_CNTRL = UCHAR_MAX + 1,
    CHAR_CLASS_NONPRINT,
    CHAR_CLASS_NONASCII,
};

enum char_repr_id {
    CHAR_REPR_BASE = UCHAR_MAX + 1,
    CHAR_REPR_IGNORE,
    CHAR_REPR_HEX,
    CHAR_REPR_CNTRLNAME,
    CHAR_REPR_STRLIT_BASE,
};

struct charmap_opts {
    int use_ascii_name;
    int use_hexfmt;
    char hexfmt[8];
};
struct charmap_opts charmap_opts = {0};

struct char_map {
#if 0
    struct char_map_item {
        char s[ESC_STR_MAX_LEN + 1];
    } map[UCHAR_MAX + 1];
#endif
    uint16_t map[UCHAR_MAX + 1];
};

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

static int hex_is_valid_fmt(const char *fmt)
{
    if (!fmt)
        return 1;

    char buf[REPR_BUF_SIZE];

    int rc = snprintf(buf, sizeof(buf), fmt, 0xAB);
    if (rc < 1)
        return EINVAL;

    if (rc >= sizeof(buf))
        return ERANGE;

    if (!strstr(buf, "ab") && !strstr(buf, "AB"))
        return EINVAL;

    return 0;
}

char g_hextbl[UCHAR_MAX + 1][REPR_BUF_SIZE];

static void hex_mk_table(void) 
{
    const char *fmt = charmap_opts.hexfmt;
    const int maxlen = sizeof(g_hextbl[0]);
    for (int i = 0; i <= UCHAR_MAX; i++) {
        char c = i;
        int rc = snprintf(g_hextbl[i], maxlen, fmt, c);
        assert(rc > 0);
        assert(rc < maxlen);
    }
}

struct strlits {
    unsigned int count;
    const char *strs[UCHAR_MAX + 1];
};

static struct strlits *gp_strlits = NULL;

static const char *_strlit_get(unsigned int x)
{

    if (!gp_strlits)
        return NULL;

    struct strlits *sl = gp_strlits;

    unsigned int i = x - CHAR_REPR_STRLIT_BASE;
    if (i >= sl->count)
        return NULL;

    return sl->strs[i];
}

char *str_simple_unquote_cpy(const char *str)
{
    int len = strlen(str);
    str++; // skip leading
    // skip trailing as len exlcude nul terminator
    char *s = malloc(len);
    assert(s);
    for (int i = 0; i < len; i++) {
        s[i] = *str++;
    }
    s[len] = '\0';
    return s;
}
/// assume params str to be within quotes
static int _strlit_add(const char *str, int *id)
{
    char *s = str_simple_unquote_cpy(str);

    if (strlen(s) >= REPR_BUF_SIZE) {
        free(s);
        return E2BIG;
    }

    if (!gp_strlits) {
        gp_strlits = malloc(sizeof(struct strlits));
        assert(gp_strlits);
    }

    struct strlits *sl = gp_strlits;
    unsigned int i = sl->count;
    assert(i < ARRAY_LEN(sl->strs));

    sl->strs[i] = s;
    sl->count++;

    *id = i + CHAR_REPR_STRLIT_BASE;
    return 0;
}
#if 0
static void _strlit_cleanup(void)
{
    if (!gp_strlits)
        return;

    struct strlits *sl = gp_strlits;
    for (int i = 0; i < sl->count; i++) {
        free(sl->strs[i]);
    }
    free(sl);
    gp_strlits = NULL;
}
#endif

static inline int bufputc(char *dst, char c)
{
    *dst++ = c;
    *dst = '\0';
    return 1;
}

static int bufputs(char *dst, const char *src)
{
    assert(src);
    for (int i = 0; i < REPR_BUF_SIZE; i++) {
            *dst++ = *src++;
            if (*src == '\0')
                return i;
    }
    assert(0);
    return 0;
}

int charmap_get(const struct char_map *cm, char c, char *buf)
{
    unsigned char i = c;
#if 0

    const char *s = cm->map[i].s;
    if (*s == '\0')
        return NULL;

    return s;
#else
    int paranoid = 1;
    if (!cm && paranoid)
        return bufputc(buf, c);

    const char *s;
    uint16_t x = cm->map[i];
    if (x < CHAR_REPR_BASE)
        return bufputc(buf, c);

    if (x == CHAR_REPR_HEX)
        return bufputs(buf, g_hextbl[i]);

    if (x == CHAR_REPR_IGNORE)
        return 0;

    if (x == CHAR_REPR_CNTRLNAME) {
        s = val_to_cntrl_str(c);
        if (s)
            return bufputs(buf, s);
        else
            return bufputc(buf, c);
    }

    if (x >= CHAR_REPR_STRLIT_BASE) {
        s = _strlit_get(x);
        if (s)
            return bufputs(buf, s);
        else
            return bufputc(buf, c);
    }

    assert(0); // should not get here
    return bufputc(buf, c);
#endif
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


/**
 *
--char-map=cntrl:hex
--char-map=lf:hex
--char-map=cr:null # or none
--char-map=noprint:hex
--char-map=bel:bs
--char-map=0xa:0xb

--char-map=cr:cntrlname
--char-map=lf:cntrlname == --char-map=lf:\"LF\"

--char-map=cntrl:cntrlname
--char-map-cntrl-names=yes
*/



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

int charmap_set_class_repr(struct char_map *cm, int x, int creprid)
{
    assert(x >= 0);
    if (x <= UCHAR_MAX) {
        unsigned char i = x;
        cm->map[i] = creprid;
        return 0;
    }

    int cclassid = x;
    for (int c = 0; c <= UCHAR_MAX; c++) {
        if (!is_in_cclass(c, cclassid))
            continue;

        cm->map[x] = creprid;
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

int str_isquoted(const char *s, char qc) 
{
    if (!s)
        return false;

    int n = strlen(s);

    if (s[0] == qc && s[n] == qc)
        return true;

    return false;
}

static int charmap_parse_opts_repr(const char *sval, int *reprid)
{
    int err;

    err = str_0xhexbytetoi(sval, reprid);
    if (!err)
        return 0;

    err = str_to_cntrl_val(sval, reprid);
    if (!err)
        return 0;

    err = str_to_crepr_id(sval, reprid);
    if (!err)
        return 0;

    if (str_isquoted(sval, '"')) {
        err = _strlit_add(sval, reprid);
        if (err)
            return err;
        else
            return 0;
    }

    return EINVAL;
}

static int charmap_set(struct char_map *cm, const char *skey, const char *sval)
{
    int err = 0;

    int classid = 0;
    err = charmap_parse_opts_class(skey, &classid);
    if (err)
        return err;

    int reprid;
    err = charmap_parse_opts_repr(sval, &reprid);
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
        err = charmap_set(cm, kv->key, kv->val);
        if (err)
            goto done;

    }

done:

    free(sdup);
    return err;
}

