#ifndef STRTO_R_INCLUDE_H_
#define STRTO_R_INCLUDE_H_

#include <limits.h>

/**
 * strto{type}_r functions:
 *
 * handles all checks on errno and endpointer that posix strto family requires.
 *    @param ep - endpointer `const char **ep` is optional, if NULL, used
 *    internal endpointer and check that it points to nul byte ('\0') or isspace().
 *    @return zero on success
 */

int strtoul_r(const char *s, const char **ep, int base, unsigned long int *res);
int strtol_r(const char *s, const char **ep, int base, long int *res);
int strtof_r(const char *s, const char **ep, float *res);

#define ___STRTOL_R_WRAPPER_DEFINE(NAME, TYPE, TYPE_MIN, TYPE_MAX)             \
static inline int NAME(const char *s, const char **ep, int base, TYPE *res)    \
{                                                                              \
    long int tmp = 0;                                                          \
    int err = strtol_r(s, ep, base, &tmp);                                     \
    if (err) {                                                                 \
        return err;                                                            \
    }                                                                          \
                                                                               \
    if (tmp < TYPE_MIN || tmp > TYPE_MAX) {                                    \
        return STR_ERANGE;                                                     \
    }                                                                          \
                                                                               \
    *res = tmp;                                                                \
                                                                               \
    return 0;                                                                  \
}

___STRTOL_R_WRAPPER_DEFINE(strtoi_r, int, INT_MIN, INT_MAX)
___STRTOL_R_WRAPPER_DEFINE(strtoi8_r, int8_t, INT8_MIN, INT8_MAX)
___STRTOL_R_WRAPPER_DEFINE(strtoi16_r, int16_t, INT16_MIN, INT16_MAX)
___STRTOL_R_WRAPPER_DEFINE(strtoi32_r, int32_t, INT32_MIN, INT32_MAX)

#undef ___STRTOL_R_WRAPPER_DEFINE

#define ___STRTOUL_R_WRAPPER_DEFINE(NAME, TYPE, TYPE_MAX)                      \
static inline int NAME(const char *s, const char **ep, int base, TYPE *res)    \
{                                                                              \
    unsigned long int tmp = 0;                                                 \
    int err = strtoul_r(s, ep, base, &tmp);                                    \
    if (err) {                                                                 \
        return err;                                                            \
    }                                                                          \
                                                                               \
    if (tmp > TYPE_MAX) {                                                      \
        return -ERANGE;                                                        \
    }                                                                          \
                                                                               \
    *res = tmp;                                                                \
                                                                               \
    return 0;                                                                  \
}

___STRTOUL_R_WRAPPER_DEFINE(strtouc_r, unsigned char, UCHAR_MAX)
___STRTOUL_R_WRAPPER_DEFINE(strtou8_r, uint8_t, UINT8_MAX)
___STRTOUL_R_WRAPPER_DEFINE(strtou16_r, uint16_t, UINT16_MAX)
___STRTOUL_R_WRAPPER_DEFINE(strtou32_r, uint32_t, UINT32_MAX)

#undef ___STRTOUL_R_WRAPPER_DEFINE

#endif
