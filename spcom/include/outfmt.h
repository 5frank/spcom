#ifndef OUTFMT_INCLUDE_H_
#define OUTFMT_INCLUDE_H_

#include <stddef.h>

/**
 * format output. if no output format options is set, which might not be the
 * default, this function should be same as calling fwrite()
 */
void outfmt_write(const void *data, size_t size);

/**
 * end line if last char(s) to output was not new line (eol)
*/
void outfmt_endline(void);

#endif
