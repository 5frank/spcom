#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// local
#include "assert.h"
#include "btree.h"
#include "opt.h"
#include "opt_argviter.h"

#define OPT_DBG(...)                                                           \
    if (0) {                                                                   \
        fprintf(stderr, __VA_ARGS__);                                          \
    }                                                                          \
    else

#define OPT_MAX_COUNT 256

struct opt_context {

    bool initialized;
    /// number of opt_conf
    unsigned int num_opts;
    // binary search tree for options also provides duplicate check
    struct btree btree;
    /// number of nodes in btree. up to 3 per opt_conf.
    unsigned int num_nodes;
    /// tree nodes dynamically allocated
    struct btree_node *btree_nodes;
    /// node write pointer. should always point in range of btree_nodes
    struct btree_node *node_wrp;
    /// used to track whatever argument parse error printed or not
    unsigned int perror_count;
    struct {
        unsigned int last;
        const struct opt_conf *conf[8]; // lazy no malloc
    } positionals;
};

static struct opt_context g_ctx = { 0 };

/**
 * @return NULL if @param c i zero, otherwise a static allocated and
 * nul terminated string of length 1 with c as first char
 */
static const char *opt_shortstr(char c)
{
    static const char *lut[] = {
        // clang-format off
        [ 0] = "0", [ 1] = "1", [ 2] = "2", [ 3] = "3",
        [ 4] = "4", [ 5] = "5", [ 6] = "6", [ 7] = "7",
        [ 8] = "8", [ 9] = "9", [17] = "A", [18] = "B",
        [19] = "C", [20] = "D", [21] = "E", [22] = "F",
        [23] = "G", [24] = "H", [25] = "I", [26] = "J",
        [27] = "K", [28] = "L", [29] = "M", [30] = "N",
        [31] = "O", [32] = "P", [33] = "Q", [34] = "R",
        [35] = "S", [36] = "T", [37] = "U", [38] = "V",
        [39] = "W", [40] = "X", [41] = "Y", [42] = "Z",
        [49] = "a", [50] = "b", [51] = "c", [52] = "d",
        [53] = "e", [54] = "f", [55] = "g", [56] = "h",
        [57] = "i", [58] = "j", [59] = "k", [60] = "l",
        [61] = "m", [62] = "n", [63] = "o", [64] = "p",
        [65] = "q", [66] = "r", [67] = "s", [68] = "t",
        [69] = "u", [70] = "v", [71] = "w", [72] = "x",
        [73] = "y", [74] = "z"
        // clang-format on
    };

    if (c == '\0')
        return NULL;

    unsigned int i = c;
    i -= '0';
    assert(i < ARRAY_LEN(lut));
    const char *s = lut[i];
    assert(s[0]);
    return s;
}

static bool opt_conf_has_val(const struct opt_conf *conf)
{
    assert(conf->parse);

    if (conf->flags & OPT_F_NO_VAL)
        return false;

    if (conf->positional)
        return false;

    if (conf->parse == opt_parse_flag_true)
        return false;

    if (conf->parse == opt_parse_flag_false)
        return false;

    if (conf->parse == opt_parse_flag_count)
        return false;

    return true;
}

#define SSPRINTF(SP, REMAINS, ...)                                             \
    do {                                                                       \
        int _rc = snprintf(SP, REMAINS, __VA_ARGS__);                          \
        assert(_rc > 0);                                                       \
        assert(_rc < REMAINS);                                                 \
        REMAINS -= _rc;                                                        \
        SP += _rc;                                                             \
    } while (0)

static char *opt_conf_str(const struct opt_section_entry *entry,
                          const struct opt_conf *conf)
{
    static char buf[128];
    size_t remains = sizeof(buf);
    char *sp = buf;
    sp[0] = '\0';

    if (conf->positional) {
        SSPRINTF(sp, remains, "<positional %d>", conf->positional);
    }

    const char *delim = "";
    if (conf->shortname) {
        SSPRINTF(sp, remains, "-%c", conf->shortname);
        delim = ", ";
    }

    if (conf->name) {
        SSPRINTF(sp, remains, "%s--%s", delim, conf->name);
    }

    // can not have alias only
    if (conf->alias) {
        SSPRINTF(sp, remains, ", --%s", conf->alias);
    }

    if (entry && entry->name) {
        SSPRINTF(sp, remains, " (section: %s)", entry->name);
    }

    return buf;
}
/**
 * @brief this is a software error. Some conf is incorrect.
 */
