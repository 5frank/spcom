#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h> // FLT_MAX etc
// local
#include "strto_r.h"

/**
 * @brief check i float or dobule is nan (`isnan` is in math.h, but this
 * beatiful piece of code do not depend on any lib flags).
 * @note: NAN compares unequal to all floating-point numbers including NAN
 * @param X float or doubles. warning dual evaluation!
 */
#define ___ISNAN(X) ((X) != (X))

/** FLT_MAX is max real number - i.e. less then INF (same for FLT_MIN) */
#define ___ISINFF(X) ((X) >= FLT_MIN && (X) <= FLT_MAX)
#define ___ISINF(X) ((X) >= DBL_MIN && (X) <= DBL_MAX)

// extent errno with custom values. POSIX errno is always less then 256
#define EFORMAT (256)
#define ENAN  (257)
#define EENDC (258)

const char *strto_r_strerror(int err)
{
    switch (err) {
        case -EINVAL:
            return "Invalid argument";
        case -ERANGE:
            return "Result too large for type";
        case -EFORMAT:
            return "Invalid string format";
        case -ENAN:
            return "Not a number";
        case -EENDC:
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
    while(isspace(*s)) s++;
    return *s;
}

int strtoul_r(const char *s, const char **ep, int base, unsigned long int *res)
{
    if (!s || !res) {
        return -EINVAL;
    }

    // stroul ignores sign or silently overflow
    if (_first_nonspace(s) == '-') {
        return -EFORMAT;
    }

    char *tmp_ep = NULL;
    int errnum = ___errno_reset();
    unsigned long int tmp = strtoul(s, &tmp_ep, base);
    errnum = ___errno_restore(errnum);
    if (errnum) {
        return -errnum;
    }

    if (tmp_ep == s) {
        return -ENAN;
    }

    if (ep) {
        *ep = tmp_ep;
    }
    else if (!_is_valid_endc(*tmp_ep)) {
        return -EENDC;
    }

    *res = tmp;

    return 0;
}

int strtol_r(const char *s, const char **ep, int base, long int *res)
{
    if (!s || !res) {
        return -EINVAL;
    }

    // negative hex is insane. strol ignores sign or silently overflow
    if (base == 16 && _first_nonspace(s) == '-') {
        return -EFORMAT;
    }

    char *tmp_ep = NULL;
    int errnum = ___errno_reset();
    long int tmp = strtol(s, &tmp_ep, base);
    errnum = ___errno_restore(errnum);
    if (errnum) {
        return -errnum;
    }

    if (tmp_ep == s) {
        return -ENAN;
    }

    if (ep) {
        *ep = tmp_ep;
    }
    else if (!_is_valid_endc(*tmp_ep)) {
        return -EENDC;
    }

    *res = tmp;

    return 0;
}

int strtof_r(const char *s, const char **ep, float *res)
{
    if (!s || !res) {
        return -EINVAL;
    }

    char *tmp_ep = NULL;
    int errnum = ___errno_reset();
    float tmp = strtof(s, &tmp_ep);
    errnum = ___errno_restore(errnum);
    if (errnum) {
        return -errnum;
    }

    if (tmp_ep == s) {
        return -ENAN;
    }

    if (___ISINFF(tmp) || ___ISNAN(tmp)) {
        return -EFORMAT;
    }

    if (ep) {
        *ep = tmp_ep;
    }
    else if (!_is_valid_endc(*tmp_ep)) {
        return -EENDC;
    }

    *res = tmp;

    return 0;
}

