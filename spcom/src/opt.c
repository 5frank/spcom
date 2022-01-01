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
#include "opt_argviter.h"

struct opt_short_map {
    char shortname;
    const struct opt_conf *conf;
};

struct opt_context {
    unsigned int num_opts;
    struct opt_short_map *short_map;
    const struct opt_section_entry *entries;
    size_t nentries;
    // binary search tree for long options also provides duplicate check
    struct btree btree;
    struct btree_node *btree_nodes;
    struct {
        unsigned int last;
        const struct opt_conf *conf[8]; // lazy no malloc 
    } positionals;
};

#ifndef IS_ALIGNED
#define IS_ALIGNED(PTR, SIZE) (((uintptr_t)(PTR) % (SIZE)) == 0)
#endif

#define OPT_DBG(...) printf(__VA_ARGS__)



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

int opt_error(struct opt_context *ctx, const struct opt_conf *conf, const char *msg)
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

static const struct opt_conf *opt_lookup_short(struct opt_context *ctx,
                                               char shortname)
{
    for (int i = 0; i < ctx->num_opts; i++) {

        struct opt_short_map *sm = &ctx->short_map[i];
        if (sm->shortname == '\0')
            break;

        if (sm->shortname == shortname)
            return sm->conf;
    }

    return NULL;
}

static const struct opt_conf *opt_lookup_long(struct opt_context *ctx,
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
        if (!ctx->positionals.conf[i]) {
            ctx->positionals.conf[i] = conf;
            return 0;
        }
    }

    return -ENOMEM;
}

static int opt_section_read(struct opt_context *ctx)
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
    for (int i = 0; i < nentries; i++) {
        const struct opt_section_entry *entry = &entries[i];
        assert(entry);
        assert(entry->name);
        OPT_DBG("nconf=%zu from src: %s\n", entry->nconf, entry->name);
        num_opts += entry->nconf;
    }

    OPT_DBG("num_opts=%zu\n", num_opts);
    ctx->num_opts = num_opts;

    size_t size;
    size = (num_opts + 1) * sizeof(struct opt_short_map);
    ctx->short_map = malloc(size);
    assert(ctx->short_map);
    struct opt_short_map *smap = ctx->short_map;

    size = (num_opts + 1) * sizeof(struct btree_node);
    ctx->btree_nodes = malloc(size);
    assert(ctx->btree_nodes);
    struct btree_node *node = ctx->btree_nodes;


    for (int i = 0; i < nentries; i++) {
        const struct opt_section_entry *entry = &entries[i];

        for (int i = 0; i < entry->nconf; i++) {
            const struct opt_conf *conf = &entry->conf[i];

            if (!conf->parse) {
                return opt_conf_error(entry, conf, "no parse callback");
            }

            if (conf->positional) {
                err = opt_add_positional(ctx, conf);
                assert(!err);
                continue;
            }

            if (!conf->shortname && !conf->name) {
                return opt_conf_error(entry, conf, "no long or short name");
            }

            if (conf->shortname) {
                smap->shortname = conf->shortname;
                smap->conf = conf;
                smap++;
            }

            if (conf->name) {
                node->sval = conf->name;
                node->data = conf;
                err = btree_add_node(&ctx->btree, node);
                if (err == -EEXIST) {
                    opt_conf_error(entry, conf, "duplicate names");
                    return err;
                }
                assert(!err); // never!?
                node++;
            }
        }
    }
    // nul terminate
    smap->shortname = '\0';
    smap->conf = NULL;

    ctx->entries = entries;
    ctx->nentries = nentries;

    return 0;
}

static int opt_conf_parse(struct opt_context *ctx, 
                    const struct opt_conf *conf,
                    char *sval)
{
    printf("parsing %s:%c: sval='%s'\n", conf->name, conf->shortname, sval);
    assert(conf->parse);
    return conf->parse(ctx, conf, sval);

    return 0;
}

int opt_parse_args(struct opt_context *ctx, int argc, char *argv[])
{
    int err;

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

        printf("looking up: %s\n", itr.out.name);
        const struct opt_conf *conf = NULL;
        char *sval = NULL;

        switch(itr.out.dashes) {
            case 0:
                conf = opt_lookup_next_positional(ctx);
                sval = itr.out.name;
                break;
            case 1:
                conf = opt_lookup_short(ctx, itr.out.name[0]);
                break;
            case 2:
                conf = opt_lookup_long(ctx, itr.out.name);
                break;
            default:
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
        err = entry->post_parse(ctx, entry);
        if (err)
            return err;
    }
    return 0;
}

/// this could also be dynamicly allocated but no point with that!?
static struct opt_context g_ctx = { 0 };

struct opt_context *opt_init(void)
{
    int err;
    struct opt_context *ctx = &g_ctx;
    err = opt_section_read(ctx);
    if (err)
        return NULL;

    return ctx;
}


static int help_traverse_cb(const struct btree_node *node)
{
    const struct opt_conf *conf = node->data;
    assert(conf);

    if (conf->metavar)
        printf("[%s]\n", conf->metavar);

    if (conf->descr)
        printf("%s\n", conf->descr);

    return 0;
}

int opt_show_help(struct opt_context *ctx, const char *s)
{
    printf("TODO short options only missing here.\n");
    return btree_traverse(&ctx->btree, NULL, s, help_traverse_cb);
}
