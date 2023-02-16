#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assert.h"
#include "charmap.h"
#include "common.h"
#include "ctohex.h"
#include "eol.h"
#include "opt.h"
#include "str.h"
#include "strto.h"

enum char_group_id {
    CHARMAP_GROUP_CNTRL = UCHAR_MAX + 1,
    CHARMAP_GROUP_NONPRINT,
    CHARMAP_GROUP_NONASCII,
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

struct charmap_s {
    /**
     * map[<groupid>] = <reprid>.
     *
     * note that both <groupid> can be a single character (byte).
     * map initalized to its index `map[i] = i` then any remapping applied.
     * i.e. remapped if `map[i] != i`
     *
     * if map[c] < CHARMAP_REPR_BASE - c is remapped to map[x] (single byte
     * value) if map[c] ==  CHARMAP_REPR_HEX - c is remapped to hex
     * representation if map[c] >= CHARMAP_REPR_STRLIT_BASE, it is some string
     */
    uint16_t map[UCHAR_MAX + 1];
};

// initalized if needed
static struct charmap_s _charmap_tx = { 0 };
;
// exposed const pointer. NULL until enabled
const struct charmap_s *charmap_tx = NULL;
// initalized if needed
static struct charmap_s _charmap_rx = { 0 };
// exposed const pointer. NULL until enabled
const struct charmap_s *charmap_rx = NULL;

int charmap_repr_type(const struct charmap_s *cm, int c)
{
    if (!cm) {
        return CHARMAP_REPR_NONE;
    }

    uint16_t reprid = cm->map[(unsigned char)c];

    bool is_remapped = ((int)reprid == (int)c) ? false : true;

    if (!is_remapped) {
        return CHARMAP_REPR_NONE;
    }

    // clamp ranges to base value
    if (reprid < CHARMAP_REPR_BASE) {
        return CHARMAP_REPR_BASE;
    }
    if (reprid >= CHARMAP_REPR_STRLIT_BASE) {
        return CHARMAP_REPR_STRLIT_BASE;
    }

    return reprid;
}

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

struct charmap_strlits {
    unsigned int count;
    const char *strs[16];
};

static struct charmap_strlits *gp_strlits = NULL;

static const char *_strlit_get(int reprid)
{
    if (!gp_strlits)
        return NULL;

    struct charmap_strlits *sl = gp_strlits;

    unsigned int i = reprid - CHARMAP_REPR_STRLIT_BASE;
    if (i >= sl->count)
        return NULL;

    return sl->strs[i];
}

static int _strlit_add(const char *s, int *id)
{
    if (strlen(s) >= CHARMAP_REPR_BUF_SIZE) {
        return E2BIG;
    }

    if (!gp_strlits) {
        gp_strlits = malloc(sizeof(struct charmap_strlits));
        assert(gp_strlits);
    }

    struct charmap_strlits *sl = gp_strlits;
    unsigned int i = sl->count;
    assert(i < ARRAY_LEN(sl->strs));

    sl->strs[i] = s;
    sl->count++;

    *id = i + CHARMAP_REPR_STRLIT_BASE;
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
    for (int i = 0; i < CHARMAP_REPR_BUF_SIZE; i++) {
        *dst++ = *src++;
        if (*src == '\0')
            return i;
    }
    assert(0);
    return 0;
}

int charmap_remap(const struct charmap_s *cm, char c, char *buf)
{
    assert(cm);

    const char *s;

    uint16_t reprid = cm->map[(unsigned char)c];

#if 0
    bool is_remapped = ((int) reprid == (int) c) ? false : true;
    if (!is_remapped)
        return bufputc(buf, c);
#endif

    if (reprid < CHARMAP_REPR_BASE) {
        return bufputc(buf, reprid);
    }

    if (reprid == CHARMAP_REPR_HEX) {
        _Static_assert(CTOHEX_BUF_SIZE <= CHARMAP_REPR_BUF_SIZE, "");
        return ctohex(c, buf);
    }

    if (reprid == CHARMAP_REPR_IGNORE) {
        *buf = '\0'; // always nul terminate
        return 0;
    }

    if (reprid == CHARMAP_REPR_CNTRLNAME) {
        s = cntrl_valtostr(c);
        if (s)
            return bufputs(buf, s);
        else
            return bufputc(buf, c);
    }

    if (reprid >= CHARMAP_REPR_STRLIT_BASE) {
        s = _strlit_get(reprid);
        if (s)
            return bufputs(buf, s);
        else
            return bufputc(buf, c);
    }

    return bufputc(buf, c);
}

// clang-format off
static const struct str_map group_id_strmap[] = {
    STR_MAP_INT("cntrl",    CHARMAP_GROUP_CNTRL ),
    STR_MAP_INT("ctrl",     CHARMAP_GROUP_CNTRL ),
    STR_MAP_INT("nonprint", CHARMAP_GROUP_NONPRINT ),
    STR_MAP_INT("nisprint", CHARMAP_GROUP_NONPRINT ),
};
// clang-format on

static int str_to_group_id(const char *s, int *id)
{
    const size_t nmemb = ARRAY_LEN(group_id_strmap);
    return str_map_to_int(s, group_id_strmap, nmemb, id);
}

// clang-format off
static const struct str_map crepr_id_strmap[] = {
    STR_MAP_INT("hex",    CHARMAP_REPR_HEX ),
    STR_MAP_INT("none",   CHARMAP_REPR_IGNORE ),
    STR_MAP_INT("ignore", CHARMAP_REPR_IGNORE ),
};
// clang-format on

static int str_to_crepr_id(const char *s, int *id)
{
    const size_t nmemb = ARRAY_LEN(crepr_id_strmap);
    return str_map_to_int(s, crepr_id_strmap, nmemb, id);
}

static int _hexbytetoi(const char *s, int *res)
{
    uint8_t u8tmp = 0;
    int err = strto_u8(s, NULL, 16, &u8tmp);
    if (err)
        return err;
    *res = u8tmp;

    return 0;
}

static int is_in_group(int c, int groupid)
{
    switch (groupid) {
        case CHARMAP_GROUP_CNTRL:
            return iscntrl(c);

        case CHARMAP_GROUP_NONPRINT:
            return !isprint(c);

        case CHARMAP_GROUP_NONASCII:
            return !isascii(c);

        default:
            return false;
    }
}

static int charmap_set_group_repr(struct charmap_s *cm, int groupid, int reprid)
{
    assert(groupid >= 0);
    /* group is a single char remapped to the value of `reprid` */
    if (groupid <= UCHAR_MAX) {
        unsigned char c = groupid;
        cm->map[c] = reprid;
        return 0;
    }

    for (unsigned char c = 0; c <= UCHAR_MAX; c++) {
        if (!is_in_group(c, groupid))
            continue;

        cm->map[c] = reprid;
    }

    return 0;
}

static int charmap_opt_parse_group(const char *skey, int *key)
{
    int err;
    err = _hexbytetoi(skey, key);
    if (!err)
        return 0;

    err = cntrl_strtoval(skey, key);
    if (!err)
        return 0;

    err = str_to_group_id(skey, key);
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

/// @return copy of string with first and last char removed
static char *str_crop_cpy(const char *str)
{
    int len = strlen(str);
    str++; // skip leading
    char *s = malloc(len);

    // skip trailing as strlen exlcudes nul terminator
    assert(s);
    memcpy(s, str, len);
    s[len] = '\0';

    return s;
}

static int charmap_opt_parse_repr(const char *sval, int *reprid)
{
    int err;

    err = _hexbytetoi(sval, reprid);
    if (!err)
        return 0;

    err = cntrl_strtoval(sval, reprid);
    if (!err)
        return 0;

    err = str_to_crepr_id(sval, reprid);
    if (!err)
        return 0;

    if (str_isquoted(sval, '"')) {
        char *s = str_crop_cpy(sval);

        err = _strlit_add(s, reprid);
        if (err) {
            free(s);
            return err;
        }

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
static int charmap_set(struct charmap_s *cm, const char *skey, const char *sval)
{
    int err = 0;

    int groupid = 0;
    err = charmap_opt_parse_group(skey, &groupid);
    if (err)
        return err;

    int reprid;
    err = charmap_opt_parse_repr(sval, &reprid);
    if (err)
        return err;

    err = charmap_set_group_repr(cm, groupid, reprid);
    if (err)
        return err;

    return 0;
}

static int charmap_parse_opts(struct charmap_s *cm, const char *s)
{
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

static int parse_map_txc(const struct opt_conf *conf, char *s)
{
    int err = charmap_parse_opts(&_charmap_tx, s);
    if (!err)
        charmap_tx = &_charmap_tx; // i.e. enable

    return err;
}

static int parse_map_rxc(const struct opt_conf *conf, char *s)
{
    int err = charmap_parse_opts(&_charmap_rx, s);
    if (!err)
        charmap_rx = &_charmap_rx; // i.e. enable

    return err;
}

static int charmap_opts_post_parse(const struct opt_section_entry *entry)
{
    // note: do not use LOG here
#if 0
    charmap_print_map("tx", charmap_tx);
    charmap_print_map("rx", charmap_rx);
#endif

    return 0;
}

static const struct opt_conf charmap_opts_conf[] = {
    {
        .name = "map-txc",
        .parse = parse_map_txc,
        .descr = "map TX (sent) character(s)",
    },
    {
        .name = "map-rxc",
        .parse = parse_map_rxc,
        .descr = "map RX (received) character(s)",
    },
};

OPT_SECTION_ADD(outfmt, charmap_opts_conf, ARRAY_LEN(charmap_opts_conf),
                charmap_opts_post_parse);
