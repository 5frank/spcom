#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>

#include "opt.h"
#include "btree.h"
#include "btable.h"
#include "opt_argviter.h"
#include "opt_shortstr.h"

#ifndef IS_ALIGNED
#define IS_ALIGNED(PTR, SIZE) (((uintptr_t)(PTR) % (SIZE)) == 0)
#endif

#define OPT_DBG(...) if (0) { printf(__VA_ARGS__); } else

struct opt_context {
    bool initialized;
    unsigned int num_opts;
    const struct opt_section_entry *entries;
    size_t nentries;
    // binary search tree for options also provides duplicate check
    struct btree btree;
    struct btree_node *btree_nodes;
    struct btable *btable;
    size_t nnodes;
    struct {
        unsigned int last;
        const struct opt_conf *conf[8]; // lazy no malloc 
    } positionals;
};

static struct opt_context g_ctx = { 0 };


static bool opt_conf_has_val(const struct opt_conf *conf)
{
    assert(conf->parse);

    if (conf->flags & OPT_F_NO_VAL)
        return false;

    if (conf->positional)
        return false;

    if (conf->parse == opt_ap_flag_true)
        return false;

    if (conf->parse == opt_ap_flag_false)
        return false;

    if (conf->parse == opt_ap_flag_count)
        return false;

    return true;
}

static int opt_conf_error(const struct opt_section_entry *entry,
                          const struct opt_conf *conf,
                          const char *msg)
{

    fprintf(stderr, "error: opt_conf %s. ", msg);
    fprintf(stderr, "name: ");
    if (conf->name)
        fprintf(stderr, "\"%s\"", conf->name);

    if (conf->shortname)
        fprintf(stderr, "'%c'", conf->shortname);

    if (entry && entry->name) {
        fprintf(stderr, " in  %s", entry->name);
    }

    fprintf(stderr, "\n");

    return -EBADF;
}

static int opt_arg_error(const struct opt_conf *conf,
                        const char *arg,
                        const char *msg)
{
    fprintf(stderr, "error: ");
    if (conf) {
        //fprintf(stderr, "name: ");
        if (conf->name)
            fprintf(stderr, "--%s", conf->name);

        if (conf->name && conf->shortname)
            fprintf(stderr, ",");

        if (conf->shortname)
            fprintf(stderr, "-%c", conf->shortname);

        fprintf(stderr, " ");
    }

    fprintf(stderr, "%s", msg);

    if (arg)
        fprintf(stderr, " \"%s\"", arg);

    fprintf(stderr, "\n");
    return -EINVAL;
}

int opt_error(const struct opt_conf *conf, const char *msg) 
{
   return opt_arg_error(conf, NULL, msg); // TODO
}

/**
 * empty dummy ignored by reader. this to ensure section exists in case notmade
 * elsewere. otherwise start and stop symbols not found and we get a compile
 * error.  there might be a prettier way of handling this. using weak attribute
 * for example.
 */
OPT_SECTION_ADD(dummy, NULL, 0, NULL)

/**
 * these assume `__attribute((used, section("options"))) `
 * used in OPT_SECTION_ADD() macro
*/
extern const struct opt_section_entry __start_options;
extern const struct opt_section_entry __stop_options;

static const struct opt_conf *opt_lookup(struct opt_context *ctx,
                                         const char *name)
{
     struct btree_node *node = btree_find(&ctx->btree, name);
     if (!node)
         return NULL;

     return node->data;
}

static const struct opt_conf *opt_lookup_next_positional(struct opt_context *ctx)
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

static int opt_add_positional(struct opt_context *ctx, const struct opt_conf *conf)
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

static int opt_add_conf_nodes(struct opt_context *ctx, 
                               const struct opt_conf *conf,
            struct btree_node **pp_node)

{
    struct btree_node *node = *pp_node;
    unsigned int conf_id = (unsigned int)(node - ctx->btree_nodes);

    const char *svals[] = {
        conf->name,
        opt_shortstr(conf->shortname),
        conf->alias
    };

    for (int i = 0; i < ARRAY_LEN(svals); i++) {
        if (!svals[i])
            continue;

        node->sval = svals[i];
        node->data = conf;
        node->id = conf_id;

        int err = btree_add_node(&ctx->btree, node);
        if (err) {
            if (err == -EEXIST)
                opt_conf_error(NULL, conf, "duplicate option names");
            return err;
        }
        node++;
    }

    *pp_node = node;
    return 0;
}
#if 0
static inline bool is_valid_long_opt(const char *s)
{
    if (!s)
        return true; // valid - no long option

    if (!s[0])
        return false;

    if (!s[1])
        return false

    return true;
}
#endif

