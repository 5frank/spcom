#ifndef COMMON_INCLUDE_H__
#define COMMON_INCLUDE_H__


#include <stdio.h>
#include <stdbool.h>

#include "assert.h"
#include "outfmt.h"

#ifdef _WIN32
#include <io.h>
#define isatty(FD) _isatty(FD)
#define fileno(F) _fileno(F)
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

struct global_opts_s {
    int verbose;
};

extern const struct global_opts_s *global_opts;

__attribute__((format(printf, 3, 4)))
void spcom_exit(int err, const char *where, const char *fmt, ...);


#if 1

#define SPCOM_EXIT(ERR, FMT, ...)                                              \
        spcom_exit(ERR, LOG_WHERESTR(), FMT, ##__VA_ARGS__)

#else
#define SPCOM_EXIT(ERR, FMT, ...)                                              \
    do {                                                                       \
        int _err = (ERR);                                                      \
        if (_err) {                                                            \
            /* to log and stderr - possible duplicated messages */             \
            LOG_ERR("exit %d:" FMT, _err, ##__VA_ARGS__);                      \
            /* extra new line to ensure error message on separate line */      \
            fprintf(stderr, "\nerror: %d: " FMT "\n", _err, ##__VA_ARGS__);    \
        }                                                                      \
        else {                                                                 \
            LOG_DBG("exit 0: " FMT, ##__VA_ARGS__);                            \
        }                                                                      \
        spcom_exit(_err, LOG_WHERESTR(), ##__VA_ARGS__);                       \
    } while(0)
#endif

#endif
