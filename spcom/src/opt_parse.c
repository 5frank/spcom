#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "strto_r.h"
#include "assert.h"
#include "opt.h"

#ifndef IS_ALIGNED
#define IS_ALIGNED(PTR, SIZE) (((uintptr_t)(PTR) % (SIZE)) == 0)
#endif

int opt_parse_int(const struct opt_conf *conf, char *s)
{
    assert(conf->dest);
    assert(IS_ALIGNED(conf->dest, sizeof(int)));

    int err = strtoi_r(s, NULL, 0, (int *)conf->dest);
    if (err) {
        const char *emsg = strto_r_strerror(err);
        return opt_error(conf, emsg ? emsg : "parse int failed");
    }

    return 0;
}

int opt_parse_uint(const struct opt_conf *conf, char *s)
{
    assert(conf->dest);
    assert(IS_ALIGNED(conf->dest, sizeof(unsigned int)));

    int err = strtoui_r(s, NULL, 0, (unsigned int *)conf->dest);
    if (err) {
        const char *emsg = strto_r_strerror(err);
        return opt_error(conf, emsg ? emsg : "parse uint failed");
    }

    return 0;
}

int opt_parse_float(const struct opt_conf *conf, char *s)
{
    assert(conf->dest);
    assert(IS_ALIGNED(conf->dest, sizeof(float)));

    int err = strtof_r(s, NULL, 0, (float *)conf->dest);
    if (err) {
        const char *emsg = strto_r_strerror(err);
        return opt_error(conf, emsg ? emsg : "parse float failed");
    }

    return 0;
}

int opt_parse_str(const struct opt_conf *conf, char *s)
{
    assert(s);
    assert(conf->dest);
    assert(IS_ALIGNED(conf->dest, sizeof(char *)));
    char **dest = conf->dest;
    *dest = s;
    return 0;
}

int opt_parse_flag_true(const struct opt_conf *conf, char *s)
{
    assert(!s);
    assert(conf->dest);
    assert(IS_ALIGNED(conf->dest, sizeof(int)));
    *((int *)conf->dest) = true;
    return 0;
}


int opt_parse_flag_false(const struct opt_conf *conf, char *s)
{
    assert(!s);
    assert(conf->dest);
    assert(IS_ALIGNED(conf->dest, sizeof(int)));
    *((int *)conf->dest) = false;
    return 0;
}



int opt_parse_flag_count(const struct opt_conf *conf, char *s)
{
    assert(!s);
    assert(conf->dest);
    assert(IS_ALIGNED(conf->dest, sizeof(int)));
    *((int *)conf->dest) += 1;
    return 0;
}
