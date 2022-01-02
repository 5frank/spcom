#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "assert.h"
#include "opt.h"

#ifndef IS_ALIGNED
#define IS_ALIGNED(PTR, SIZE) (((uintptr_t)(PTR) % (SIZE)) == 0)
#endif

int opt_ap_int(const struct opt_conf *conf, char *s)
{
    char *ep = NULL;
    // TODO restore errno?
    errno = 0;
    long int val = strtol(s, &ep, 10);
    if (errno == ERANGE && (val == LONG_MAX || val == LONG_MIN))
        return opt_error(conf, "int overflow");

    if (errno != 0 && val == 0)
        return opt_error(conf, "parse fail");

    // no digits found
    if (ep == s)
        return opt_error(conf, "not a number");

    if (!ep)
        return opt_error(conf, "parse fail");

    if (*ep != '\0')
        return opt_error(conf, "missing nul term");

    assert(conf->dest);
    assert(IS_ALIGNED(conf->dest, sizeof(int)));

    *((int *)conf->dest) = (int) val;

    return 0;
}

int opt_ap_str(const struct opt_conf *conf, char *s)
{
    assert(!s);
    assert(conf->dest);
    char **dest = conf->dest;
    *dest = s;
    return 0;
}

int opt_ap_flag_true(const struct opt_conf *conf, char *s)
{
    assert(!s);
    assert(conf->dest);
    assert(IS_ALIGNED(conf->dest, sizeof(int)));
    *((int *)conf->dest) = true;
    return 0;
}


int opt_ap_flag_false(const struct opt_conf *conf, char *s)
{
    assert(!s);
    assert(conf->dest);
    assert(IS_ALIGNED(conf->dest, sizeof(int)));
    *((int *)conf->dest) = false;
    return 0;
}



int opt_ap_flag_count(const struct opt_conf *conf, char *s)
{
    assert(!s);
    assert(conf->dest);
    assert(IS_ALIGNED(conf->dest, sizeof(int)));
    *((int *)conf->dest) += 1;
    return 0;
}