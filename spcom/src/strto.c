#include <ctype.h>
#include <errno.h>
#include <float.h> // FLT_MAX etc
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
// local
#include "strto.h"

/// perhaps these belongs in header
#define STRTO_ACCEPT_NAN    (1 << 0)
#define STRTO_ACCEPT_INF    (1 << 1)

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
 * 16. Example `unsigned long int val = strtoul("0xff", &ep, 3);` - will in
 * this case not set errno, `val` is 0 and `ep` set to "xff" (tested with glibc
 * 10.2.1).
 *
 *
 * errnos used is similar to what the BSD functions `strtoi()` and `strtou()`
 * uses.
 */

/** EDOM - Mathematics argument out of domain of function (POSIX.1, C99) */
#define ERR_NEGATIVE -EDOM
/** EBADF Bad file descriptor. "Creative" use of errnos - Could be read as
 * "error bad float" */
#define ERR_ABNORMAL  -EBADF

const char *strto_strerror(int err)
{
    switch (err) {
        case -EINVAL:
            return "Invalid argument";
        case -ERANGE:
            return "Result too large for type";
        /*  ECANCELED Operation canceled. conform to BSD usage */
        case -ECANCELED:
            return "No numeric digits found";
        /* ENOTSUP Operation not supported. Conform to BSD usage */
        case -ENOTSUP:
            return "Unexpected character at end of input";
        // ==========
        case ERR_ABNORMAL:
            return "Abnormal value";
        case ERR_NEGATIVE:
            return "Negative value";
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
        return -ECANCELED;
    }

    if (errnum) {
        if (errnum == ERANGE) {
            return -ERANGE;
        }
        // this should not occur
        return -errnum;
    }

    if (ep) {
        *ep = tmp_ep;
    }
    else if (!_is_valid_endc(*tmp_ep)) {
        return -ENOTSUP;
    }

    return 0;
}

int strto_ul(const char *s, const char **ep, int base, unsigned long int *res)
{
    if (!s || !res || !_is_valid_base(base)) {
        return -EINVAL;
    }

    // stroul ignores sign or silently overflow
    if (_first_nonspace(s) == '-') {
        return -ERANGE;
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
        return -EINVAL;
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
        return -EINVAL;
    }

    char *tmp_ep = NULL;
    int errnum = ___errno_reset();
    float tmp = strtof(s, &tmp_ep);
    errnum = ___errno_restore(errnum);

    int err = _post_check(s, ep, tmp_ep, errnum);
    if (err) {
        return err;
    }

    if (!(naninf & STRTO_ACCEPT_NAN) && isnan(tmp)) {
        return ERR_ABNORMAL;
    }

    if (!(naninf & STRTO_ACCEPT_INF) && isinf(tmp)) {
        return ERR_ABNORMAL;
    }

    *res = tmp;

    return 0;
}

int strto_d(const char *s, const char **ep, int naninf, double *res)
{
    if (!s || !res) {
        return -EINVAL;
    }

    char *tmp_ep = NULL;
    int errnum = ___errno_reset();
    double tmp = strtod(s, &tmp_ep);
    errnum = ___errno_restore(errnum);

    int err = _post_check(s, ep, tmp_ep, errnum);
    if (err) {
        return err;
    }

    if (!(naninf & STRTO_ACCEPT_NAN) && isnan(tmp)) {
        return ERR_ABNORMAL;
    }

    if (!(naninf & STRTO_ACCEPT_INF) && isinf(tmp)) {
        return ERR_ABNORMAL;
    }

    *res = tmp;

    return 0;
}
