#ifndef LOG_INCLUDE_H_
#define LOG_INCLUDE_H_

#include <stdbool.h>
#include <stdarg.h>
#include "misc.h"
#include "common.h"

#ifndef DEBUG
#define DEBUG 0
#endif

#define LOG_LEVEL_ERR 1
#define LOG_LEVEL_WRN 2
#define LOG_LEVEL_INF 3
#define LOG_LEVEL_DBG 4

void log_vprintf(int level,
                 const char *file,
                 unsigned int line,
                 const char *fmt,
                 va_list args);

__attribute__((format(printf, 4, 5)))
void log_printf(int level,
                const char *file,
                unsigned int line,
                const char *fmt,
                ...);

#define ___LOG(LEVEL, FMT, ...)                                                 \
    log_printf(LEVEL, __FILENAME__, __LINE__, FMT, ##__VA_ARGS__)

#define LOG_ERR(FMT, ...) ___LOG(LOG_LEVEL_ERR, FMT, ##__VA_ARGS__)

#define LOG_WRN(FMT, ...) ___LOG(LOG_LEVEL_WRN, FMT, ##__VA_ARGS__)

#define LOG_INF(FMT, ...) ___LOG(LOG_LEVEL_INF, FMT, ##__VA_ARGS__)

#define LOG_DBG(FMT, ...) ___LOG(LOG_LEVEL_DBG, FMT, ##__VA_ARGS__)


static inline void ___log_int(int level,
                              const char *file,
                              unsigned int line,
                              const char *msg,
                              int num,
                              const char *(*num_to_str)(int num))
{
    const char *num_str = num_to_str(num);
    num_str = num_str ? num_str : "";

    log_printf(level, file, line, "%s - %d (%s)",
               msg, num, num_str);
}

#define ___LOG_INT(LEVEL, MSG, NUM, NUM_TO_STR) \
    ___log_int(LEVEL, __FILENAME__, __LINE__, MSG, NUM, NUM_TO_STR)

#define LOG_ERRNO(ERRNUM, MSG) \
    ___LOG_INT(LOG_LEVEL_ERR, MSG, ERRNUM, strerrorname_np)

/// libuv (uv)
#define LOG_UV_ERR(UV_ERR, MSG) \
    ___LOG_INT(LOG_LEVEL_ERR, MSG, UV_ERR, misc_uv_err_to_str)

/// libserialport (sp)
#define LOG_SP_ERR(SP_ERR, MSG) \
    ___LOG_INT(LOG_LEVEL_ERR, MSG, SP_ERR, misc_sp_err_to_str)




void log_init(void);
void log_cleanup(void);
#endif
