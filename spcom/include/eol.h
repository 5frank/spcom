
#ifndef EOL_INCLUDE_H_
#define EOL_INCLUDE_H_


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
