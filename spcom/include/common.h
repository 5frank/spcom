#ifndef COMMON_INCLUDE_H__
#define COMMON_INCLUDE_H__

#include <stdio.h>
#include <stdbool.h>
#include <sysexits.h>
#include <string.h>

#include "assert.h"

#ifdef _WIN32
#include <io.h>
#define isatty(FD) _isatty(FD)
#define fileno(F) _fileno(F)
//define sysexit.h
// #define STDOUT_FILENO ?
#else
#include <unistd.h>
#endif

/// see magic trick in CMakeList.txt
#ifdef SOURCE_PATH_SIZE
#define __FILENAME__ (__FILE__ + SOURCE_PATH_SIZE)
#else
#define __FILENAME__ __FILE__
#endif

__attribute__((always_inline))
static inline const char *___filebasename(const char *strrchr_res, const char *file)
{
    return strrchr_res ? (strrchr_res  + 1) : file;
}

/** evaluate strrchr once - should be optimized at compile time */
#define FILEBASENAME() ___filebasename(strrchr(__FILENAME__, '/'), __FILENAME__)

#ifndef _GNU_SOURCE // TODO proper test macro!!!
/**
 * Provide if not _GNU_SOURCE
 * The strerrorname_np()
 * returns literal name of errnum.  For example, EPERM argument returns "EPERM".
 * returns NULL if unknown errnum
 * */
const char *strerrorname_np(int errnum);

#endif


/** UV_VERSION_HEX "broken" on version 1.24.1 and is 0x011801 i.e. 1.18.01 */
#define UV_VERSION_GT_OR_EQ(MAJOR, MINOR)                                      \
    ((UV_VERSION_MAJOR > MAJOR)                                                \
     || ((UV_VERSION_MAJOR == MAJOR) && (UV_VERSION_MINOR >= MINOR)))

/**
 * This macro is commonly named `ARRAY_SIZE` - it have always anoyed me as the
 * word 'size' implies bytes (chars). Compare with strlen etc where "len" or "length" is
 * is have a more ambiguous meaning - i.e. _not_ in bytes
 */
#define ARRAY_LEN(ARRAY) (sizeof(ARRAY) / sizeof((ARRAY)[0]))

#define STRINGIFY(X) ___STRINGIFY_VIA(X)
#define ___STRINGIFY_VIA(X) #X


__attribute__((noreturn, format(printf, 4, 5)))
void spcom_exit(int exit_code,
                const char *file,
                unsigned int line,
                const char *fmt,
                ...);

#define SPCOM_EXIT(EXIT_CODE, FMT, ...)                                       \
        spcom_exit(EXIT_CODE,  __FILENAME__, __LINE__, FMT, ##__VA_ARGS__)


#if CONFIG_TERMIOS_DEBUG
void ___termios_debug_before(void);
void ___termios_debug_after(const char *what);

#define ___TERMIOS_DEBUG_BEFORE()    ___termios_debug_before()
#define ___TERMIOS_DEBUG_AFTER(WHAT) ___termios_debug_after(WHAT)
#else
#define ___TERMIOS_DEBUG_BEFORE()    do { } while (0)
#define ___TERMIOS_DEBUG_AFTER(WHAT) do { } while (0)
#endif /* CONFIG_TERMIOS_DEBUG */

#endif
