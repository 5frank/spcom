/**
 * ANSI Escape sequences - VT100 / VT52 aka "(ANSI Escape codes)"
 * prefixed VT because it is shorter.
*/
#ifndef __VT_DEFS_INCLUDE_H__
#define __VT_DEFS_INCLUDE_H__

// clang-format off
#define VT_COLOR_OFF        "\001\x1B[0m\002"
#define VT_COLOR_RED        "\001\x1B[0;91m\002"
#define VT_COLOR_GREEN      "\001\x1B[0;92m\002"
#define VT_COLOR_YELLOW     "\001\x1B[0;93m\002"
#define VT_COLOR_BLUE       "\001\x1B[0;94m\002"
#define VT_COLOR_BOLDGRAY   "\001\x1B[1;30m\002"
#define VT_COLOR_BOLDWHITE  "\001\x1B[1;37m\002"
#define VT_COLOR_HIGHLIGHT  "\001\x1B[1;39m\002"
// clang-format on

/// VT100 control-key to printable (uppper case) character
#define VT_CTRL_CHR(K) ((unsigned char)(K) | 0x60)
/** printable character to VT100 control-key (case insensitive as case bit is
 * masked) */
#define VT_CTRL_KEY(C) ((unsigned char)(C) & 0x1f)

#define VT_ESC     (0x1B)  //  ESC (escape) same as ASCII ESC

/// LINE_MAX might be here
#include <limits.h> 

#ifndef LINE_MAX 
#define LINE_MAX _POSIX2_LINE_MAX
#endif


#endif
