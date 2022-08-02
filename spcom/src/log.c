#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include <uv.h>
#include <libserialport.h>

#include "strbuf.h"
#include "opt.h"
#include "shell.h"
#include "common.h"
#include "log.h"

/**
 * assert RC is greater then zero (printf returns negative on error)
 * log module should not use log itself or application custom assert as exit
 * handler might call log_printf() resulting in recursion loop. Must run
 * cleanup code so can not use libc assert. Alternative soulution: ensure
 * assert and exit handler do not use the log module */
#define check_printfrc(RC)                                                     \
    do {                                                                       \
        int _rc = (RC);                                                        \
        if (_rc < 0) {                                                         \
            fprintf(stderr,                                                    \
              "ERR: (sn)printf retval '%d' in log module func:%s at line %d\n",\
               _rc, __func__, __LINE__);                                       \
        }                                                                      \
    } while(0)

#define LOG_OPTS_DEFAULT_LEVEL 3

struct log_opts_s {
    const char *file;
    int level;
    int silent;
} log_opts = {
    .level = LOG_OPTS_DEFAULT_LEVEL
};

static struct log_data_s {
    bool initialized;
    FILE *filefp;
    int level;
} log_data = { 0 };


static void log_sb_fwrite_nolock(const struct strbuf *sb, FILE *fp)
{
    size_t rc = fwrite(sb->buf, 1, sb->len, fp);
    if (rc != sb->len) {
        // TODO do what?
    }
}

/* should only be called from strbuf if buf to small */
static void log_strbuf_flush(struct strbuf *sb)
{
    const void *lock = shell_output_lock();

    /* silent option do not apply to separate file output */
    if (log_data.filefp) {
        log_sb_fwrite_nolock(sb, log_data.filefp);
    }

    /* output both to file and stderr in some cases */
    if (!log_opts.silent && log_data.level <= LOG_LEVEL_INF) {
        log_sb_fwrite_nolock(sb, stderr);
    }

    shell_output_unlock(lock);

    sb->len = 0;
}

STRBUF_STATIC_INIT(log_strbuf, 1024, log_strbuf_flush);

static const char *log_level_to_str(int level)
{
    switch (level) {
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

void log_vprintf(int level,
                 const char *file,
                 unsigned int line,
                 const char *fmt,
                 va_list args)
{
    /* in case some other module uses log module before log is initialized, do it here.
     * otherwise hard reason about initalization order always correct */
    if (!log_data.initialized) {
        log_init();
    }

    if (level > log_opts.level) {
        return;
    }

    struct strbuf *sb = &log_strbuf;
    strbuf_reset(sb);
    log_data.level = level; // used in strbuf callback

    const char *levelstr = log_level_to_str(level);
    if (levelstr) {
        strbuf_puts(sb, levelstr);
        strbuf_putc(sb, ':');
    }

    if (file) {
        strbuf_puts(sb, file);
        strbuf_putc(sb, ':');
        strbuf_printf(sb, "%u:", line);
    }

    strbuf_putc(sb, ' ');
    strbuf_vprintf(sb, fmt, args);
    strbuf_putc(sb, '\n');

    log_strbuf_flush(sb);
}

void log_printf(int level,
                const char *file,
                unsigned int line,
                const char *fmt,
                ...)
{
    va_list args;
    va_start(args, fmt);
    log_vprintf(level, file, line, fmt, args);
    va_end(args);
}

static void _sp_log_handler(const char *fmt, ...)
{
    FILE *fp = log_data.filefp;
    if (!fp)
        return;
    va_list args;

    const void *lock = shell_output_lock();

    fputs("DBG:SP:", fp);
    va_start(args, fmt);
    int rc = vfprintf(fp, fmt, args);
    va_end(args);
    (void) rc;

    shell_output_unlock(lock);
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

void log_init(void)
{
    if (log_data.initialized) {
        return;
    }

    int level = log_opts.level;

    log_data.filefp = NULL;

    const char *fpath = log_opts.file;
    if (fpath) {
        FILE *fp = fopen(fpath, "w");
        if (!fp) {
            /* errnos set are same for fopen and open.
             * usage error as probably permission erros */
            int exit_code = (errno == EACCES) ? EX_NOPERM : EX_USAGE;
            SPCOM_EXIT(exit_code,
                        "Failed to open log file %s '%s'",
                        fpath, strerror(errno));
            return;
        }
        //setlinebuf(fp); // TODO needed and correct?
        log_data.filefp = fp;
    }

    // TODO set debug for this application
    if (level == 0) {
        //
    }
    if (level > LOG_LEVEL_DBG) {
        // very verbose
        sp_set_debug_handler(_sp_log_handler);
    }

    log_data.initialized = true;
}

void log_cleanup(void)
{
    if (!log_data.initialized) {
        return;
    }

    FILE *fp = log_data.filefp;
    if ((fp != stderr) && (fp != stdout)) {
        return;
        fclose(fp);
        log_data.filefp = NULL;
    }

    log_data.initialized = false;
}


static const struct opt_conf log_opts_conf[] = {
    {
        .name = "loglevel",
        .dest = &log_opts.level,
        .parse = opt_ap_int,
        .descr = "log verbosity level. "
            "Error: " STRINGIFY(LOG_LEVEL_ERR) ", "
            "Warn: " STRINGIFY(LOG_LEVEL_WRN) ", "
            "Info: " STRINGIFY(LOG_LEVEL_INF) ", "
            "Debug: >=" STRINGIFY(LOG_LEVEL_DBG) ", "
            "Default: " STRINGIFY(LOG_OPTS_DEFAULT_LEVEL)
    },
    {
        .name = "logfile",
        .dest = &log_opts.file,
        .parse = opt_ap_str,
    },
    {
        .name = "silent",
        .dest = &log_opts.silent,
        .parse = opt_ap_flag_true,
        .descr = "Only serial port data written to stdout. Error message(s) suppressed"
    },
};

static int log_opts_post_parse(const struct opt_section_entry *entry)
{
    return 0;
}

OPT_SECTION_ADD(log, log_opts_conf, ARRAY_LEN(log_opts_conf), NULL);