static int opt_init(struct opt_context *ctx)
{
    int err;
#if 1
    size_t sect_size = (uintptr_t)&__stop_options
                     - (uintptr_t)&__start_options;
    size_t nentries = sect_size / sizeof(struct opt_section_entry);
#else
    size_t nentries = &__stop_options - &__start_options;
#endif
    OPT_DBG("nentries=%zu\n", nentries);

    const struct opt_section_entry *entries = &__start_options;

    size_t num_opts = 0;
    size_t nnodes = 0;
    for (int i = 0; i < nentries; i++) {
        const struct opt_section_entry *entry = &entries[i];
        assert(entry);
        assert(entry->name);
        OPT_DBG("nconf=%zu from src: %s\n", entry->nconf, entry->name);
        num_opts += entry->nconf;


        for (int i = 0; i < entry->nconf; i++) {
            const struct opt_conf *conf = &entry->conf[i];
            if (!conf->parse)
                return opt_conf_error(entry, conf, "no parse callback");

            if (conf->positional)
                continue;
#if 0
            if (!is_valid_long_opt(conf->name))
                return opt_conf_error(entry, conf, "invalid name");

            if (!is_valid_long_opt(conf->alias))
                return opt_conf_error(entry, conf, "invalid alias");
#endif
            if (!conf->shortname && !conf->name)
                return opt_conf_error(entry, conf, "no long or short name");

            if (conf->name)
                nnodes++;

            if (conf->shortname)
                nnodes++;

            if (conf->alias)
                nnodes++;
        }
    }

    OPT_DBG("num_opts=%zu\n", num_opts);
    ctx->num_opts = num_opts;

    ctx->btable = btable_create(nnodes + 1);
    assert(ctx->btable);

    size_t size = (nnodes + 1) * sizeof(struct btree_node);
    ctx->btree_nodes = malloc(size);
    assert(ctx->btree_nodes);
    struct btree_node *node = ctx->btree_nodes;

    for (int i = 0; i < nentries; i++) {
        const struct opt_section_entry *entry = &entries[i];

        for (int i = 0; i < entry->nconf; i++) {
            const struct opt_conf *conf = &entry->conf[i];

            if (conf->positional) {
                err = opt_add_positional(ctx, conf);
                assert(!err);
                continue;
            }

            err = opt_add_conf_nodes(ctx, conf, &node);
            if (err)
                return err;
        }
    }

    ctx->entries = entries;
    ctx->nentries = nentries;
    ctx->nnodes = nnodes;

    return 0;
}

static int opt_conf_parse(struct opt_context *ctx, 
                    const struct opt_conf *conf,
                    char *sval)
{
    OPT_DBG("parsing %s:%c: sval='%s'\n", conf->name, conf->shortname, sval);
    assert(conf->parse);
    return conf->parse(conf, sval);

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

    struct opt_argviter_ctx itr = { };
    int flags = 0;
    err = opt_argviter_init(&itr, flags, argc, argv);
    assert(!err);


    while(1) {
        err = opt_argviter_getkey(&itr);
        if (err == OPT_ARGVITER_DONE)
            break;
        else if (err)
            return err;

        const char *arg = opt_argviter_getargv(&itr);

        OPT_DBG("looking up: %s\n", itr.out.name);
        const struct opt_conf *conf = NULL;
        char *sval = NULL;

        switch(itr.out.dashes) {
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
            opt_arg_error(NULL, arg, "unknown option");
            return 404;
        }

        if (opt_conf_has_val(conf)) {
            err = opt_argviter_getval(&itr);
            if (err)
                return opt_arg_error(conf, arg, itr.errmsg);

            sval = itr.out.val;
        }

        err = opt_conf_parse(ctx, conf, sval);
        if (err)
           return err;
    }

    for (int i = 0; i < ctx->nentries; i++) {
        const struct opt_section_entry *entry = &ctx->entries[i];
        if (!entry->post_parse)
            continue;
        err = entry->post_parse(entry);
        if (err)
            return err;
    }
    return 0;
}


static int opt_help_cb(const struct btree_node *node, void *arg)
{
    const struct opt_conf *conf = node->data;
    assert(conf);
    struct btable *btable = arg;

    // ignore duplicate due alias or short option name
    if (btable_get(btable, node->id))
        return 0;

    btable_set(btable, node->id);

    // TODO pretty print
    if (conf->metavar)
        printf("[%s]\n", conf->metavar);

    if (conf->descr)
        printf("%s\n", conf->descr);

    return 0;
}

int opt_show_help(const char *s)
{
    struct opt_context *ctx = &g_ctx;
    assert(ctx->initialized);
    struct btable *btable = ctx->btable;
    btable_reset(btable, 0);

    return btree_traverse(&ctx->btree, NULL, s, opt_help_cb, btable);
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
