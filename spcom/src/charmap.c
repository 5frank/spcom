
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
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

// clang-format off
static const char *ascii_cntrl_names[] = {
   //0,     1,     2,     3,     4,     5,     6,     7
    "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
    "BS",  "HT",  "LF",  "VT",  "FF",  "CR",  "SO",  "SI",
    "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
    "CAN", "EM",  "SUB", "ESC", "FS",  "GS",  "RS",  "US",
};
// clang-format on

static struct charmap_opts {
    int use_ascii_name;
    int use_hexfmt;
    char hexfmt[8];
} charmap_opts = {0};

static struct char_map {
    uint16_t map[UCHAR_MAX + 1];
};

static struct char_map g_cmap = { 0};

static const char *cntrl_valtostr(int val)
{
    unsigned int n = val;
    if (n < ARRAY_LEN(ascii_cntrl_names))
        return ascii_cntrl_names[n];

    if (n == 127)
        return "DEL";

    return NULL;
}

static int cntrl_strtoval(const char *s, int *val)
{
    for (int i = 0; i < ARRAY_LEN(ascii_cntrl_names); i++) {
        if (!strcasecmp(s, ascii_cntrl_names[i])) {
            *val = i;
            return 0;
        }
    }
    if (!strcasecmp(s, "DEL")) {
        *val = 127;
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

static char g_hextbl[UCHAR_MAX + 1][REPR_BUF_SIZE];

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
    const char *strs[16];
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

int charmap_remap(char c, char *buf, int *remapped)
{
    struct char_map *cm = &g_cmap;
    unsigned char i = c;

    const char *s;
    uint16_t x = cm->map[i];

    *remapped = ((int) x == (int) c) ? false : true;

    if (x < CHAR_REPR_BASE)
        return bufputc(buf, x);

    if (x == CHAR_REPR_HEX)
        return bufputs(buf, g_hextbl[i]);

    if (x == CHAR_REPR_IGNORE)
        return -1;

    if (x == CHAR_REPR_CNTRLNAME) {
        s = cntrl_valtostr(c);
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

// clang-format off
static const struct str_kvi cclass_id_strmap[] = {
    { "cntrl",    CHAR_CLASS_CNTRL },
    { "ctrl",     CHAR_CLASS_CNTRL },
    { "nonprint", CHAR_CLASS_NONPRINT },
    { "nprint",   CHAR_CLASS_NONPRINT },
};
// clang-format on

static int str_to_cclass_id(const char *s, int *id)
{
    const size_t nmemb = ARRAY_LEN(cclass_id_strmap);
    return str_kvi_getval(s, id, cclass_id_strmap, nmemb);
}

// clang-format off
static const struct str_kvi crepr_id_strmap[] = {
    { "hex",    CHAR_REPR_HEX },
    { "none",   CHAR_REPR_IGNORE },
    { "ignore", CHAR_REPR_IGNORE },
};
// clang-format on

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

static int charmap_set_class_repr(struct char_map *cm, int x, int creprid)
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

    err = cntrl_strtoval(skey, key);
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

    err = cntrl_strtoval(sval, reprid);
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

/**
  examples of key:value options
        cntrl:hex
        lf:hex
        cr:null # or none
        noprint:hex
        bel:bs
        0xa:0xb

        cr:cntrlname
        lf:cntrlname == lf:\"LF\"
        cntrl:cntrlname

*/
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
    struct char_map *cm = &g_cmap;

    int err = 0;
    char *sdup = strdup(s);
    assert(sdup);

    int numkv = 32;
    struct str_kv *kvls = malloc(sizeof(struct str_kv) * numkv);
    assert(kvls);

    err = str_split_kv_list(sdup, kvls, &numkv);
    if (err)
        goto done;

    for (int i = 0; i < numkv; i++) {
        struct str_kv *kv = &kvls[i];
        err = charmap_set(cm, kv->key, kv->val);
        if (err)
            goto done;
    }

done:
    free(sdup);
    free(kvls);

    return err;
}

