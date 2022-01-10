#ifndef OPT_INCLUDE_H_
#define OPT_INCLUDE_H_
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <ctype.h>
#include "opt.h"

#ifndef ARRAY_LEN
#define ARRAY_LEN(ARRAY) (sizeof(ARRAY) / sizeof((ARRAY)[0]))
#endif

/** option has a no value argument. i.e. flag **/
#define OPT_F_NO_VAL      (1 << 0)
/** stritct parsing option. only accept hexadecimal input for integers */
#if 0
#define OPT_F_HEX_ARG      (1 << 1)
/** only accept decimal input for integers */
#define OPT_F_DEC_ARG      (1 << 2)
/**
 * stritct parsing option. only accept NUL ('\0') at end of numeric value.
 * default behavior checks if this charachter is either NUL or isspace(c) is
 * non-zero */
#define OPT_F_NUL_EP       (1 << 3)
/** Do not test parsed floating point values with `isnormal(X)`
 * (see man page for `isnomral`for more info) */
#define OPT_F_NO_ISNORMAL  (1 << 4)
#endif


struct opt_conf;

typedef const char **(opt_complete_fn)(const char *s);
typedef int (opt_parse_fn)(const struct opt_conf *conf, char *sval);

struct opt_conf {
    const char *name;
    opt_parse_fn *parse;
    opt_complete_fn *complete;
    void *dest; // write pointer
    const char *metavar;
    const char *descr;
    const char *alias;
    char shortname;
    uint8_t flags;
    uint8_t positional;
    uint8_t ___reserved;// pad
};

/// inlcude depends on sizeof(struct opt_conf)
#include "opt_section.h"

/**
 * @param startstr only match words starting with startstr. can be NULL.
 * @return NULL or NULL terminated list of autocomplete strings.
 */
const char **opt_autocomplete(const char *name, const char *startstr);

/** set error message. always return non-zero i.e. can be used in return
 * statements.
 */
int opt_error(const struct opt_conf *conf, const char *msg);

int opt_parse_args(int argc, char *argv[]);

int opt_show_help(const char *s);

int opt_ap_int(const struct opt_conf *conf, char *s);

int opt_ap_str(const struct opt_conf *conf, char *s);
/** set conf->dest to true.
 * implies no arg value and assumes conf->dest points to a int
 */
int opt_ap_flag_true(const struct opt_conf *conf, char *s);

/** set conf->dest to false.
 * implies no arg value and assumes conf->dest points to a int
 */
int opt_ap_flag_false(const struct opt_conf *conf, char *s);

/** set conf->dest += 1 .
 * implies no arg value and assumes conf->dest points to a int
 */
int opt_ap_flag_count(const struct opt_conf *conf, char *s);
#endif

