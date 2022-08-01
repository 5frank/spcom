
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "common.h"
#include "assert.h"
#include "opq.h"

struct opq {
    struct opq_item items[64];
    unsigned int wridx;
    unsigned int rdidx;
    opq_write_done_cb *write_done_cb;
};

struct opq opq_oo;
struct opq opq_rt;

static inline unsigned int opq_len(const struct opq *q)
{
    // unsigend overflow is defined behavior
    return (q->wridx - q->rdidx) % ARRAY_LEN(q->items);
}

static inline bool opq_isfull(const struct opq *q)
{
    // intentionaly off by one!
    return opq_len(q) >= (ARRAY_LEN(q->items) - 1);
}

static inline bool opq_isempty(const struct opq *q)
{
    // or return q->wridx == q->rdidx;
    return opq_len(q) == 0;
}

void opq_reset(struct opq *q)
{
    q->wridx = 0;
    q->rdidx = 0;
}

void opq_set_write_done_cb(struct opq *q, opq_write_done_cb *cb)
{
    q->write_done_cb = cb;
}
/** enqueue/put.
 * updates write index (aka tail) after new item is added. this is where
 * the intentional off by one is from
 */ 
#if 0
int opq_push(struct opq *q, const void *data, size_t size)
{
    if (opq_isfull(q)) {
        return TXQUEUE_E_FULL;
    }

    struct opq_item *itm = &q->items[q->wridx];
    assert(!itm->data); // caller forgot to free?
    itm->data = data;
    itm->size = size;
    itm->offset = 0;

    q->wridx = (q->wridx + 1) % ARRAY_LEN(q->items);
    q->totsize += itm->size;
    return 0;
}
#endif

/// "peek" on tail
struct opq_item *opq_acquire_tail(struct opq *q)
{
    if (opq_isfull(q)) {
        return NULL;
    }

    return &q->items[q->wridx];
}

int opq_enqueue_val(struct opq *q, uint16_t op_code, int val)
{
    struct opq_item *itm = opq_acquire_tail(q);
    assert(itm);
    itm->op_code = op_code;
    itm->size = 0;
    itm->u.val = val;

    return opq_enqueue_tail(q, itm);
}

int opq_enqueue_write(struct opq *q,
                      void *data,
                      uint16_t size)
{
    struct opq_item *itm = opq_acquire_tail(q);
    assert(itm);
    assert(size);

    itm->op_code = OP_PORT_WRITE;
    itm->size = size;
    itm->u.data = data;

    return opq_enqueue_tail(q, itm);
}

int opq_enqueue_tail(struct opq *q, struct opq_item *itm)
{
    if (opq_isfull(q)) {
        return -1;
    }

    assert(itm == &q->items[q->wridx]);
    q->wridx = (q->wridx + 1) % ARRAY_LEN(q->items);
    return 0;
}
#if 0
/// dequeue, 'read'
static int opq_pop(struct opq *q, struct opq_item *itm)
{
    if (opq_isempty(q)) {
        return TXQUEUE_E_EMPTY;
    }
    // need to copy here as consumed
    *itm = q->items[q->rdidx];

    q->rdidx = (q->rdidx + 1) % ARRAY_LEN(q->items);
    q->totsize -= itm->size;
    return 0;
}
#endif

/**
 * peek at head of queue without updating index.
 * assume passed to free_head later */
struct opq_item *opq_acquire_head(struct opq *q)
{
    if (opq_isempty(q)) {
        return NULL;
    }

    return &q->items[q->rdidx];
}

/// release or free item retrived with peek_head
void opq_release_head(struct opq *q, struct opq_item *itm)
{
    // enusre itm is same as head
    assert(itm == &q->items[q->rdidx]);

    if (itm->size) {
        if (itm->op_code == OP_PORT_WRITE && q->write_done_cb) {
            q->write_done_cb(itm);
        }

        itm->size = 0;
    }
    itm->op_code = 0;
    itm->u = (typeof(itm->u)) {0};

    // update "head"
    q->rdidx = (q->rdidx + 1) % ARRAY_LEN(q->items);
    //q->totsize -= itm->size;
}

void opq_release_all(struct opq *q)
{
    while(!opq_isempty(q)) {
        struct opq_item *itm = opq_acquire_head(q);
        opq_release_head(q, itm);
   }
}

