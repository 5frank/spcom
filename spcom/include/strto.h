#ifndef STRTO_INCLUDE_H_
#define STRTO_INCLUDE_H_

#include <errno.h>
#include <limits.h>

/**
 * @brief error value to string.
 * return NULL if unknown error
 */
const char *strto_strerror(int err);

/**
 * @defgroup strto_<type> functions:
 * @{
 * @details
 *     handles all checks on errno and end pointer that posix strto family
 *     requires.
 *
 * @param s - input string. leading spaces (as defined by `isspace) ignored.
 *
 * @param base - either 0, 10 or 16.
 *
 * @param ep - end pointer (optional):
 * - If NULL internal end pointer is used and checks are made that it has a
 *   sane value (points to nul byte ('\0') or isspace()).
 * - If not NULL - only internal check that ep is set to end of input, but no
 *   checks on the value of `*ep` That is, the error case as indicated by the
 *   return value `-ENOTSUP` is up to the caller.
 *
 * @param res - result. Only modified if return value is zero.
 *
 * @return zero on success or negative errno:
 * - `-ECANCELED` The string did not contain any characters that were
 *   converted.
 *
 * - `-EINVAL` Invalid base value or NULL argument for input string or result
 *   pointer. This indicate a error from the caller.
 *
 * - `-ENOTSUP` Unexpected character at end of input. The string contained
 *   non-numeric characters that did not get converted.
 *
 * - `-ERANGE` The given string was out of range and result too large for type.
 *   i.e. overflow.  Or input string contains a negative number and result is
 *   of unsigned type.
 *
 * - `-EBADF` Abnormal float or double. i.e. `NAN` or `INF`. Could be read as
 *   "error bad float"
 *
 * - `-EDOM` Out of domain.
 *
 * @note errnos used are same as for the BSD functions `strtoi()` and
 * `strtou()` with the following differences:
 *  - The BSD function uses positive errno values.
 *  - The BSD function do not use EBADF.
 *
 *
 * @note: There is also errno EOVERFLOW that one might seem more suitable then
 *   ERANGE, but as strto* is expected to set errno to ERANGE on overflow it
 *   arguably more correct.
 *
 * @note One could argue that if leading spaces is accepted input than
 *   so should trailing spaces.
 */

int strto_l(const char *s, const char **ep, int base, long int *res);
int strto_ul(const char *s, const char **ep, int base, unsigned long int *res);

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
___STRTO_L_WRAPPER_DEFINE(strto_sc, signed char, SCHAR_MIN, SCHAR_MAX)
___STRTO_L_WRAPPER_DEFINE(strto_i8, int8_t, INT8_MIN, INT8_MAX)
___STRTO_L_WRAPPER_DEFINE(strto_i16, int16_t, INT16_MIN, INT16_MAX)
___STRTO_L_WRAPPER_DEFINE(strto_i32, int32_t, INT32_MIN, INT32_MAX)
___STRTO_L_WRAPPER_DEFINE(strto_i64, int64_t, INT64_MIN, INT64_MAX)

/** check if char signed */
#if (CHAR_MIN < 0)
___STRTO_L_WRAPPER_DEFINE(strto_c, char, CHAR_MIN, CHAR_MAX)
#endif

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

#if (CHAR_MIN == 0)
___STRTO_UL_WRAPPER_DEFINE(strto_c, char, CHAR_MAX)
#endif

#undef ___STRTO_UL_WRAPPER_DEFINE

/**
 * @param naninf check for NAN and/or INFINITY. ex. whatever "nan",
 * "-inf" etc is accepted or not.
 *  if 0 (0b00) both NAN and INFINITY result in non-zero return value
 *  if 1 (0b01) NAN allowed
 *  if 2 (0b10) INFINITY allowed
 *  if 3 (0b11) both NAN and INFINITY allowed
 */
int strto_f(const char *s, const char **ep, int naninf, float *res);

int strto_d(const char *s, const char **ep, int naninf, double *res);

/**
 * @}
 */

#endif
