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

// clang-format off
enum op_code_e {
    OP_PORT_WRITE    =  1,
    OP_PORT_PUTC     =  2,
    OP_PORT_PUT_EOL  =  3,
    OP_PORT_SET_RTS  =  4,
    OP_PORT_SET_CTS  =  5,
    OP_PORT_SET_DTR  =  6,
    OP_PORT_SET_DSR  =  7,
    OP_PORT_DRAIN    =  8,
    OP_PORT_FLUSH    =  9,
    OP_SLEEP         = 10,
    OP_EXIT          = 11
};
// clang-format on

struct opq_item {
    int op_code;
    /// value for setters etc
    int val;

    struct op_data {
        size_t size;
        char *data; // freed when done
    } buf;
};

/// oo - on open
extern struct opq opq_oo;
/// rt - runtime
extern struct opq opq_rt;

void opq_reset(struct opq *q);

int opq_push_value(struct opq *q, int op_code, int val);
int opq_push_heapdata(struct opq *q, void *data, size_t size);

/// alloc tail
struct opq_item *opq_alloc_tail(struct opq *q);

int opq_push_tail(struct opq *q, struct opq_item *itm);

/// peek at head of queue without updating index. 
/// assume passed to free_head later 
struct opq_item *opq_peek_head(struct opq *q);

/// "free" item retrived with peek_head
void opq_free_head(struct opq *q, struct opq_item *itm);

void opq_free_all(struct opq *q);
#endif

