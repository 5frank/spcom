
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
    /// eol sequence or char found
    EOL_C_MATCH,
    /// ignore c. (should still update prev_c)
    EOL_C_IGNORE,
};

struct eol_seq {
    unsigned char c_a;
    unsigned char c_b;
    short int ignore;
    int (*match_func)(const struct eol_seq *p, int prev_c, int c);
};

extern const struct eol_seq *eol_rx;
extern const struct eol_seq *eol_tx;

/**
 * @param prev_c previous c. assumed -1 on first call
 */
int eol_match(const struct eol_seq *es, int prev_c, int c);

int eol_seq_cpy(const struct eol_seq *es, char *dst, size_t size);

#endif
