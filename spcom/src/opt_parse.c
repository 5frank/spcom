#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
// local
#include "assert.h"
#include "opt.h"
#include "strto_r.h"

#ifndef IS_ALIGNED
#define IS_ALIGNED(PTR, SIZE) (((uintptr_t)(PTR) % (SIZE)) == 0)
#endif

int opt_parse_int(const struct opt_conf *conf, char *s)
{
    assert(conf->dest);
    assert(IS_ALIGNED(conf->dest, sizeof(int)));

    int err = strto_i(s, NULL, 0, (int *)conf->dest);
    if (err) {
        return opt_perror(conf, "invalid integer");
    }

    return 0;
}

int opt_parse_uint(const struct opt_conf *conf, char *s)
{
    assert(conf->dest);
    assert(IS_ALIGNED(conf->dest, sizeof(unsigned int)));

    int err = strto_ui(s, NULL, 0, (unsigned int *)conf->dest);
    if (err) {
        return opt_perror(conf, "invalid positive integer");
    }

    return 0;
}

int opt_parse_float(const struct opt_conf *conf, char *s)
{
    assert(conf->dest);
    assert(IS_ALIGNED(conf->dest, sizeof(float)));

    int err = strto_f(s, NULL, 0, (float *)conf->dest);
    if (err) {
        return opt_perror(conf, "invalid float");
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
