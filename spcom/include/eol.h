
#ifndef EOL_INCLUDE_H_
#define EOL_INCLUDE_H_

#include <stdint.h>

enum eol_c_e {
    EOL_C_NONE = 0,
    EOL_C_FWD1,
    EOL_C_FWD2,
    EOL_C_CONSUMED,
    EOL_C_FOUND,
};

struct eol_seq;

typedef int (eol_eval_fn)(struct eol_seq *es, char c, char *cfwd);

struct eol_seq {
    eol_eval_fn *handler;
    uint8_t index;
    uint8_t len;
    char seq[2];
};

extern struct eol_seq *eol_rx;
extern struct eol_seq *eol_tx;

static inline int eol_eval(struct eol_seq *es, char c, char *cfwd)
{
    return es->handler(es, c, cfwd);
}

#if 0

struct eol_seq {
    uint8_t seq[4];
    uint8_t len;
    uint32_t seqmsk;
    uint32_t cmpmsk;
};


int eol_rx_check(char c);
void eol_rx_check_reset(void);

const struct eol_seq *eol_tx_get(void);
#endif

#endif
