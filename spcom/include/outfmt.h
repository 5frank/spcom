#ifndef __OUTFMT_INCLUDE_H__
#define __OUTFMT_INCLUDE_H__

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
