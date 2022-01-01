#ifndef OPT_ARGVITER_INCLUDE_H_
#define OPT_ARGVITER_INCLUDE_H_

#define OPT_ARGVITER_DONE (0xff)

struct opt_argviter_ctx {
    const char *errmsg;
    int argi;
    int argc;
    char **argv;
    char *sp;
    int dashes;
    char *key;
    unsigned int key_len;
    char *val;
    unsigned int val_len;

    struct opt_argviter_out {
        int dashes;
        char *name;
        char *val;
    } out;
};

int opt_argviter_init(struct opt_argviter_ctx *p,
                      int flags, int argc, char *argv[]);
int opt_argviter_getkey(struct opt_argviter_ctx *p);
int opt_argviter_getval(struct opt_argviter_ctx *p);

static inline const char *opt_argviter_getargv(struct opt_argviter_ctx *p)
{
    return p->argv[p->argi];
}
#endif
