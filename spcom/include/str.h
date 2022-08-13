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
 * string match (completer) helpers:
 *    @param s if s is a empty string, all possible matches is returned.
 *    @return a NULL terminated list of strings beginning with `s`, or NULL if
 *    none found
 */

#define STR_MATCH_LIST(S, LIST) str_match_list(S, LIST, sizeof(LIST[0]), ARRAY_LEN(LIST))

const char **str_match_list(const char *s, const void *items, size_t itemsize, size_t numitems);

int str_to_bool(const char *s, bool *rbool);

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

static inline int str_list_indexof(const char *s, const char *list[], size_t listlen)
{
   for (int i = 0; i < listlen; i++) {
       if (!strcasecmp(s, list[i])) {
           return i;
       }
   }

   return -1;
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
