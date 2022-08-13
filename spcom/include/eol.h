
#ifndef EOL_INCLUDE_H_
#define EOL_INCLUDE_H_

#include <stdint.h>

enum {
    EOL_C_NOMATCH = 0,
    /// hold on to c. partial match
    EOL_C_STASH,
    /// release/pop prev_c. c is not a match
    EOL_C_POP,
    /// release/pop prev_c c is a match
    EOL_C_POP_AND_STASH,
    /// eol found
    EOL_C_MATCH,
    // excpect same behavior
    EOL_C_IGNORE,
};

struct eol_seq {
    unsigned char seq[2];
    unsigned char len;
    int (*match_func)(const struct eol_seq *p, int prev_c, int c);
};

extern struct eol_seq *eol_rx;
extern struct eol_seq *eol_tx;
/**
 * @param prev_c previous c. assumed -1 on first call */
static inline int eol_match(const struct eol_seq *es, int prev_c, int c)
{
    if (!es->match_func)
        return EOL_C_NOMATCH;

    return es->match_func(es, prev_c, c);
}

int eol_seq_cpy(const struct eol_seq *es, char *dst, size_t size);

#endif
