#include <ctype.h>
#include <errno.h>
#include <float.h> // FLT_MAX etc
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
// local
#include "strto.h"

/**
 * @brief check i float or dobule is nan (`isnan` is in math.h, but no
 * required linker flags is nice)
 *
 * @note: NAN compares unequal to all floating-point numbers including NAN
 * @param X float or doubles. warning dual evaluation!
 */
#define ___ISNAN(X) ((X) != (X))

/** FLT_MAX is max real number - i.e. less then INF (same for FLT_MIN) */
#define ___ISINFF(X) ((X) >= FLT_MIN && (X) <= FLT_MAX)
#define ___ISINF(X) ((X) >= DBL_MIN && (X) <= DBL_MAX)


/*
 * Notes on errors and implementations:
 *
 * According to strtol (Linux 5.10) man page errno can be set accordingly:
 * """
 * - EINVAL (not in C99) The given base contains an unsupported value.
 * - ERANGE The resulting value was out of range.
 * The implementation may also set errno to EINVAL in case no conversion was
 * performed (no digits seen, and 0 returned).
 * """
 *
 * That is, in C99 (and presumably C89 and elsewhere) strtoul, strtol might
 * silently fails and returns 0 if the base parameter is other then 0, 10 or
 * 16. Example `unsigned long int val = strtoul("0xff", &ep, 3);` - will in this case
 * not set errno, `val` is 0 and `ep` set to "xff" (tested with glibc 10.2.1).
 *
 *
 * The BSD functions `strtoi()` and `strtou()` have the following documented
 * error codes:
 * """
 * - [ECANCELED] The string did not contain any characters that were converted.
 *
 * - [EINVAL] The base is not between 2 and 36 and does not contain the special
 *   value 0.
 *
 * - [ENOTSUP] The string contained non-numeric characters that did not get
 *   converted. In this case, endptr points to the first unconverted character.
 *
 * - [ERANGE] The given string was out of range; the value converted has been
 *   clamped; or the range given was invalid, i.e.  lo > hi.
 * """
 *
 * Differences from BSD `strtoi()` and `strtou()`:
 * - error codes are negative errno values. (as in libuv, zepyr RTOS and other)
 *
 * - if endptr argument provided, valid last char at end of input string can be
 *   nul ('\0') or space (as defined by POSIX `isspace`). strto{i,u} only
 *   accepts nul. One could argue that if leading spaces is accepted input than
 *   so should trailing spaces.
 *
 */

#define ERR_EINVAL   -EINVAL
/** There is also errno `EOVERFLOW` that might seem more suitable, but all
 * legacy useage and all implementations of strto* will set errno to ERANGE on
 * overflow so ERANGE is arguably more correct */
#define ERR_OVERFLOW -ERANGE
/** EDOM -    Mathematics argument out of domain of function (POSIX.1, C99) */
#define ERR_NEGATIVE -EDOM

/**  ECANCELED Operation canceled (POSIX.1-2001). conform to BSD usage */
#define ERR_NONUMERIC -ECANCELED
/** ENOTSUP Operation not supported (POSIX.1-2001). Conform to BSD usage */
#define ERR_ENDCHAR   -ENOTSUP
/** EBADF Bad file descriptor. "Creative" use of errnos - Could be read as
 * "error bad float" */
#define ERR_ABNORMAL  -EBADF

const char *strto_strerror(int err)
{
    switch (err) {
        case ERR_EINVAL:
            return "Invalid argument";
        case ERR_OVERFLOW:
            return "Result too large for type";
        case ERR_ABNORMAL:
            return "Abnormal value";
        case ERR_NEGATIVE:
            return "Negative value";
        case ERR_NONUMERIC:
            return "No numeric digits found";
        case ERR_ENDCHAR:
            return "Unexpected character at end of input";
        default:
            return NULL;
    }
}

static inline int ___errno_reset(void)
{
    int ret = errno;
    errno = 0;
    return ret;
}

static inline int ___errno_restore(int errnum)
{
    int ret = errno;
    errno = errnum;
    return ret;
}

static inline int _is_valid_base(int base)
{
    return (base == 0 || base == 10 || base == 16);
}

/** returns true if c is a "sane" character after a number
 */
static inline int _is_valid_endc(char c)
{
    return (c == '\0') || isspace(c);
}

static inline int _have_0x_prefix(const char *s)
{
    return (s[0] == '0' && s[1] == 'x');
}

static char _first_nonspace(const char *s)
{
    while (isspace(*s))
        s++;
    return *s;
}

static inline int _post_check(const char *s, const char **ep,
                              char *tmp_ep, int errnum)
{
    /* check endptr first as errno _might_ be set to EINVAL if no digits and
     * we want to distinguish this error case */
    if (tmp_ep == s) {
        return ERR_NONUMERIC;
    }

    if (errnum) {
        if (errnum == ERANGE) {
            return ERR_OVERFLOW;
        }
        // this should not occur
        return -errnum;
    }

    if (tmp_ep == s) {
        return ERR_NONUMERIC;
    }

    if (ep) {
        *ep = tmp_ep;
    }
    else if (!_is_valid_endc(*tmp_ep)) {
        return ERR_ENDCHAR;
    }

    return 0;
}

int strto_ul(const char *s, const char **ep, int base, unsigned long int *res)
{
    if (!s || !res || !_is_valid_base(base)) {
        return ERR_EINVAL;
    }

    // stroul ignores sign or silently overflow
    if (_first_nonspace(s) == '-') {
        return ERR_NEGATIVE;
    }

    char *tmp_ep = NULL;
    int errnum = ___errno_reset();
    unsigned long int tmp = strtoul(s, &tmp_ep, base);
    errnum = ___errno_restore(errnum);

    int err = _post_check(s, ep, tmp_ep, errnum);
    if (err) {
        return err;
    }

    *res = tmp;

    return 0;
}

int strto_l(const char *s, const char **ep, int base, long int *res)
{
    if (!s || !res || !_is_valid_base(base)) {
        return ERR_EINVAL;
    }
    // negative hex is insane. strol ignores sign or silently overflow
    if (base == 16 && _first_nonspace(s) == '-') {
        return ERR_NEGATIVE;
    }

    char *tmp_ep = NULL;
    int errnum = ___errno_reset();
    long int tmp = strtol(s, &tmp_ep, base);
    errnum = ___errno_restore(errnum);

    int err = _post_check(s, ep, tmp_ep, errnum);
    if (err) {
        return err;
    }

    *res = tmp;

    return 0;
}

int strto_f(const char *s, const char **ep, int naninf, float *res)
{
    if (!s || !res) {
        return ERR_EINVAL;
    }

    char *tmp_ep = NULL;
    int errnum = ___errno_reset();
    float tmp = strtof(s, &tmp_ep);
    errnum = ___errno_restore(errnum);

    int err = _post_check(s, ep, tmp_ep, errnum);
    if (err) {
        return err;
    }

    if (!(naninf & 1) && ___ISNAN(tmp)) {
        return ERR_ABNORMAL;
    }

    if (!(naninf & 2) && ___ISINFF(tmp)) {
        return ERR_ABNORMAL;
    }

    *res = tmp;

    return 0;
}
