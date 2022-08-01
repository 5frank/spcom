/**
 * Should be inlcluded by compiler to ensure these defs are defined before any
 * library headers included.
 *
*/

#ifndef PRE_DEFS_INCLUDE_H_
#define PRE_DEFS_INCLUDE_H_

//#define _GNU_SOURCE 1

/**
 * Kudos: Jonathan Leffler - https://stackoverflow.com/a/3552156
 *
 * Include this file before including system headers.  By default, with
 * C99 support from the compiler, it requests POSIX 2001 support.  With
 * C89 support only, it requests POSIX 1997 support.  Override the
 * default behaviour by setting either _XOPEN_SOURCE or _POSIX_C_SOURCE.
 *
 * @{
 */

/* _XOPEN_SOURCE 700 is loosely equivalent to _POSIX_C_SOURCE 200809L */
/* _XOPEN_SOURCE 600 is loosely equivalent to _POSIX_C_SOURCE 200112L */
/* _XOPEN_SOURCE 500 is loosely equivalent to _POSIX_C_SOURCE 199506L */

#if !defined(_XOPEN_SOURCE) && !defined(_POSIX_C_SOURCE)

#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600   /* SUS v3, POSIX 1003.1 2004 (POSIX 2001 + Corrigenda) */
#else
#define _XOPEN_SOURCE 500   /* SUS v2, POSIX 1003.1 1997 */
#endif /* __STDC_VERSION__ */
#endif /* !_XOPEN_SOURCE && !_POSIX_C_SOURCE */

/** @} end group */

/// LINE_MAX might be here
#include <limits.h>
#ifndef LINE_MAX

#ifdef _POSIX2_LINE_MAX
    #define LINE_MAX _POSIX2_LINE_MAX
#else
    /* _POSIX2_LINE_MAX must be at least 2048 */
    #define LINE_MAX    2048
#endif
#endif /* LINE_MAX */

#endif /* PRE_DEFS_INCLUDE_H_ */

