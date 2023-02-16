/**
 * This module could be seen as a "tx queue with extras".
 *
 * convert commands to operations because:
 * - alows to verify command is valid before executed - i.e. when passed as cli option
 * - simplifies implementation of sleep (i.e. hold on write)
 */
#ifndef OPQ_INCLUDE_H_
#define OPQ_INCLUDE_H_

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
    uint16_t size; //<! non-zero if u.data not NULL
    /// type of data implied by op_code and size
    union {
        int val;
        void *data;
    } u;
};

typedef void (opq_free_cb)(const struct opq_item *itm);

/// oo - on open
extern struct opq opq_oo;
/// rt - runtime
extern struct opq opq_rt;

void opq_reset(struct opq *q);

void opq_set_free_cb(struct opq *q, opq_free_cb *cb);

/// enqueue value
int opq_enqueue_val(struct opq *q, uint16_t op_code, int val);

/**
 * enqueue a write operation. prior call to opq_set_free_cb might be
 * needed */
int opq_enqueue_write(struct opq *q,
                      void *data,
                      uint16_t size);

/**
 * peek and acquire tail without updating index. Returned item (unless NULL)
 * could be passed to opq_enqueue_tail() or ignored.  */
struct opq_item *opq_acquire_tail(struct opq *q);

/// assumes @param itm same as returned by opq_acquire_tail()
int opq_enqueue_tail(struct opq *q, struct opq_item *itm);

/**
 * peek at head of queue without updating index.
 * assume passed to free_head later */
struct opq_item *opq_acquire_head(struct opq *q);

/** free and release item retrived with opq_acquire_head() */
void opq_release_head(struct opq *q, struct opq_item *itm);

void opq_release_all(struct opq *q);
#endif

