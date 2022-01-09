
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <uv.h>
#include <libserialport.h>
#include "opt.h"
#include "shell.h"
#include "assert.h"
#include "log.h"

struct log_opts_s {
    const char *file;
    int level;
} log_opts = {
    .level = 3
};

extern const char *errnotostr(int n);

static FILE *logfp = NULL;

static char *_truncate(char *buf, size_t size) 
{
    char *p = &buf[size - 4];
    *p++ = '.';
    *p++ = '.';
    *p++ = '.';
    *p = '\0';
    return buf;
}

static const char *log_tagstr(int tag) 
{
    switch (tag) {
        case LOG_LEVEL_ERR:
            return "ERR";
        case LOG_LEVEL_WRN:
            return "WRN";
        case LOG_LEVEL_INF:
            return "INF";
        case LOG_LEVEL_DBG:
            return "DBG";
        default:
            return "";
    }
}

static int _snprintf_errno(char *dest, size_t size, int eno) 
{
     return snprintf(dest, size, ", errno=%d=%s='%s'", eno, errnotostr(eno), strerror(eno));
}

const char *log_errnostr(int eno)
{
    static char buf[64];
    if (!eno) 
        return "";

    int n = _snprintf_errno(buf, sizeof(buf), eno);
    assert(n > 0);
    if (n >= sizeof(buf))
        return _truncate(buf, sizeof(buf));

    return buf;
}

const char *log_libuv_errstr(int uvrc, int eno)
{
    static char buf[128];
    buf[0] = '\0';
    int size = sizeof(buf);
    char *dest = &buf[0];

    static char uvstr[64];
    uv_err_name_r(uvrc, uvstr, sizeof(uvstr));

    int n = 0;
    n = snprintf(dest, size, "uv_err=%d='%s'", uvrc, uvstr);
    assert(n > 0);
    dest += n;
    size -= n;
    if (size <= 0)
        return _truncate(buf, sizeof(buf));

    if (eno) {
        n = _snprintf_errno(dest, size, eno);
        assert(n > 0);
        dest += n;
        size -= n;
        if (size <= 0)
            return _truncate(buf, sizeof(buf));
    }

    return buf;
}

const char *log_libsp_errstr(int sprc, int eno)
{
    static char buf[128];
    buf[0] = '\0';
    char *dest = &buf[0];
    int size = sizeof(buf);
    int n = 0;

    n = snprintf(dest, size, "sp_err=%d=", sprc);
    assert(n > 0);
    dest += n;
    size -= n;
    if (size <= 0)
        return _truncate(buf, sizeof(buf));

    char *tmp;
    switch (sprc) {
        case SP_ERR_ARG:
            n = snprintf(dest, size, "'invalid argument'");
            break;
        case SP_ERR_FAIL:
            tmp = sp_last_error_message();
            n = snprintf(dest, size, "'%s''", tmp);
            sp_free_error_message(tmp);
            break;
        case SP_ERR_SUPP:
            n = snprintf(dest, size, "'not supported'");
            break;
        case SP_ERR_MEM:
            n = snprintf(dest, size, "'malloc failed'");
            break;
        case SP_OK:
            n = snprintf(dest, size, "'OK''");
        default:
            n = snprintf(dest, size, "'unknown''");
            break;
    }
    assert(n > 0);
    dest += n;
    size -= n;
    if (size <= 0)
        return _truncate(buf, sizeof(buf));


    if (eno) {
        n = _snprintf_errno(dest, size, eno);
        assert(n > 0);
        dest += n;
        size -= n;
        if (size <= 0)
            return _truncate(buf, sizeof(buf));
    }

    return buf;
}

const char *log_wherestr(const char *file, unsigned int line, const char *func)
{
    static char buf[128];
    int n = snprintf(buf, sizeof(buf), "%s:%d:%s", file, line, func);
    assert(n > 0);
    if (n >= sizeof(buf))
        return _truncate(buf, sizeof(buf));
    return buf;
}

void log_printf(int level, const char *where, const char *fmt, ...)
{
    va_list args;

    if (level > log_opts.level)
        return;

    if (!logfp)
        logfp = stderr;

    struct shell_rls_s *rls = shell_rls_save();
#if 1 // behave like normal printf on log_printf(0, 0, ...)
    if (level) {
        fputs(log_tagstr(level), logfp);
        fputc(':', logfp);
    }
    if (where) {
        fputs(where, logfp);
        fputc(':', logfp);
    }
    fputc(' ', logfp);
#else
    if (!where)
        where = "";
    fprintf(logfp, "%s:%s: ", , where);
#endif

    int rc;
    va_start(args, fmt);
    rc = vfprintf(logfp, fmt, args);
    va_end(args);
    (void) rc;

    fputc('\n', logfp);

    shell_rls_restore(rls);
}

static void _sp_log_handler(const char *fmt, ...)
{
    va_list args;
    if (!logfp)
        return;

    struct shell_rls_s *rls = shell_rls_save();

    fputs("DBG:SP:", logfp);
    va_start(args, fmt);
    int rc = vfprintf(logfp, fmt, args);
    va_end(args);
    (void) rc;

    shell_rls_restore(rls);
}

void log_set_debug(int verbose) 
{
    // TODO set debug for this application
    if (verbose == 0) {
        sp_set_debug_handler(NULL);
    }
    if (verbose > 3) {
        sp_set_debug_handler(_sp_log_handler);
    }
}

const char *log_uv_handle_type_to_str(int n)
{
    // clang-format off
    switch (n) {
        case UV_UNKNOWN_HANDLE: return "UNKNOWN_HANDLE";
        case UV_ASYNC:          return "ASYNC";
        case UV_CHECK:          return "CHECK";
        case UV_FS_EVENT:       return "FS_EVENT";
        case UV_FS_POLL:        return "FS_POLL";
        case UV_HANDLE:         return "HANDLE";
        case UV_IDLE:           return "IDLE";
        case UV_NAMED_PIPE:     return "NAMED_PIPE";
        case UV_POLL:           return "POLL";
        case UV_STREAM:         return "STREAM";
        case UV_FILE:           return "FILE";
        case UV_PREPARE:        return "PREPARE";
        case UV_PROCESS:        return "PROCESS";
        case UV_TCP:            return "TCP";
        case UV_TIMER:          return "TIMER";
        case UV_TTY:            return "TTY";
        case UV_UDP:            return "UDP";
        case UV_SIGNAL:         return "SIGNAL";
        default:                return "<unknown>";
    }
    // clang-format on
}

int log_init(void) 
{

    const char *path = log_opts.file;
    int level = log_opts.level;
    if (!path) {
        logfp = stderr;
    }
    else {
        logfp = fopen(path, "w");
        if (!logfp) {
            fprintf(stderr, "ERR: failed to open log file");
            return -1;
        }
        setlinebuf(logfp);
    }
    // TODO set debug for this application
    if (level == 0) {
        //
    }
    if (level > LOG_LEVEL_DBG) {
        // very verbose
        sp_set_debug_handler(_sp_log_handler);
    }
    return 0;
}

void log_cleanup(void)
{
    if (!logfp)
        return;
    if ((logfp == stderr) || (logfp == stdout))
        return;
    fclose(logfp);
    logfp = NULL;
}


static const struct opt_conf log_opts_conf[] = {
    {
        .name = "loglevel",
        .dest = &log_opts.level,
        .parse = opt_ap_int,
    },
    {
        .name = "logfile",
        .dest = &log_opts.file,
        .parse = opt_ap_str,
    },
};

OPT_SECTION_ADD(log, log_opts_conf, ARRAY_LEN(log_opts_conf), NULL);

