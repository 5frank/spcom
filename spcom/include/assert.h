/**
 * Override system assert.h. if this cauese a problem it is intentional as we
 * it indicates that the system assert.h was included.  Want to ensure the
 * applications exit handler is called and nothing else.  Also this seems to be
 * more portable way then guessing the implementation of `__assert_fail()`
 *
 * Note that this assert is not affected by NDEBUG but the verbosity of error
 * messages generated might.
 */
#ifndef ASSERT_INCLUDE_H_
#define ASSERT_INCLUDE_H_

#include <errno.h>
#include "common.h"
#include "misc.h"
#include "log.h"

#define assert(COND)                                                           \
    do {                                                                       \
        if (!(COND)) {                                                         \
            SPCOM_EXIT(EX_SOFTWARE, "assert '%s' failed", STRINGIFY(COND));    \
        }                                                                      \
    } while (0)

/// assert zero
#define assert_z(VAL, MSG)                                                     \
    do {                                                                       \
        int __val = (VAL);                                                     \
        if (__val) {                                                           \
            SPCOM_EXIT(EX_SOFTWARE, "assert failed, "                          \
                       "'%d' is not zero, %s",                                 \
                       __val, (MSG));                                          \
        }                                                                      \
    } while (0)

__attribute__((noreturn))
void ___assert_uv_fail(const char *file,
                       unsigned int line,
                       const char *func,
                       const char *msg,
                       int uv_err);

/// assert libuv succesful return code
#define assert_uv_ok(ERR, MSG)                                                 \
    do {                                                                       \
        int _err = (ERR);                                                      \
        if (_err) {                                                            \
            ___assert_uv_fail(__FILENAME__, __LINE__, __func__, MSG, _err);    \
        }                                                                      \
    } while (0)

__attribute__((noreturn))
void ___assert_sp_fail(const char *file,
                       unsigned int line,
                       const char *func,
                       const char *msg,
                       int sp_err);

/// assert libserialport (sp) succesful return code
#define assert_sp_ok(ERR, MSG)                                                 \
    do {                                                                       \
        int _err = (ERR);                                                      \
        if (_err) {                                                            \
            ___assert_sp_fail(__FILENAME__, __LINE__, __func__, MSG, _err);    \
        }                                                                      \
    } while (0)

#endif

