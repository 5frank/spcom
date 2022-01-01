/** bit "field" table of any width */
#ifndef BTABLE_INCLUDE_H_
#define BTABLE_INCLUDE_H_

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define BTABLE_WORD_NBITS (sizeof(unsigned int) * 8)
#define BTABLE_WORD_OFFSET(INDEX) (INDEX / BTABLE_WORD_NBITS)
#define BTABLE_BITPOS(INDEX) (INDEX & (BTABLE_WORD_NBITS - 1))
#define BTABLE_MASK(INDEX) (1 << BTABLE_BITPOS(INDEX))

struct btable {
    // all data typed unsigned int to avoid pad bytes 
    unsigned int nwords;
    unsigned int data[];
};

static inline struct btable *btable_create(size_t nbits)
{
    unsigned int nwords = (nbits / BTABLE_WORD_NBITS) + 1;
    size_t size = sizeof(struct btable) + nwords * sizeof(unsigned int);
    struct btable * bt = malloc(size);
    if (!bt)
        return NULL;

    bt->nwords = nwords;

    return bt;
}

static inline void btable_free(struct btable *table)
{
    free(table);
}

static inline void btable_reset(struct btable *table, int bitval)
{
    size_t size = table->nwords * sizeof(unsigned int);
    memset(table->data, bitval ? 0xff : 0, size);
}

static inline void btable_set(struct btable *table, unsigned int index)
{
    unsigned int offs = BTABLE_WORD_OFFSET(index);
    assert(offs < table->nwords);
    table->data[offs] |= BTABLE_MASK(index);
}

static inline void btable_clr(struct btable *table, unsigned int index)
{
    unsigned int offs = BTABLE_WORD_OFFSET(index);
    assert(offs < table->nwords);
    table->data[offs] &= ~(BTABLE_MASK(index));
}

static inline int btable_get(const struct btable *table, unsigned int index)
{
    unsigned int offs = BTABLE_WORD_OFFSET(index);
    assert(offs < table->nwords);
    return !!(table->data[offs] & BTABLE_MASK(index));
}

#endif

