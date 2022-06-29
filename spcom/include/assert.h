/**
 * Override system assert.h. if this cauese a problem it is intentional as we
 * it indicates that the system assert.h was included.  Want to ensure the
 * applications exit handler is called and nothing else.  Also this seems to be
 * more portable way then guessing the implementation of `__assert_fail()`
 *
 * Note that this assert is not affected by NDEBUG but the verbosity of error
 * messages generated might.
 */
#ifndef ASSERT_INCLUDE_H__
#define ASSERT_INCLUDE_H__

#include <stdlib.h> // EXIT_FAILURE
#include <errno.h>
#include "common.h"
#include "log.h"

#ifndef STRINGIFY
#define STRINGIFY(X) ___STRINGIFY_VIA(X)
#define ___STRINGIFY_VIA(X) #X
#endif

/// might look like dual evaluation but it is not
#define assert(COND)                                                           \
    do {                                                                       \
        if (!(COND)) {                                                         \
            LOG_ERR("assert '%s' failed", STRINGIFY(COND));                    \
            spcom_exit(EXIT_FAILURE);                                          \
        }                                                                      \
    } while (0)

/// assert zero
#define assert_z(VAL, MSG)                                                     \
    do {                                                                       \
        int __val = (VAL);                                                     \
        if (__val) {                                                           \
            LOG_ERR("assert failed, '%d' is not zero, %s, %s", __val, (MSG),   \
                    log_errnostr(errno));                                      \
            spcom_exit(EXIT_FAILURE);                                          \
        }                                                                      \
    } while (0)

/// assert libuv zero return code
#define assert_uv_z(VAL, MSG)                                                  \
    do {                                                                       \
        int __val = (VAL);                                                     \
        if (__val) {                                                           \
            LOG_ERR("assert failed, '%d' is not zero, %s, %s", __val, (MSG),   \
                    log_libuv_errstr(__val, errno));                           \
            spcom_exit(EXIT_FAILURE);                                          \
        }                                                                      \
    } while (0)

/// assert libserialport (sp) zero return code
#define assert_sp_z(VAL, MSG)                                                  \
    do {                                                                       \
        int __val = (VAL);                                                     \
        if (__val) {                                                           \
            LOG_ERR("assert failed, '%d' is not zero, %s, %s", __val, (MSG),   \
                    log_libsp_errstr(__val, errno));                           \
            spcom_exit(EXIT_FAILURE);                                          \
        }                                                                      \
    } while (0)

#endif

