/** bit "field" table of any width.
 * can be allocated static or dynamic
 * */
#ifndef BTABLE_INCLUDE_H_
#define BTABLE_INCLUDE_H_

#include <stdlib.h>
#include <string.h>

typedef unsigned int btable_t;
/** number of bits per word */
#define BTABLE_WORD_NBITS (sizeof(btable_t) * 8)
/** data word offset */
#define BTABLE_WORD_OFFSET(INDEX) ((INDEX) / BTABLE_WORD_NBITS)

/** bit position in word! */
#define BTABLE_BITPOS(INDEX) ((INDEX) & (BTABLE_WORD_NBITS - 1))

/** word mask. assume offset applied */
#define BTABLE_WORD_MASK(INDEX) (1 << BTABLE_BITPOS(INDEX))

/** number of bits to number of data words (of type btable_t).
 * round up to closest to ensure at least NBITS will fit.
 *
 * example usage for static allocation:
 *   static btable_t my_btable[BTABLE_NBITS_TO_NWORDS(123)];
 * */
#define BTABLE_NBITS_TO_NWORDS(NBITS)                                          \
    (((NBITS) + BTABLE_WORD_NBITS - 1) / BTABLE_WORD_NBITS)

/** @param NBITS - number of bits
 */
#define BTABLE_NBITS_TO_SIZE(NBITS)                                            \
    (BTABLE_NBITS_TO_NWORDS(NBITS) * sizeof(btable_t))

/** note might be more then nbits (number of bits) used as argument to
 * initalization macros above if nbits was not a power of two */
#define BTABLE_SIZE_TO_MAX_NBITS(SIZE)                                         \
    (((SIZE) / sizeof(btable_t)) * BTABLE_WORD_NBITS)

static inline btable_t *btable_create(size_t nbits)
{
    return calloc(BTABLE_NBITS_TO_NWORDS(nbits), sizeof(btable_t));
}

static inline void btable_free(btable_t *table) { free(table); }

/** reset all bits to 1 or 0 */
static inline void btable_reset(btable_t *table, int bitval, size_t nbits)
{
    size_t size = BTABLE_NBITS_TO_SIZE(nbits);
    memset(table, bitval ? 0xff : 0, size);
}

/** clear bit at index. assumes index in range */
static inline void btable_set(btable_t *table, unsigned int index)
{
    unsigned int offs = BTABLE_WORD_OFFSET(index);
    table[offs] |= BTABLE_WORD_MASK(index);
}

/** clear bit at index. assumes index in range */
static inline void btable_clr(btable_t *table, unsigned int index)
{
    unsigned int offs = BTABLE_WORD_OFFSET(index);
    table[offs] &= ~(BTABLE_WORD_MASK(index));
}

/** get bit value at index. assumes index in range */
static inline int btable_get(const btable_t *table, unsigned int index)
{
    unsigned int offs = BTABLE_WORD_OFFSET(index);
    return !!(table[offs] & BTABLE_WORD_MASK(index));
}

#endif
