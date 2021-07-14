
#ifndef EOL_INCLUDE_H_
#define EOL_INCLUDE_H_

enum eol_type_e {
    EOL_TX = 1 << 0,
    EOL_RX = 1 << 1
};

struct eol_seq {
    uint8_t seq[4];
    uint8_t len;
    uint32_t seqmsk;
    uint32_t cmpmsk;
};

int eol_parse_opts(int type, const char *s);

int eol_set(int type, const void *data, size_t len);

//const struct eol_seq *eol_rx_check(char c);
int eol_rx_check(char c);
void eol_rx_check_reset(void);

const struct eol_seq *eol_tx_get(void);


#endif
