#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "opt_argviter.h"

#ifndef ARRAY_LEN
#define ARRAY_LEN(ARRAY) (sizeof(ARRAY) / sizeof((ARRAY)[0]))
#endif

/** single char string to point for short options.
 * no malloc or modifications to argv */
static struct {
    char c;
    char ___nul;
} g_shortstr[] =
{
    [ 0] = {'0'}, [ 1] = {'1'}, [ 2] = {'2'}, [ 3] = {'3'},
    [ 4] = {'4'}, [ 5] = {'5'}, [ 6] = {'6'}, [ 7] = {'7'},
    [ 8] = {'8'}, [ 9] = {'9'}, [17] = {'A'}, [18] = {'B'},
    [19] = {'C'}, [20] = {'D'}, [21] = {'E'}, [22] = {'F'},
    [23] = {'G'}, [24] = {'H'}, [25] = {'I'}, [26] = {'J'},
    [27] = {'K'}, [28] = {'L'}, [29] = {'M'}, [30] = {'N'},
    [31] = {'O'}, [32] = {'P'}, [33] = {'Q'}, [34] = {'R'},
    [35] = {'S'}, [36] = {'T'}, [37] = {'U'}, [38] = {'V'},
    [39] = {'W'}, [40] = {'X'}, [41] = {'Y'}, [42] = {'Z'},
    [49] = {'a'}, [50] = {'b'}, [51] = {'c'}, [52] = {'d'},
    [53] = {'e'}, [54] = {'f'}, [55] = {'g'}, [56] = {'h'},
    [57] = {'i'}, [58] = {'j'}, [59] = {'k'}, [60] = {'l'},
    [61] = {'m'}, [62] = {'n'}, [63] = {'o'}, [64] = {'p'},
    [65] = {'q'}, [66] = {'r'}, [67] = {'s'}, [68] = {'t'},
    [69] = {'u'}, [70] = {'v'}, [71] = {'w'}, [72] = {'x'},
    [73] = {'y'}, [74] = {'z'},
};

static char *to_shortstr(char c)
{
    unsigned int i = c;
    i -= '0';
    assert(i < ARRAY_LEN(g_shortstr));
    char *s = &g_shortstr[i].c;
    assert(s[0]);
    return s;
}

static inline int parse_error(struct opt_argviter_ctx *p, const char *msg)
{
    //fprintf(stderr, "error: %s\n", msg);
    p->errmsg = msg;
    return -EINVAL;
}

static inline void reset_tmp_states(struct opt_argviter_ctx *p)
{
    p->dashes = 0;
    p->key = NULL;
    p->key_len = 0;
    p->val = NULL;
    p->val_len = 0;
}

static inline int load_next_argv(struct opt_argviter_ctx *p)
{
    int i = p->argi + 1;
    if (i >= p->argc)
        return EOF;

    p->sp = p->argv[i];
    p->argi = i;

    _Static_assert(EOF != 0, "");

    return 0;
}

static inline void parse_dashes(struct opt_argviter_ctx *p)
{
    while(1) {
        char c = *p->sp;
        if (c == '-')
            p->dashes++;
        else
            break;
        p->sp++;
    }

    if (*p->sp != '\0')
        p->key = p->sp;
    else
        p->key = NULL;
}

static inline int parse_short_key(struct opt_argviter_ctx *p,
                                 struct opt_argviter_out *out)
{
    assert(*p->sp != '\0');
    p->key = p->sp;
    p->sp++;

    out->name = to_shortstr(p->key[0]);
    return 0;
}

static inline int parse_short_value(struct opt_argviter_ctx *p)
{
    assert(p->dashes == 1);
    if (*p->sp == '\0') {
        if (load_next_argv(p) == EOF)
            return parse_error(p, "requires an argument");
        assert(*p->sp != '\0'); 
    }

    p->val = p->sp;

    return 0;
}

static inline int parse_long_key(struct opt_argviter_ctx *p,
                                 struct opt_argviter_out *out)

{
    assert(*p->sp != '\0');
    p->key = p->sp;

    while(1) {
        char c = *p->sp;
        if (c == '=') {
            *p->sp++ = '\0';
            p->val = p->sp;
            break;
        }
        if (c == '\0') {
            p->val = NULL; // load next later
            break;
        }
        p->sp++;
        p->key_len++;
    }

    out->name = p->key;
    return 0;
}

// should only run if value expected
static inline int parse_long_value(struct opt_argviter_ctx *p)
{
    assert(p->dashes == 2);
    // if had `--foo 123`
    if (!p->val) {
        if (load_next_argv(p) == EOF)
            return parse_error(p, "requires an argument");
        p->val = p->sp;
    }
    // else had `--foo=123`
    assert(p->val);
    // note `--foo=""` is accepted and is a empty string
    assert(p->val == p->sp);

    if (*p->sp == '-') {
        // TODO
    }
    while(1) {
        char c = *p->sp++;
        if (c == '\0')
            break;

        p->val_len++;
    }

    return 0;
}

static inline int parse_positional(struct opt_argviter_ctx *p,
                                   struct opt_argviter_out *out)
{
    assert(*p->sp != '\0');
    p->key = p->sp;
    out->name = p->key;

    return 0;
}

int opt_argviter_init(struct opt_argviter_ctx *p,
                     int flags, int argc, char *argv[])
{
    p->argc = argc;
    p->argv = argv;
    p->argi = 0;
    return 0;
}

int opt_argviter_getkey(struct opt_argviter_ctx *p)
{
    struct opt_argviter_out *out = &p->out;
    int err;
    // had several flags a, b, c  combined. i.e '-abc'
    if (p->dashes == 1) {
        if (*p->sp != '\0') {
            /* setting user opt dashes to 1 to indicate short option even
             * though it is stricly not true */
            *out = (struct opt_argviter_out) {
                .dashes = 1,
                .name = to_shortstr(*p->sp),
                .val = NULL,
            };
            p->sp++; // next flag
            return 0;
        }
    }

    *out = (struct opt_argviter_out) { 0 }; // reset
    reset_tmp_states(p);

    if (load_next_argv(p) == EOF)
        return OPT_ARGVITER_DONE;

    parse_dashes(p);

    switch (p->dashes) {
        case 0:
            if (!p->key)
                return parse_error(p, "no leading dash");

            err = parse_positional(p, out);
            break;
        case 1:
            if (!p->key)
                return parse_error(p, "single dash only");

            err = parse_short_key(p, out);
            break;

        case 2:
            if (!p->key)
                return OPT_ARGVITER_DONE;

            err = parse_long_key(p, out);
            break;

        default:
            return parse_error(p, "to many dashes");
    }
    if (err)
        return err;

    out->dashes = p->dashes;

    return 0;
}

int opt_argviter_getval(struct opt_argviter_ctx *p)
{
    struct opt_argviter_out *out = &p->out;
    assert(out->name); // no prior call to getkey
    assert(out->dashes == p->dashes);
    int err;
    switch (p->dashes) {
        case 0:
#if 1
            err = parse_error(p, "positional value not supported");
#else
            p->val = p->key;
            err = 0;
#endif
            break;
        case 1:
            err = parse_short_value(p);
            break;
        case 2:
            err = parse_long_value(p);
            break;
        default:
            assert(0);
            return -1;
    }

    if (err)
        return err;

    assert(p->val);
    out->val = p->val;

    p->dashes = 0; // do not read more flags
    return 0;
}
