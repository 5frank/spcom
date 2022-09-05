/**
 * Should be inlcluded by compiler to ensure these defs are defined before any
 * library headers included.
*/

#ifndef PRE_DEFS_INCLUDE_H_
#define PRE_DEFS_INCLUDE_H_


/** Enable libc extensions. There is also other defines that might control
 * this but safe to assume they are deprecated? example:
 *  - `__USE_GNU`     - same as _GNU_SOURCE
 *  - `__EXTENSIONS__`  - 1 same as _GNU_SOURCE!?
 *  - `_BSD_SOURCE`  - features.h might have a warning "_BSD_SOURCE and
 *                  _SVID_SOURCE are deprecated, use _DEFAULT_SOURCE"
*/
#define _GNU_SOURCE 1
#define _DEFAULT_SOURCE 1

/// __GLIBC__ and __GLIBC_MINOR__
#include <features.h>

/// LINE_MAX might be in limits.h
#include <limits.h>

#if defined(__GLIBC__) && defined(_GNU_SOURCE)
    #define HAVE_GNU_SOURCE 1
#else
    #define HAVE_GNU_SOURCE 0
#endif

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

/* _XOPEN_SOURCE 700 is loosely equivalent to _POSIX_C_SOURCE 200809L
 * _XOPEN_SOURCE 600 is loosely equivalent to _POSIX_C_SOURCE 200112L
 * _XOPEN_SOURCE 500 is loosely equivalent to _POSIX_C_SOURCE 199506L
 */

#if !defined(_XOPEN_SOURCE) && !defined(_POSIX_C_SOURCE)
    #if __STDC_VERSION__ >= 199901L
        /* SUS v3, POSIX 1003.1 2004 (POSIX 2001 + Corrigenda) */
        #define _XOPEN_SOURCE 600
    #else
        /* SUS v2, POSIX 1003.1 1997 */
        #define _XOPEN_SOURCE 500
    #endif /* __STDC_VERSION__ */
#endif /* !_XOPEN_SOURCE && !_POSIX_C_SOURCE */

/** @} end group */


#ifndef LINE_MAX
    #ifdef _POSIX2_LINE_MAX
        #define LINE_MAX _POSIX2_LINE_MAX
    #else
        /* _POSIX2_LINE_MAX Must be at least 2048 */
        #define LINE_MAX    2048
    #endif
#endif /* LINE_MAX */



/// strerrorname_np in glibc >= 2.32 and in <string.h>
#ifdef HAVE_GNU_SOURCE
    #if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 32
        #define HAVE_STRERRORNAME_NP 1
    #endif
#endif

#endif /* PRE_DEFS_INCLUDE_H_ */