static int opt_conf_error(const struct opt_section_entry *entry,
                          const struct opt_conf *conf, const char *msg)
{
    msg = msg ? msg : "";
    fprintf(stderr, "error: opt_conf %s - %s\n", opt_conf_str(entry, conf),
            msg);

    return -EBADF;
}

int opt_perror(const struct opt_conf *conf, const char *fmt, ...)
{
    g_ctx.perror_count++;

    fprintf(stderr, "spcom: option error: ");
    if (conf) {
        fprintf(stderr, "%s - ", opt_conf_str(NULL, conf));
    }
    va_list args;

    va_start(args, fmt);
    int rc = vfprintf(stderr, fmt, args);
    va_end(args);
    (void)rc;

    fputc('\n', stderr);
    return -1;
}

/**
 * empty dummy ignored by reader. this to ensure section exists in case notmade
 * elsewere. otherwise start and stop symbols not found and we get a compile
 * error.  there might be a prettier way of handling this. using weak attribute
 * for example.
 */
OPT_SECTION_ADD(dummy, NULL, 0, NULL)

static const struct opt_conf *opt_lookup(struct opt_context *ctx,
                                         const char *name)
{
    struct btree_node *node = btree_find(&ctx->btree, name);
    if (!node)
        return NULL;

    return node->data;
}

static const struct opt_conf *
opt_lookup_next_positional(struct opt_context *ctx)
{
    for (int i = 0; i < ARRAY_LEN(ctx->positionals.conf); i++) {
        const struct opt_conf *conf = ctx->positionals.conf[i];
        if (!conf)
            return NULL;

        assert(conf->positional > 0);
        if (conf->positional <= ctx->positionals.last)
            continue;

        ctx->positionals.last = conf->positional;
        return conf;
    }

    return NULL;
}

static int opt_add_positional(struct opt_context *ctx,
                              const struct opt_conf *conf)
{
    for (int i = 0; i < ARRAY_LEN(ctx->positionals.conf); i++) {
        const struct opt_conf *pconf = ctx->positionals.conf[i];
        if (pconf) {
            assert(pconf->positional != conf->positional);
        }
        else {
            ctx->positionals.conf[i] = conf;
            return 0;
        }
    }

    return -ENOMEM;
}

static int opt_init_cb_precheck(const struct opt_section_entry *entry,
                                const struct opt_conf *conf, void *arg)
{
    struct opt_context *ctx = arg;

    ctx->num_opts++;

    if (!conf->parse)
        return opt_conf_error(entry, conf, "no parse callback");

    // ignore positonals here
    if (conf->positional)
        return 0;

    if (!conf->shortname && !conf->name)
        return opt_conf_error(entry, conf, "no long or short name");

    if (conf->name)
        ctx->num_nodes++;

    if (conf->shortname)
        ctx->num_nodes++;

    if (conf->alias)
        ctx->num_nodes++;

    return 0;
}

static int opt_init_cb_addtobtree(const struct opt_section_entry *entry,
                                  const struct opt_conf *conf, void *arg)
{
    struct opt_context *ctx = arg;
    int err;

    if (conf->positional) {
        err = opt_add_positional(ctx, conf);
        assert(!err);
        return 0;
    }

    unsigned int conf_id = (uintptr_t)ctx->node_wrp
        - (uintptr_t)ctx->btree_nodes;

    const char *svals[] = { conf->name, opt_shortstr(conf->shortname),
                            conf->alias };

    for (int i = 0; i < ARRAY_LEN(svals); i++) {
        if (!svals[i])
            continue;

        struct btree_node *node = ctx->node_wrp++;
        node->sval = svals[i];
        node->data = conf;
        node->id = conf_id;

        err = btree_add_node(&ctx->btree, node);
        if (err) {
            if (err == -EEXIST)
                opt_conf_error(NULL, conf, "duplicate option names");

            return err;
        }
    }

    return 0;
}

