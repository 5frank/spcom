#ifndef STRTO_R_INCLUDE_H_
#define STRTO_R_INCLUDE_H_

#include <errno.h>
#include <limits.h>

/**
 * @brief error value to string.
 * return NULL if unkown error
 */
const char *strto_strerror(int err);
/**
 * @defgroup strto<type>_r functions:
 * @{
 * @details
 *     handles all checks on errno and end pointer that posix strto family
 *     requires.
 *
 *
 *
 * @param base - either 0, 10 or 16.
 *
 * @param ep - endpointer is optional, if not provided, internal
 *    endpointer used and checks are made that it has a sane value (points to
 *    nul byte ('\0') or isspace()).
 * @param res result. Not modified on failure (non zero return value)
 * @return zero on success
 */

int strto_ul(const char *s, const char **ep, int base, unsigned long int *res);
int strto_l(const char *s, const char **ep, int base, long int *res);

/**
 * @param naninf check for NAN and/or INFINITY. ex. whatever "nan",
 * "-inf" etc is accepted or not.
 *  if 0 (0b00) both NAN and INFINITY result in non-zero return value
 *  if 1 (0b01) NAN allowed
 *  if 2 (0b10) INFINITY allowed
 *  if 3 (0b11) both NAN and INFINITY allowed
 */
int strto_f(const char *s, const char **ep, int naninf, float *res);

#define ___STRTO_L_WRAPPER_DEFINE(NAME, TYPE, TYPE_MIN, TYPE_MAX)              \
static inline int NAME(const char *s, const char **ep, int base, TYPE *res)    \
{                                                                              \
    long int tmp = 0;                                                          \
    int err = strto_l(s, ep, base, &tmp);                                      \
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

___STRTO_L_WRAPPER_DEFINE(strto_i, int, INT_MIN, INT_MAX)
___STRTO_L_WRAPPER_DEFINE(strto_i8, int8_t, INT8_MIN, INT8_MAX)
___STRTO_L_WRAPPER_DEFINE(strto_i16, int16_t, INT16_MIN, INT16_MAX)
___STRTO_L_WRAPPER_DEFINE(strto_i32, int32_t, INT32_MIN, INT32_MAX)
___STRTO_L_WRAPPER_DEFINE(strto_i64, int64_t, INT64_MIN, INT64_MAX)

#undef ___STRTO_L_WRAPPER_DEFINE

#define ___STRTO_UL_WRAPPER_DEFINE(NAME, TYPE, TYPE_MAX)                       \
static inline int NAME(const char *s, const char **ep, int base, TYPE *res)    \
{                                                                              \
    unsigned long int tmp = 0;                                                 \
    int err = strto_ul(s, ep, base, &tmp);                                     \
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

___STRTO_UL_WRAPPER_DEFINE(strto_ui, unsigned int, UCHAR_MAX)
___STRTO_UL_WRAPPER_DEFINE(strto_uc, unsigned char, UCHAR_MAX)
___STRTO_UL_WRAPPER_DEFINE(strto_u8, uint8_t, UINT8_MAX)
___STRTO_UL_WRAPPER_DEFINE(strto_u16, uint16_t, UINT16_MAX)
___STRTO_UL_WRAPPER_DEFINE(strto_u32, uint32_t, UINT32_MAX)
___STRTO_UL_WRAPPER_DEFINE(strto_u64, uint64_t, UINT64_MAX)

#undef ___STRTO_UL_WRAPPER_DEFINE

/**
 * @}
 */

#endif
