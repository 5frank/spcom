#include <uv.h>
#include <libserialport.h>
#include <sysexits.h>
#include <readline/readline.h>

#include "misc.h"

int misc_print_version(int verbose)
{
    //TODO if (verbose)
    printf("libuv: %s\n", uv_version_string());
    printf("libserialport: lib:%s, pkg:%s\n", sp_get_lib_version_string(), sp_get_package_version_string());

    //printf("readline: %d.%d\n", RL_VERSION_MAJOR, RL_VERSION_MINOR);
    printf("readline: %s\n", rl_library_version);

#ifdef __GNUC__ // || defined(__GNUG__)

#ifndef __GNUC_MAJOR__
#define __GNUC_MAJOR__  __GNUC__
#endif

    printf("gcc: %d.%d.%d\n", __GNUC_MAJOR__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#endif

#ifdef __clang__
    printf("clang: %d.%d.%d\n", __clang_major__, __clang_minor__, __clang_patchlevel__);
#endif

#ifdef _MSC_VER
    printf("visual studio: %d\n",  _MSC_VER);
#endif
    return 0;
}

int misc_sp_err_to_exit_code(int sp_err)
{
    switch (sp_err) {
        case SP_OK:
            return EX_OK;
        // Invalid arguments. This implies a bug in the caller
        case SP_ERR_ARG:
            return EX_SOFTWARE;
        // A system error occurred
        case SP_ERR_FAIL:
            return EX_OSERR;
        // A memory allocation failed
        case SP_ERR_MEM:
            return EX_OSERR;
        // operation not supported by OS, driver or device.
        case SP_ERR_SUPP:
            return EX_UNAVAILABLE;
        default:
            return EX_SOFTWARE;
    }
}

const char *misc_sp_err_to_str(int sp_err)
{
    static char buf[128];
    switch (sp_err) {
        // never!?
        case SP_OK:
            return "";

        // Invalid arguments. This implies a bug in the caller
        case SP_ERR_ARG:
            return "invalid argument";
            break;

        // A system error occurred
        case SP_ERR_FAIL: {

            char *tmp = sp_last_error_message();
            if (!tmp) {
                return "fail system error";
            }

            snprintf(buf, sizeof(buf), "fail '%s'", tmp);
            sp_free_error_message(tmp);

            return buf;
        }

        // A memory allocation failed
        case SP_ERR_MEM:
            return "malloc failed";

        // operation not supported by OS, driver or device.
        case SP_ERR_SUPP:
            return "operation not supported";

        default:
            return "<unknown>";
    }
}


int misc_uv_err_to_exit_code(int uv_err)
{
    switch (uv_err) {
        case 0:
            return EX_OK;

        case UV_EPERM: // operation not permitted
            return EX_NOPERM;

        case UV_EIO: // i/o error
            return EX_IOERR;

        default:
            return EX_SOFTWARE;

        // Most not applicable for this application
#if 0
        case UV_E2BIG: // argument list too long
        case UV_EACCES: // permission denied
        case UV_EADDRINUSE: // address already in use
        case UV_EADDRNOTAVAIL: // address not available
        case UV_EAFNOSUPPORT: // address family not supported
        case UV_EAGAIN: // resource temporarily unavailable
        case UV_EAI_ADDRFAMILY: // address family not supported
        case UV_EAI_AGAIN: // temporary failure
        case UV_EAI_BADFLAGS: // bad ai_flags value
        case UV_EAI_BADHINTS: // invalid value for hints
        case UV_EAI_CANCELED: // request canceled
        case UV_EAI_FAIL: // permanent failure
        case UV_EAI_FAMILY: // ai_family not supported
        case UV_EAI_MEMORY: // out of memory
        case UV_EAI_NODATA: // no address
        case UV_EAI_NONAME: // unknown node or service
        case UV_EAI_OVERFLOW: // argument buffer overflow
        case UV_EAI_PROTOCOL: // resolved protocol is unknown
        case UV_EAI_SERVICE: // service not available for socket type
        case UV_EAI_SOCKTYPE: // socket type not supported
        case UV_EALREADY: // connection already in progress
        case UV_EBADF: // bad file descriptor
        case UV_EBUSY: // resource busy or locked
        case UV_ECANCELED: // operation canceled
        case UV_ECHARSET: // invalid Unicode character
        case UV_ECONNABORTED: // software caused connection abort
        case UV_ECONNREFUSED: // connection refused
        case UV_ECONNRESET: // connection reset by peer
        case UV_EDESTADDRREQ: // destination address required
        case UV_EEXIST: // file already exists
        case UV_EFAULT: // bad address in system call argument
        case UV_EFBIG: // file too large
        case UV_EHOSTUNREACH: // host is unreachable
        case UV_EINTR: // interrupted system call
        case UV_EINVAL: // invalid argument
        case UV_EISCONN: // socket is already connected
        case UV_EISDIR: // illegal operation on a directory
        case UV_ELOOP: // too many symbolic links encountered
        case UV_EMFILE: // too many open files
        case UV_EMSGSIZE: // message too long
        case UV_ENAMETOOLONG: // name too long
        case UV_ENETDOWN: // network is down
        case UV_ENETUNREACH: // network is unreachable
        case UV_ENFILE: // file table overflow
        case UV_ENOBUFS: // no buffer space available
        case UV_ENODEV: // no such device
        case UV_ENOENT: // no such file or directory
        case UV_ENOMEM: // not enough memory
        case UV_ENONET: // machine is not on the network
        case UV_ENOPROTOOPT: // protocol not available
        case UV_ENOSPC: // no space left on device
        case UV_ENOSYS: // function not implemented
        case UV_ENOTCONN: // socket is not connected
        case UV_ENOTDIR: // not a directory
        case UV_ENOTEMPTY: // directory not empty
        case UV_ENOTSOCK: // socket operation on non-socket
        case UV_ENOTSUP: // operation not supported on socket
        case UV_EOVERFLOW: // value too large for defined data type
        case UV_EPIPE: // broken pipe
        case UV_EPROTO: // protocol error
        case UV_EPROTONOSUPPORT: // protocol not supported
        case UV_EPROTOTYPE: // protocol wrong type for socket
        case UV_ERANGE: // result too large
        case UV_EROFS: // read-only file system
        case UV_ESHUTDOWN: // cannot send after transport endpoint shutdown
        case UV_ESPIPE: // invalid seek
        case UV_ESRCH: // no such process
        case UV_ETIMEDOUT: // connection timed out
        case UV_ETXTBSY: // text file is busy
        case UV_EXDEV: // cross-device link not permitted
        case UV_UNKNOWN: // unknown error
        case UV_EOF: // end of file
        case UV_ENXIO: // no such device or address
        case UV_EMLINK: // too many links
        case UV_ENOTTY: // inappropriate ioctl for device
        case UV_EFTYPE: // inappropriate file type or format
        case UV_EILSEQ: // illegal byte sequence
        case UV_ESOCKTNOSUPPORT: // socket type not supported
#endif
    }
}

const char *misc_uv_err_to_str(int uv_err)
{
    static char buf[64];
    buf[0] = '\0';

    uv_err_name_r(uv_err, buf, sizeof(buf));

    return buf;
}

const char *misc_uv_handle_type_to_str(int n)
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
