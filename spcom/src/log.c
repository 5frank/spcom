
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <uv.h>
#include <libserialport.h>
#include "log.h"
#include "shell.h"
#include "assert.h"


struct log_opts_s log_opts = {
    .level = 3
};

static const char *_errnonamestr(int n);
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
        case LOG_TAG_ERR:
            return "ERR";
        case LOG_TAG_WRN:
            return "WRN";
        case LOG_TAG_INF:
            return "INF";
        case LOG_TAG_DBG:
            return "DBG";
        default:
            return "";
    }
}

static int _snprintf_errno(char *dest, size_t size, int eno) 
{
     return snprintf(dest, size, ", errno=%d=%s='%s'", eno, _errnonamestr(eno), strerror(eno));
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

void log_printf(int tag, const char *where, const char *fmt, ...)
{
    va_list args;

    if (tag >= log_opts.level)
        return;

    if (!logfp)
        logfp = stderr;

    struct shell_rls_s *rls = shell_rls_save();
#if 1 // behave like normal printf on log_printf(0, 0, ...)
    if (tag) {
        fputs(log_tagstr(tag), logfp);
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
    if (level > 3) {
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

static const char *_errnonamestr(int n)
{
    switch(n) {
#ifdef E2BIG
    case E2BIG:
        return "E2BIG";
#endif
#ifdef EACCES
    case EACCES:
        return "EACCES";
#endif
#ifdef EADDRINUSE
    case EADDRINUSE:
        return "EADDRINUSE";
#endif
#ifdef EADDRNOTAVAIL
    case EADDRNOTAVAIL:
        return "EADDRNOTAVAIL";
#endif
#ifdef EAFNOSUPPORT
    case EAFNOSUPPORT:
        return "EAFNOSUPPORT";
#endif
#ifdef EAGAIN
    case EAGAIN:
        return "EAGAIN (EWOULDBLOCK)";
#endif
#ifdef EALREADY
    case EALREADY:
        return "EALREADY";
#endif
#ifdef EBADE
    case EBADE:
        return "EBADE";
#endif
#ifdef EBADF
    case EBADF:
        return "EBADF";
#endif
#ifdef EBADFD
    case EBADFD:
        return "EBADFD";
#endif
#ifdef EBADMSG
    case EBADMSG:
        return "EBADMSG";
#endif
#ifdef EBADR
    case EBADR:
        return "EBADR";
#endif
#ifdef EBADRQC
    case EBADRQC:
        return "EBADRQC";
#endif
#ifdef EBADSLT
    case EBADSLT:
        return "EBADSLT";
#endif
#ifdef EBUSY
    case EBUSY:
        return "EBUSY";
#endif
#ifdef ECANCELED
    case ECANCELED:
        return "ECANCELED";
#endif
#ifdef ECHILD
    case ECHILD:
        return "ECHILD";
#endif
#ifdef ECHRNG
    case ECHRNG:
        return "ECHRNG";
#endif
#ifdef ECOMM
    case ECOMM:
        return "ECOMM";
#endif
#ifdef ECONNABORTED
    case ECONNABORTED:
        return "ECONNABORTED";
#endif
#ifdef ECONNREFUSED
    case ECONNREFUSED:
        return "ECONNREFUSED";
#endif
#ifdef ECONNRESET
    case ECONNRESET:
        return "ECONNRESET";
#endif
#ifdef EDEADLK
    case EDEADLK:
#ifdef EDEADLOCK 
    //_Static_assert(EDEADLK == EDEADLOCK, "");
#endif 
        return "EDEADLK (EDEADLOCK)";
#endif

#ifdef EDESTADDRREQ
    case EDESTADDRREQ:
        return "EDESTADDRREQ";
#endif
#ifdef EDOM
    case EDOM:
        return "EDOM";
#endif
#ifdef EDQUOT
    case EDQUOT:
        return "EDQUOT";
#endif
#ifdef EEXIST
    case EEXIST:
        return "EEXIST";
#endif
#ifdef EFAULT
    case EFAULT:
        return "EFAULT";
#endif
#ifdef EFBIG
    case EFBIG:
        return "EFBIG";
#endif
#ifdef EHOSTDOWN
    case EHOSTDOWN:
        return "EHOSTDOWN";
#endif
#ifdef EHOSTUNREACH
    case EHOSTUNREACH:
        return "EHOSTUNREACH";
#endif
#ifdef EHWPOISON
    case EHWPOISON:
        return "EHWPOISON";
#endif
#ifdef EIDRM
    case EIDRM:
        return "EIDRM";
#endif
#ifdef EILSEQ
    case EILSEQ:
        return "EILSEQ";
#endif
#ifdef EINPROGRESS
    case EINPROGRESS:
        return "EINPROGRESS";
#endif
#ifdef EINTR
    case EINTR:
        return "EINTR";
#endif
#ifdef EINVAL
    case EINVAL:
        return "EINVAL";
#endif
#ifdef EIO
    case EIO:
        return "EIO";
#endif
#ifdef EISCONN
    case EISCONN:
        return "EISCONN";
#endif
#ifdef EISDIR
    case EISDIR:
        return "EISDIR";
#endif
#ifdef EISNAM
    case EISNAM:
        return "EISNAM";
#endif
#ifdef EKEYEXPIRED
    case EKEYEXPIRED:
        return "EKEYEXPIRED";
#endif
#ifdef EKEYREJECTED
    case EKEYREJECTED:
        return "EKEYREJECTED";
#endif
#ifdef EKEYREVOKED
    case EKEYREVOKED:
        return "EKEYREVOKED";
#endif
#ifdef EL2HLT
    case EL2HLT:
        return "EL2HLT";
#endif
#ifdef EL2NSYNC
    case EL2NSYNC:
        return "EL2NSYNC";
#endif
#ifdef EL3HLT
    case EL3HLT:
        return "EL3HLT";
#endif
#ifdef EL3RST
    case EL3RST:
        return "EL3RST";
#endif
#ifdef ELIBACC
    case ELIBACC:
        return "ELIBACC";
#endif
#ifdef ELIBBAD
    case ELIBBAD:
        return "ELIBBAD";
#endif
#ifdef ELIBMAX
    case ELIBMAX:
        return "ELIBMAX";
#endif
#ifdef ELIBSCN
    case ELIBSCN:
        return "ELIBSCN";
#endif
#ifdef ELIBEXEC
    case ELIBEXEC:
        return "ELIBEXEC";
#endif
#ifdef ELNRANGE
    case ELNRANGE:
        return "ELNRANGE";
#endif
#ifdef ELOOP
    case ELOOP:
        return "ELOOP";
#endif
#ifdef EMEDIUMTYPE
    case EMEDIUMTYPE:
        return "EMEDIUMTYPE";
#endif
#ifdef EMFILE
    case EMFILE:
        return "EMFILE";
#endif
#ifdef EMLINK
    case EMLINK:
        return "EMLINK";
#endif
#ifdef EMSGSIZE
    case EMSGSIZE:
        return "EMSGSIZE";
#endif
#ifdef EMULTIHOP
    case EMULTIHOP:
        return "EMULTIHOP";
#endif
#ifdef ENAMETOOLONG
    case ENAMETOOLONG:
        return "ENAMETOOLONG";
#endif
#ifdef ENETDOWN
    case ENETDOWN:
        return "ENETDOWN";
#endif
#ifdef ENETRESET
    case ENETRESET:
        return "ENETRESET";
#endif
#ifdef ENETUNREACH
    case ENETUNREACH:
        return "ENETUNREACH";
#endif
#ifdef ENFILE
    case ENFILE:
        return "ENFILE";
#endif
#ifdef ENOANO
    case ENOANO:
        return "ENOANO";
#endif
#ifdef ENOBUFS
    case ENOBUFS:
        return "ENOBUFS";
#endif
#ifdef ENODATA
    case ENODATA:
        return "ENODATA";
#endif
#ifdef ENODEV
    case ENODEV:
        return "ENODEV";
#endif
#ifdef ENOENT
    case ENOENT:
        return "ENOENT";
#endif
#ifdef ENOEXEC
    case ENOEXEC:
        return "ENOEXEC";
#endif
#ifdef ENOKEY
    case ENOKEY:
        return "ENOKEY";
#endif
#ifdef ENOLCK
    case ENOLCK:
        return "ENOLCK";
#endif
#ifdef ENOLINK
    case ENOLINK:
        return "ENOLINK";
#endif
#ifdef ENOMEDIUM
    case ENOMEDIUM:
        return "ENOMEDIUM";
#endif
#ifdef ENOMEM
    case ENOMEM:
        return "ENOMEM";
#endif
#ifdef ENOMSG
    case ENOMSG:
        return "ENOMSG";
#endif
#ifdef ENONET
    case ENONET:
        return "ENONET";
#endif
#ifdef ENOPKG
    case ENOPKG:
        return "ENOPKG";
#endif
#ifdef ENOPROTOOPT
    case ENOPROTOOPT:
        return "ENOPROTOOPT";
#endif
#ifdef ENOSPC
    case ENOSPC:
        return "ENOSPC";
#endif
#ifdef ENOSR
    case ENOSR:
        return "ENOSR";
#endif
#ifdef ENOSTR
    case ENOSTR:
        return "ENOSTR";
#endif
#ifdef ENOSYS
    case ENOSYS:
        return "ENOSYS";
#endif
#ifdef ENOTBLK
    case ENOTBLK:
        return "ENOTBLK";
#endif
#ifdef ENOTCONN
    case ENOTCONN:
        return "ENOTCONN";
#endif
#ifdef ENOTDIR
    case ENOTDIR:
        return "ENOTDIR";
#endif
#ifdef ENOTEMPTY
    case ENOTEMPTY:
        return "ENOTEMPTY";
#endif
#ifdef ENOTRECOVERABLE
    case ENOTRECOVERABLE:
        return "ENOTRECOVERABLE";
#endif
#ifdef ENOTSOCK
    case ENOTSOCK:
        return "ENOTSOCK";
#endif
#ifdef ENOTSUP
    case ENOTSUP:

#ifdef EOPNOTSUPP
   //_Static_assert(ENOTSUP == EOPNOTSUPP, "");
#endif
        return "ENOTSUP (EOPNOTSUPP)";
#endif
#ifdef ENOTTY
    case ENOTTY:
        return "ENOTTY";
#endif
#ifdef ENOTUNIQ
    case ENOTUNIQ:
        return "ENOTUNIQ";
#endif
#ifdef ENXIO
    case ENXIO:
        return "ENXIO";
#endif
#ifdef EOVERFLOW
    case EOVERFLOW:
        return "EOVERFLOW";
#endif
#ifdef EOWNERDEAD
    case EOWNERDEAD:
        return "EOWNERDEAD";
#endif
#ifdef EPERM
    case EPERM:
        return "EPERM";
#endif
#ifdef EPFNOSUPPORT
    case EPFNOSUPPORT:
        return "EPFNOSUPPORT";
#endif
#ifdef EPIPE
    case EPIPE:
        return "EPIPE";
#endif
#ifdef EPROTO
    case EPROTO:
        return "EPROTO";
#endif
#ifdef EPROTONOSUPPORT
    case EPROTONOSUPPORT:
        return "EPROTONOSUPPORT";
#endif
#ifdef EPROTOTYPE
    case EPROTOTYPE:
        return "EPROTOTYPE";
#endif
#ifdef ERANGE
    case ERANGE:
        return "ERANGE";
#endif
#ifdef EREMCHG
    case EREMCHG:
        return "EREMCHG";
#endif
#ifdef EREMOTE
    case EREMOTE:
        return "EREMOTE";
#endif
#ifdef EREMOTEIO
    case EREMOTEIO:
        return "EREMOTEIO";
#endif
#ifdef ERESTART
    case ERESTART:
        return "ERESTART";
#endif
#ifdef ERFKILL
    case ERFKILL:
        return "ERFKILL";
#endif
#ifdef EROFS
    case EROFS:
        return "EROFS";
#endif
#ifdef ESHUTDOWN
    case ESHUTDOWN:
        return "ESHUTDOWN";
#endif
#ifdef ESPIPE
    case ESPIPE:
        return "ESPIPE";
#endif
#ifdef ESOCKTNOSUPPORT
    case ESOCKTNOSUPPORT:
        return "ESOCKTNOSUPPORT";
#endif
#ifdef ESRCH
    case ESRCH:
        return "ESRCH";
#endif
#ifdef ESTALE
    case ESTALE:
        return "ESTALE";
#endif
#ifdef ESTRPIPE
    case ESTRPIPE:
        return "ESTRPIPE";
#endif
#ifdef ETIME
    case ETIME:
        return "ETIME";
#endif
#ifdef ETIMEDOUT
    case ETIMEDOUT:
        return "ETIMEDOUT";
#endif
#ifdef ETOOMANYREFS
    case ETOOMANYREFS:
        return "ETOOMANYREFS";
#endif
#ifdef ETXTBSY
    case ETXTBSY:
        return "ETXTBSY";
#endif
#ifdef EUCLEAN
    case EUCLEAN:
        return "EUCLEAN";
#endif
#ifdef EUNATCH
    case EUNATCH:
        return "EUNATCH";
#endif
#ifdef EUSERS
    case EUSERS:
        return "EUSERS";
#endif
#ifdef EXDEV
    case EXDEV:
        return "EXDEV";
#endif
#ifdef EXFULL
    case EXFULL:
        return "EXFULL";
#endif
    default:
        return "<unknown name>";
    }
}

