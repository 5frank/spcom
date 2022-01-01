#ifndef __OUTFMT_INCLUDE_H__
#define __OUTFMT_INCLUDE_H__

#include <stdbool.h>



/** 
 * format output. if no output format options is set, which might not be the
 * default, this function should be same as calling fwrite()
 */ 
void outfmt_write(const void *data, size_t size);
/*
 *
 *
void outfmt_verbose(const void *data, size_t size);
*/
#endif
