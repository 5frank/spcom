/**
 * ANSI Escape sequences - VT100 / VT52 aka "(ANSI Escape codes)"
 * prefixed VT because it is shorter.
*/
#ifndef VT_DEFS_INCLUDE_H_
#define VT_DEFS_INCLUDE_H_

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

/// VT100 control-key to printable (lower case) character
#define VT_CTRL_KEY_TO_C(K) ((unsigned char)(K) | 0x60)

/** printable character to VT100 control-key (case insensitive as case bit is
 * masked) */
#define VT_C_TO_CTRL_KEY(C) ((unsigned char)(C) & 0x1F)

#define VT_ESC     (27)  //  ESC (escape) same as ASCII ESC (0x1B)


#define VT_IS_CTRL_KEY(C) iscntrl(C)

#endif

