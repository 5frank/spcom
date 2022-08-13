#ifndef STR_INCLUDE_H__
#define STR_INCLUDE_H__
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/// unknown parse failure
#define STR_EUNKNOWN (300)
/// unexpected character at end of input string
#define STR_EEND (301)
/// string is not a number (no digits)
#define STR_ENAN (302)
/// invalid format (incorrect delimiter etc)
#define STR_EFMT (303)
/// string do not match 
#define STR_ENOMATCH (304)
// number out of allowed range. (same as ERANGE on most platforms)
#define STR_ERANGE (34)
// string not valid. (same as EINVAL on most platforms)
#define STR_EINVAL (22)
// something did not fit (same as E2BIG on most platforms)
#define STR_E2BIG  (7)

/**
 * string matchers:
 *    @param s if s is a empty string, all possible matches is returned.
 *    @return a NULL terminated list of strings beginning with `s`, or NULL if
 *    none found
 */

int str_to_bool(const char *s, bool *rbool);

int str_to_pinstate(const char *s, int *state);
const char **str_match_pinstate(const char *s);

int str_to_baud(const char *s, int *baud, const char **ep);
/// match common baudrates
const char **str_match_baud(const char *s);

int str_to_databits(const char *s, int *databits, const char **ep);

/// @param parity set to value used by libserialport
int str_to_parity(const char *s, int *parity, const char **ep);

int str_to_stopbits(const char *s, int *stopbits, const char **ep);

/**
 * parse baud and optional databits, parity and stopbits.
 *
 * example: 
 * "115200/8N1" --> baud=115200, databits=8, parity=NONE, stopbits=1
 * "9600" --> baud=9600 (remaning not set)
 * "10000:8E1" --> baud=10000, databits=8, parity=EVEN, stopbits=1
 * "115200/" --> error invalid format
 *
 *  result params only set on valid string format, otherwise untouched.
 *
 *  @param parity set to value used by libserialport
 */
int str_to_baud_dps(const char *s, int *baud, int *databits, int *parity, int *stopbits);

int str_to_xonxoff(const char *s, int *xonxoff);

/// macro to retreive `SP_FLOWCONTROL_...` value from `flowcontrol`
#define FLOW_TO_SP_FLOWCONTROL(X) ((X) & 0xff)
/// macro to retreive `SP_XONOFF_...` value from `flowcontrol`
#define FLOW_TO_SP_XONXOFF(X) (((X) >> 8) & 0xff)

int str_to_flowcontrol(const char *s, int *flowcontrol);
const char **str_match_flowcontrol(const char *s);

// simple destructive to argv parse. will not handle string escaped etc
//int str_to_argv(char *s, int *argc, char **argv, unsigned int max_argc);



char *str_lstrip(char *s);
char *str_rstrip(char *s);
char *str_strip(char *s);

char *str_lstripc(char *s, char c);
char *str_rstripc(char *s, char c);
char *str_stripc(char *s, char c);

static inline int str_startswith(const char *str, const char *prefix)
{
    return !strncmp(str, prefix, strlen(prefix));
}

static inline int str_casestartswith(const char *str, const char *prefix)
{
    return !strncasecmp(str, prefix, strlen(prefix));
}
#if 1

struct str_map {
    const char *key;
    union {
        const char *v_str;
        int v_int;
    } val;
};

#define STR_MAP_INT(KEY, VAL) { .key = (KEY), .val.v_int = (VAL) }
#define STR_MAP_STR(KEY, VAL) { .key = (KEY), .val.v_str = (VAL) }

const struct str_map *str_map_lookup(const char *s,
                                     const struct str_map *map,
                                     size_t map_len);

static inline int str_map_to_int(const char *s,
                                 const struct str_map *map,
                                 size_t map_len,
                                 int *v_int)
{

    const struct str_map *itm = str_map_lookup(s, map, map_len);
    if (!itm)
        return  -ENOENT;

    *v_int = itm->val.v_int;

    return 0;
}

static inline int str_map_to_str(const char *s,
                                 const struct str_map *map,
                                 size_t map_len,
                                 const char **v_str)
{

    const struct str_map *itm = str_map_lookup(s, map, map_len);
    if (!itm)
        return  -ENOENT;

    *v_str = itm->val.v_str;

    return 0;
}
/// string to string map item
struct str_kv {
    const char *key;
    const char *val;
};

#else
const struct str_kv *str_kv_lookup(const char *s, const struct str_kv *map, size_t n);

/// string to int map item
struct str_kvi {
    const char *key;
    int val;
};
const struct str_kvi *str_kvi_lookup(const char *s, const struct str_kvi *map, size_t n);
int str_kvi_getval(const char *s, int *val, const struct str_kvi *map, size_t n);
#endif

int str_split(char *s, const char *delim, char *toks[], int *n);
int str_split_kv(char *s, struct str_kv *kv);
int str_split_kv_list(char *s, struct str_kv *kvlist, int *n);

int str_split_quoted(char *s, int *argc, char *argv[], int argvmax);

int str_escape_nonprint(char *dst, size_t dstsize,
                       const char *src, size_t srcsize);

#define STR_ISO8601_SHORT_SIZE (sizeof("19700101T010203Z.123456789") + 2)
int str_iso8601_short(char *dst, size_t size);

#endif
