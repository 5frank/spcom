#ifndef OPT_SECTION_INCLUDE_H_
#define OPT_SECTION_INCLUDE_H_

struct opt_conf;
struct opt_section_entry;


/**  */
typedef int (*opt_post_parse_fn)(const struct opt_section_entry *entry);

struct opt_section_entry {
    const char *name;
    const struct opt_conf *conf;
    size_t nconf;
    //const char *file;
    opt_post_parse_fn post_parse;
};

/**
 * these assume `__attribute((used, section("options"))) `
 * used in OPT_SECTION_ADD() macro
*/
extern const struct opt_section_entry __start_options;
extern const struct opt_section_entry __stop_options;

static inline const struct opt_section_entry *opt_section_start_entry(void)
{
    return &__start_options;
}

static inline size_t opt_section_num_entries(void)
{
    size_t sect_size = (uintptr_t)&__stop_options
                     - (uintptr_t)&__start_options;
    size_t num_entries = sect_size / sizeof(struct opt_section_entry);
    return num_entries;
}


#ifndef STRINGIFY
#define STRINGIFY(X) ___STRINGIFY_VIA(X)
// Via expand macro. No parent
#define ___STRINGIFY_VIA(X) #X
#endif

#define OPT_SECTION_ADD(NAME, CONF, NUM_CONF, POST_PARSE_CB)                   \
    __attribute((used, section("options")))                                    \
    static const struct opt_section_entry opt_conf_register_##NAME = {         \
        .name = STRINGIFY(NAME),                                               \
        .conf = CONF,                                                          \
        .nconf = NUM_CONF,                                                     \
        .post_parse = POST_PARSE_CB                                            \
    };

typedef int (opt_conf_foreach_cb_fn)(const struct opt_section_entry *entry,
                                     const struct opt_conf *conf, void *arg);

/** run callback for every opt_conf instance.
 * iteration stops if callback returns non-zero
 * */
static inline int opt_section_foreach_conf(opt_conf_foreach_cb_fn *cb,
                                           void *cb_arg)
{
    const struct opt_section_entry *entries = opt_section_start_entry();
    size_t num_entries = opt_section_num_entries();

    for (unsigned int i = 0; i < num_entries; i++) {

        const struct opt_section_entry *entry = &entries[i];

        for (int i = 0; i < entry->nconf; i++) {

            const struct opt_conf *conf = &entry->conf[i];

            int err = cb(entry, conf, cb_arg);
            if (err)
                return err;
        }
    }

    return 0;
}

#endif