static int opt_init(struct opt_context *ctx)
{
    int err;

    ctx->num_opts = 0;
    ctx->num_nodes = 0;

    err = opt_section_foreach_conf(opt_init_cb_precheck, ctx);
    if (err)
        return err;

    if (ctx->num_opts >= OPT_MAX_COUNT)
        return opt_conf_error(NULL, NULL, "number of options exceed max");

    OPT_DBG("num_opts=%u\n", ctx->num_opts);

    ctx->btree_nodes = calloc(ctx->num_nodes, sizeof(struct btree_node));
    assert(ctx->btree_nodes);
    // init write pointer used in foreach callback
    ctx->node_wrp = ctx->btree_nodes;

    err = opt_section_foreach_conf(opt_init_cb_addtobtree, ctx);
    if (err)
        return err;

    return 0;
}

static int opt_conf_parse_val(struct opt_context *ctx, const struct opt_conf *conf,
                          char *sval)
{
    OPT_DBG("parsing %s:%c: sval='%s'\n", conf->name, conf->shortname, sval);
    assert(conf->parse);

    unsigned int perror_count = ctx->perror_count;
    int err = conf->parse(conf, sval);

    if (err) {
        bool error_printed = perror_count != ctx->perror_count;
        if (error_printed)
            return err;

        fprintf(stderr, "error: %s - parse failure %d\n",
                opt_conf_str(NULL, conf), err);

        return err;
    }

    return 0;
}

int opt_parse_args(int argc, char *argv[])
{
    int err;
    struct opt_context *ctx = &g_ctx;
    if (!ctx->initialized) {
        err = opt_init(ctx);
        if (err)
            return err;
        ctx->initialized = true;
    }

    struct opt_argviter_ctx itr = {0};
    int flags = 0;
    err = opt_argviter_init(&itr, flags, argc, argv);
    assert(!err);

    while (1) {
        err = opt_argviter_getkey(&itr);
        if (err == OPT_ARGVITER_DONE)
            break;
        else if (err)
            return err;

        const char *arg = opt_argviter_getargv(&itr);

        OPT_DBG("looking up: %s\n", itr.out.name);
        const struct opt_conf *conf = NULL;
        char *sval = NULL;

        switch (itr.out.dashes) {
            case 0:
                conf = opt_lookup_next_positional(ctx);
                sval = itr.out.name;
                break;
            case 1:
            case 2:
                conf = opt_lookup(ctx, itr.out.name);
                break;
            default:
                assert(0);
                return -1;
        }

        if (!conf) {
            fprintf(stderr, "spcom: unrecognized option '%s'\n", arg);
            return -ENOENT;
        }

        if (opt_conf_has_val(conf)) {
            err = opt_argviter_getval(&itr);
            if (err)
                return opt_perror(conf, "%s '%s'", itr.errmsg, arg);

            sval = itr.out.val;
        }

        err = opt_conf_parse_val(ctx, conf, sval);
        if (err)
            return err;
    }

    const struct opt_section_entry *entries = opt_section_start_entry();
    size_t num_entries = opt_section_num_entries();

    for (unsigned int i = 0; i < num_entries; i++) {
        const struct opt_section_entry *entry = &entries[i];
        if (!entry->post_parse)
            continue;
        err = entry->post_parse(entry);
        if (err)
            return err;
    }

    return 0;
}

static int opt_help_cb(const struct opt_section_entry *entry,
                       const struct opt_conf *conf, void *arg)
{
    printf("  %s", opt_conf_str(NULL, conf));
    // TODO pretty print
    if (conf->metavar) {
        printf(" [%s]", conf->metavar);
    }

    if (conf->descr) {
        printf("  %s", conf->descr);
    }

    printf("\n");

    return 0;
}

void opt_show_help(void)
{
    int err = opt_section_foreach_conf(opt_help_cb, &g_ctx);
    // only error if callback returns error
    (void)err;
}

const char **opt_autocomplete(const char *name, const char *startstr)
{
    struct opt_context *ctx = &g_ctx;
    assert(ctx->initialized);

    const struct opt_conf *conf = opt_lookup(ctx, name);
    if (!conf)
        return NULL;

    if (!conf->complete)
        return NULL;

    return conf->complete(startstr);
}
