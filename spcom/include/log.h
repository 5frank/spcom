#ifndef __LOG_INCLUDE_H__
#define __LOG_INCLUDE_H__

/// see magic trick in CMakeList.txt 
#ifdef SOURCE_PATH_SIZE
#define __FILENAME__ (__FILE__ + SOURCE_PATH_SIZE)
#else
#define __FILENAME__ __FILE__
#endif

enum log_tag_e {
    LOG_TAG_ERR = 1,
    LOG_TAG_WRN = 2,
    LOG_TAG_INF = 3,
    LOG_TAG_DBG = 4
};
__attribute__((format(printf, 3, 4)))
void log_printf(int tag, const char *where, const char *fmt, ...);

#define __LOG(TAG, FMT, ...) \
    log_printf(TAG, LOG_WHERESTR(), FMT, ##__VA_ARGS__)

#define LOG_ERR(FMT, ...)  \
    __LOG(LOG_TAG_ERR, FMT, ##__VA_ARGS__)

#define LOG_WRN(FMT, ...) \
    __LOG(LOG_TAG_WRN, FMT, ##__VA_ARGS__)

#define LOG_INF(FMT, ...) \
    __LOG(LOG_TAG_INF, FMT, ##__VA_ARGS__)

#define LOG_DBG(FMT, ...) \
    __LOG(LOG_TAG_DBG, FMT, ##__VA_ARGS__)


const char *log_wherestr(const char *file, unsigned int line, const char *func);
#define LOG_WHERESTR() log_wherestr(__FILENAME__, __LINE__, __func__)


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
void log_set_debug(int verbose);
#endif
