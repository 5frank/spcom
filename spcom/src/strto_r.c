#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include "str.h"
#include "strto_r.h"

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
        return STR_EINVAL;
    }

    // stroul ignores sign or silently overflow
    if (_first_nonspace(s) == '-') {
        return STR_EINVAL;
    }

    char *tmp_ep = NULL;
    int err = ___errno_reset();
    unsigned long int tmp = strtoul(s, &tmp_ep, base);
    err = ___errno_restore(err);
    if (err) {
        return (err == ERANGE) ? STR_ERANGE : STR_EUNKNOWN;
    }

    if (tmp_ep == s) {
        return STR_ENAN;
    }

    if (ep) {
        *ep = tmp_ep;
    }
    else if (!_is_valid_endc(*tmp_ep)) {
        return STR_EEND;
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
        return STR_EINVAL;
    }

    char *tmp_ep = NULL;
    int err = ___errno_reset();
    long int tmp = strtol(s, &tmp_ep, base);
    err = ___errno_restore(err);
    if (err) {
        return (err == ERANGE) ? STR_ERANGE : STR_EUNKNOWN;
    }

    if (tmp_ep == s) {
        return STR_ENAN;
    }

    if (ep) {
        *ep = tmp_ep;
    }
    else if (!_is_valid_endc(*tmp_ep)) {
        return STR_EEND;
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
    int err = ___errno_reset();
    float tmp = strtof(s, &tmp_ep);
    err = ___errno_restore(err);
    if (err) {
        return (err == ERANGE) ? STR_ERANGE : STR_EUNKNOWN;
    }

    if (tmp_ep == s) {
        return STR_ENAN;
    }

    if (isinf(tmp) || isnan(tmp)) {
        return STR_EINVAL;
    }

    if (ep) {
        *ep = tmp_ep;
    }
    else if (!_is_valid_endc(*tmp_ep)) {
        return STR_EEND;
    }

    *res = tmp;

    return 0;
}

