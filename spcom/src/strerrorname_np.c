#include <errno.h>
#include "common.h"

#ifndef _GNU_SOURCE

const char *strerrorname_np(int errnum)
{
    switch(errnum) {
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
        return NULL;
    }
}

#endif // _GNU_SOURCE
