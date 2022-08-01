#ifndef CHARMAP_INCLUDE_H_
#define CHARMAP_INCLUDE_H_

#include <limits.h>
#include <stdint.h>
#include <stdbool.h>

/// max buf size needed for charmap_remap buf
#define CHARMAP_REPR_BUF_SIZE 8

struct charmap_s;

enum char_repr_id {
    CHARMAP_REPR_NONE = -1,
    CHARMAP_REPR_BASE = UCHAR_MAX + 1,
    CHARMAP_REPR_IGNORE,
    CHARMAP_REPR_HEX,
    CHARMAP_REPR_CNTRLNAME,
    CHARMAP_REPR_STRLIT_BASE,
};
/// @return CHARMAP_REPR_{XXX}
int charmap_repr_type(const struct charmap_s *cm, int c);

static inline bool charmap_is_remapped(const struct charmap_s *cm, int c)
{
    return charmap_repr_type(cm, c) != CHARMAP_REPR_NONE;
}

/// NULL if nothing remaped
extern const struct charmap_s *charmap_tx;
/// NULL if nothing remaped
extern const struct charmap_s *charmap_rx;
/**
 * @param buf must be of size CHARMAP_REPR_BUF_SIZE or more.
 *  will be nul terminated
 * @return length of string written to buf
 * */
int charmap_remap(const struct charmap_s *cm,
                  char c,
                  char *buf);


#endif
