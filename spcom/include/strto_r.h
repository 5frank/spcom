#ifndef STRTO_R_INCLUDE_H_
#define STRTO_R_INCLUDE_H_

#include <errno.h>
#include <limits.h>

/**
 * @brief error value to string.
 * return NULL if unkown error
 */
const char *strto_r_strerror(int err);
/**
 * @defgroup strto<type>_r functions:
 * @{
 * @details
 *     handles all checks on errno and end pointer that posix strto family
 *     requires.
 * @param ep - endpointer is optional, if not provided, internal
 *    endpointer used and checks are made that it has a sane value (points to
 *    nul byte ('\0') or isspace()).
 * @param res result. Not modified on failure (non zero return value)
 * @return zero on success
 */

int strtoul_r(const char *s, const char **ep, int base, unsigned long int *res);
int strtol_r(const char *s, const char **ep, int base, long int *res);
/**
 * @param naninf check for NAN and/or INFINITY. ex. whatever "nan",
 * "-inf" etc is accepted or not.
 *  if 0 (0b00) both NAN and INFINITY result in non-zero return value
 *  if 1 (0b01) NAN allowed
 *  if 2 (0b10) INFINITY allowed
 *  if 3 (0b11) both NAN and INFINITY allowed
 */
int strtof_r(const char *s, const char **ep, int naninf, float *res);

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
        return -ERANGE;                                                        \
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
___STRTOL_R_WRAPPER_DEFINE(strtoi64_r, int64_t, INT64_MIN, INT64_MAX)

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

___STRTOUL_R_WRAPPER_DEFINE(strtoui_r, unsigned int, UCHAR_MAX)
___STRTOUL_R_WRAPPER_DEFINE(strtouc_r, unsigned char, UCHAR_MAX)
___STRTOUL_R_WRAPPER_DEFINE(strtou8_r, uint8_t, UINT8_MAX)
___STRTOUL_R_WRAPPER_DEFINE(strtou16_r, uint16_t, UINT16_MAX)
___STRTOUL_R_WRAPPER_DEFINE(strtou32_r, uint32_t, UINT32_MAX)
___STRTOUL_R_WRAPPER_DEFINE(strtou64_r, uint64_t, UINT64_MAX)

#undef ___STRTOUL_R_WRAPPER_DEFINE

/**
 * @}
 */

#endif
