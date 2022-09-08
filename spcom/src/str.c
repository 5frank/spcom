// std
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
// local
#include "assert.h"
#include "common.h"
#include "str.h"
#include "strto.h"
#include "vt_defs.h"

static const char *_matchresult[32];

#define STR_MATCH_LIST(S, LIST)                                                \
    str_match_list(S, LIST, sizeof(LIST[0]), ARRAY_LEN(LIST))

const char **str_match_list(const char *s, const void *items, size_t itemsize,
                            size_t numitems)
{
    if (!s)
        s = "";

    int n = 0;
    int slen = strlen(s);

    // need a char pointer as increments of sizeof(char) and not sizeof(char *)
    const char *item = items;

    for (int i = 0; i < numitems; i++) {
        /* string pointer is the dereferenced value at begining at of every item
         but dereferencing it is not pretty. sorry! */
        const char *name = *((const char **)item);

        // have a item at every itemsize chunk
        item += itemsize;

        if (!name)
            break;

        if (strncmp(name, s, slen))
            continue;

        _matchresult[n++] = name;
        if (n >= (ARRAY_LEN(_matchresult) - 1))
            break;
    }
    // n = 0 if no matches
    _matchresult[n] = NULL;

    return _matchresult;
}
#if 1

const struct str_map *str_map_lookup(const char *s, const struct str_map *map,
                                     size_t map_len)
{
    for (unsigned int i = 0; i < map_len; i++) {
        const struct str_map *itm = &map[i];
        if (!strcasecmp(s, itm->key)) {
            return itm;
        }
    }

    return NULL;
}
#else
const struct str_kvi *str_kvi_lookup(const char *s, const struct str_kvi *map,
                                     size_t n)
{
    for (int i = 0; i < n; i++) {
        const struct str_kvi *kvi = &map[i];
        if (!strcasecmp(s, kvi->key)) {
            return kvi;
        }
    }

    return NULL;
}

int str_kvi_getval(const char *s, int *val, const struct str_kvi *map, size_t n)
{
    const struct str_kvi *kvi = str_kvi_lookup(s, map, n);
    if (!kvi)
        return -ENOENT;

    *val = kvi->val;
    return 0;
}

const struct str_kv *str_kv_lookup(const char *s, const struct str_kv *map,
                                   size_t n)
{
    for (int i = 0; i < n; i++) {
        const struct str_kv *kv = &map[i];
        if (!strcasecmp(s, kv->key)) {
            return kv;
        }
    }

    return NULL;
}
#endif

// clang-format off
static const char* bool_words[] =  {
   "0",     "1",
   "n",     "y",
   "no",    "yes",
   "false", "true",
   "off",   "on"
};
// clang-format on

int str_to_bool(const char *s, bool *rbool)
{
    assert(s);
    assert(rbool);
    int i = str_list_indexof(s, bool_words, ARRAY_LEN(bool_words));
    if (i < 0) {
        return -ENOENT;
    }

    // odd words are true
    *rbool = (i & 1);
    return 0;
}

const char **str_match_bool(const char *s)
{
    return STR_MATCH_LIST(s, bool_words);
}

int str_0xhextou8(const char *s, uint8_t *res, const char **ep)
{
    if (*s != '0')
        return EINVAL;
    s++;

    if (*s != 'x')
        return EINVAL;
    s++;

    return strto_u8(s, ep, 16, res);
}

char *str_lstrip(char *s)
{
    while (isspace(*s))
        s++;
    return s;
}

char *str_rstrip(char *s)
{
    char *back = s + strlen(s);
    while (isspace(*--back))
        ;
    *(back + 1) = '\0';
    return s;
}

char *str_strip(char *s) { return str_rstrip(str_lstrip(s)); }

char *str_lstripc(char *s, char c)
{
    while (*s == c)
        s++;
    return s;
}

char *str_rstripc(char *s, char c)
{
    char *back = s + strlen(s);
    while (*--back == c)
        ;
    *(back + 1) = '\0';
    return s;
}

char *str_stripc(char *s, char c) { return str_rstripc(str_lstripc(s, c), c); }

int str_split(char *s, const char *delim, char *toks[], int *n)
{
    const int nmax = *n;
    *n = 0;
    while (1) {
        char *tok = strsep(&s, delim);

        if (!tok)
            break;

        if (tok[0] == '\0')
            continue;

        if (*n >= nmax)
            return E2BIG;

        toks[*n] = tok;
        (*n)++;
    }

    return 0;
}

