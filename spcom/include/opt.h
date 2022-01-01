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

#ifndef STRINGIFY
#define STRINGIFY(X) ___STRINGIFY_VIA(X)
// Via expand macro. No parent
#define ___STRINGIFY_VIA(X) #X
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

struct opt_context;

struct opt_conf;

typedef const char **(opt_complete_fn)(const char *s);
typedef int (opt_parse_fn)(struct opt_context *ctx,
                         const struct opt_conf *conf,
                         char *sval);

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

struct opt_section_entry;

typedef int (*opt_post_parse_fn)(struct opt_context *ctx,
                                 const struct opt_section_entry *entry);
struct opt_section_entry {
    const char *name;
    const struct opt_conf *conf;
    size_t nconf;
    //const char *file;
    opt_post_parse_fn post_parse;
};

#define OPT_SECTION_ADD(NAME, CONF, NUM_CONF, POST_PARSE_CB)                   \
    __attribute((used, section("options")))                                    \
    static const struct opt_section_entry opt_conf_register_##NAME = {         \
        .name = STRINGIFY(NAME),                                               \
        .conf = CONF,                                                          \
        .nconf = NUM_CONF,                                                     \
        .post_parse = POST_PARSE_CB                                            \
    };

struct opt_context *opt_init(void);

/** set error message. always return non-zero i.e. can be used in return
 * statements.
 */
int opt_error(struct opt_context *ctx,
              const struct opt_conf *conf,
              const char *msg);

int opt_parse_args(struct opt_context *ctx, int argc, char *argv[]);


int opt_show_help(struct opt_context *ctx, const char *s);

int opt_ap_int(struct opt_context *ctx,
               const struct opt_conf *conf,
               char *s);

/// set conf->dest to true
int opt_ap_flag_true(struct opt_context *ctx,
               const struct opt_conf *conf,
               char *s);

/// set conf->dest to false
int opt_ap_flag_false(struct opt_context *ctx,
               const struct opt_conf *conf,
               char *s);

int opt_ap_flag_count(struct opt_context *ctx,
               const struct opt_conf *conf,
               char *s);
#endif

