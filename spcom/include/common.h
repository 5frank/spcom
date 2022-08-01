#ifndef COMMON_INCLUDE_H__
#define COMMON_INCLUDE_H__

#include <stdio.h>
#include <stdbool.h>
#include <sysexits.h>

#include "assert.h"
#include "outfmt.h"

#ifdef _WIN32
#include <io.h>
#define isatty(FD) _isatty(FD)
#define fileno(F) _fileno(F)
//define sysexit.h
// #define STDOUT_FILENO ?
#else
#include <unistd.h>
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

int version_print(int verbose);

/// libserialport error to exit status code
int sp_err_to_ex(int sp_err);

/// libuv error to exit status code
int uv_err_to_ex(int uv_err);
//#define EX_TIMEOUT  EX_TEMPFAIL

__attribute__((noreturn, format(printf, 4, 5)))
int spcom_exit(int exit_code,
               const char *file,
               unsigned int line,
               const char *fmt,
               ...);

#define SPCOM_EXIT(EXIT_CODE, FMT, ...)                                       \
        spcom_exit(EXIT_CODE,  __FILENAME__, __LINE__, FMT, ##__VA_ARGS__)


#endif
