/** 
 * This module could be seen as a "tx queue with extras".
 *
 * convert commands to operations because:
 * - alows to verify command is valid before executed - i.e. when passed as cli option
 * - simplifies implementation of sleep (i.e. hold on write)
 */ 
#ifndef OPQ_INCLUDE_H__
#define OPQ_INCLUDE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// clang-format off
enum op_code_e {
    OP_PORT_WRITE = 1,
    OP_PORT_PUTC,
    OP_PORT_PUT_EOL,
    OP_PORT_SET_RTS,
    OP_PORT_SET_CTS,
    OP_PORT_SET_DTR,
    OP_PORT_SET_DSR,
    OP_PORT_BAUD,
    OP_PORT_FLOW,
    OP_PORT_PARITY,
    OP_PORT_BREAK,
    OP_PORT_DRAIN,
    OP_PORT_FLUSH,
    OP_SLEEP,
    OP_EXIT
};
// clang-format on

struct opq_item {
    uint16_t op_code;
    uint16_t size; //<! non-zero if hdata below
    /// type of data impleied byt op_code
    union {
        uint32_t ui_val;
        uint32_t si_val;
        void *hdata; // heap data freed when done
    } u;
};

/// oo - on open
extern struct opq opq_oo;
/// rt - runtime
extern struct opq opq_rt;

void opq_reset(struct opq *q);

/// enqueue value
int opq_enqueue_val(struct opq *q, uint16_t op_code, int val);

/// enqueue heap data. freed by consumer
int opq_enqueue_hdata(struct opq *q,
                      uint16_t op_code,
                      void *data,
                      uint16_t size);

/// get tail
struct opq_item *opq_peek_tail(struct opq *q);

/// assumes @param itm same as returned by opq_peek_tail()
int opq_enqueue_tail(struct opq *q, struct opq_item *itm);


/** peek at head of queue without updating index. 
 * assume passed to free_head later */
struct opq_item *opq_peek_head(struct opq *q);

/// "free" item retrived with peek_head
void opq_free_head(struct opq *q, struct opq_item *itm);

void opq_free_all(struct opq *q);
#endif