int str_split_kv(char *s, struct str_kv *kv)
{
    char *toks[2];
    int n = ARRAY_LEN(toks);
    int err = str_split(s, ":", toks, &n);
    if (err)
        return err;

    if (n != ARRAY_LEN(toks))
        return EINVAL;

    kv->key = str_strip(toks[0]);
    kv->val = str_strip(toks[1]);

    return 0;
}

int str_split_kv_list(char *s, struct str_kv *kvlist, int *n)
{
    int err = 0;
    const int nmax = *n;
    *n = 0;
    while (1) {
        char *tok = strsep(&s, ",");

        if (!tok)
            break;

        if (tok[0] == '\0')
            continue;

        if (*n >= nmax)
            return E2BIG;

        struct str_kv *kv = &kvlist[*n];
        err = str_split_kv(tok, kv);
        if (err)
            break;

        (*n)++;
    }

    return err;
}

const struct str_kv *str_kv_get(const char *s, const struct str_kv *items,
                                int nitems)
{
    for (int i = 0; i < nitems; i++) {
        const struct str_kv *kv = &items[i];
        if (!strcasecmp(s, kv->key))
            return kv;
    }

    return NULL;
}

/// note: does not nul terminate
static int _escape_nonprint_char(char c, char *buf)
{
    static const char *hexlut = "0123456789ABCDEF";

    char *p = &buf[0];

    if (isprint(c) && c != '\\') {
        *p++ = c;
        return 1;
    }

    switch (c) {
        case '\a':
            *p++ = '\\';
            *p++ = 'a';
            return 2;
        case '\b':
            *p++ = '\\';
            *p++ = 'b';
            return 2;
        case '\t':
            *p++ = '\\';
            *p++ = 't';
            return 2;
        case '\n':
            *p++ = '\\';
            *p++ = 'n';
            return 2;
        case '\v':
            *p++ = '\\';
            *p++ = 'v';
            return 2;
        case '\f':
            *p++ = '\\';
            *p++ = 'f';
            return 2;
        case '\r':
            *p++ = '\\';
            *p++ = 'r';
            return 2;
        case '\\':
            *p++ = '\\';
            *p++ = '\\';
            return 2;
    }

    unsigned char byte = c;
    *p++ = '\\';
    *p++ = hexlut[(byte >> 4) & 0x0F];
    *p++ = hexlut[(byte >> 0) & 0x0F];
    return 3;
}

int str_escape_nonprint(char *dst, size_t dstsize, const char *src,
                        size_t srcsize)
{
    int totlen = 0;
    while (1) {

        if (!srcsize)
            break;

        if (dstsize < 4)
            break;

        int len = _escape_nonprint_char(*src, dst);
        dst += len;
        dstsize -= len;

        src += 1;
        srcsize -= 1;

        totlen += len;
    }

    *dst = '\0';
    return totlen;
}

/* or use ts from moreutils? `apt install moreutils`
 *
 *  comand | ts '[%Y-%m-%d %H:%M:%S]'
 */

#define STR_ISO8601_SHORT_SIZE (sizeof("19700101T010203Z.123456789") + 2)
//#define STR_ISO8601_LONG_SIZE (sizeof("1970-01-01T01:02:03Z.123456789") + 2)
int str_iso8601_short(char *dst, size_t size)
{
    int len = 0;

    if (size < STR_ISO8601_SHORT_SIZE)
        return -ENOSPC;

    /* gettimeofday marked obsolete in POSIX.1-2008 and recommends
     * ... or use uv_gettimeofday?
     */
    struct timespec now;
#if 0 // TODO use uv_gettimeofday - more portable
    uv_timeval64_t uvtv;
    int err = uv_gettimeofday(&uvtv);
    if (err) {
        LOG_ERR("uv_gettimeofday err=%d", err);
        return;
    }
    now.tv_sec = uvtv.tv_sec;
    now.tv_nsec = uvtv.tv_usec * 1000;

#else
    int err = clock_gettime(CLOCK_REALTIME, &now);
    if (err) {
        // should not occur.
        return -errno;
    }
#endif
    struct tm tm;
    gmtime_r(&now.tv_sec, &tm);

    size_t rsz = strftime(dst, size, "%Y%m%dT%H%M%S", &tm);
    if (rsz == 0) {
        // content of buf undefined on zero
        *dst = '\0';
        return -1;
    }

    assert(rsz < size);
    size -= rsz;
    dst += rsz;
    len += rsz;

    // do not have nanosecond precision
    int rc = snprintf(dst, size, ".%03luZ: ", now.tv_nsec / 1000000);
    if (rc < 0) {
        return -1;
    }
    else if (rc >= size) {
        return -ENOSPC;
    }

    len += rc;

    return len;
}
