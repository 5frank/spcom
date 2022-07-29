#ifndef __LOG_INCLUDE_H__
#define __LOG_INCLUDE_H__

#include <stdbool.h>
#include <stdarg.h>

#ifndef DEBUG
#define DEBUG 0
#endif

/// see magic trick in CMakeList.txt
#ifdef SOURCE_PATH_SIZE
#define __FILENAME__ (__FILE__ + SOURCE_PATH_SIZE)
#else
#define __FILENAME__ __FILE__
#endif

#define LOG_LEVEL_ERR 1
#define LOG_LEVEL_WRN 2
#define LOG_LEVEL_INF 3
#define LOG_LEVEL_DBG 4

void log_vprintf(int level, const char *where, const char *fmt, va_list args);

__attribute__((format(printf, 3, 4)))
void log_printf(int level, const char *where, const char *fmt, ...);

#define __LOG(LEVEL, FMT, ...) \
    log_printf(LEVEL, LOG_WHERESTR(), FMT, ##__VA_ARGS__)

#define LOG_ERR(FMT, ...)  \
    __LOG(LOG_LEVEL_ERR, FMT, ##__VA_ARGS__)

#define LOG_WRN(FMT, ...) \
    __LOG(LOG_LEVEL_WRN, FMT, ##__VA_ARGS__)

#define LOG_INF(FMT, ...) \
    __LOG(LOG_LEVEL_INF, FMT, ##__VA_ARGS__)

#define LOG_DBG(FMT, ...) \
    __LOG(LOG_LEVEL_DBG, FMT, ##__VA_ARGS__)

const char *log_wherestr(const char *file, unsigned int line, const char *func);
#define LOG_WHERESTR() \
    ((DEBUG) ? log_wherestr(__FILENAME__, __LINE__, __func__) : NULL)

const char *log_errnostr(int eno);

#define LOG_SYS_ERR(RC, MSG) \
    LOG_ERR("%s rc=%d - %s", MSG, (int)(RC), log_errnostr(errno))

const char *log_libuv_errstr(int uvrc, int eno);
#define LOG_UV_ERR(VAL, MSG) \
    LOG_ERR("%s - %s", MSG, log_libuv_errstr(VAL, errno))

/// libserialport (sp)
const char *log_libsp_errstr(int sprc, int eno);
#define LOG_SP_ERR(VAL, MSG) \
    LOG_ERR("%s - %s", MSG, log_libsp_errstr(VAL, errno))

const char *log_uv_handle_type_to_str(int n);

void log_init(void);
void log_cleanup(void);
#endif
