#ifndef CHARMAP_INCLUDE_H_
#define CHARMAP_INCLUDE_H_

#define CHARMAP_REMAP_MAX_LEN 8

struct charmap_s;
/// NULL if nothing remaped
extern const struct charmap_s *charmap_tx;
/// NULL if nothing remaped
extern const struct charmap_s *charmap_rx;
int charmap_remap(const struct charmap_s *cm,
                  char c,
                  char *buf,
                  char **color);


#endif
