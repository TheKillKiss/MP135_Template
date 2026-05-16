#ifndef USER_SYS_ERRNO_H__
#define USER_SYS_ERRNO_H__

#include <errno.h>

#ifndef EPERM
#define EPERM           1
#endif
#ifndef ENOENT
#define ENOENT          2
#endif
#ifndef EINTR
#define EINTR           4
#endif
#ifndef EIO
#define EIO             5
#endif
#ifndef ENXIO
#define ENXIO           6
#endif
#ifndef EAGAIN
#define EAGAIN          11
#endif
#ifndef EBADF
#define EBADF           9
#endif
#ifndef ENOMEM
#define ENOMEM          12
#endif
#ifndef EACCES
#define EACCES          13
#endif
#ifndef EFAULT
#define EFAULT          14
#endif
#ifndef EBUSY
#define EBUSY           16
#endif
#ifndef EEXIST
#define EEXIST          17
#endif
#ifndef EXDEV
#define EXDEV           18
#endif
#ifndef ENODEV
#define ENODEV          19
#endif
#ifndef ENOTDIR
#define ENOTDIR         20
#endif
#ifndef EISDIR
#define EISDIR          21
#endif
#ifndef EINVAL
#define EINVAL          22
#endif
#ifndef ENOSPC
#define ENOSPC          28
#endif
#ifndef EROFS
#define EROFS           30
#endif
#ifndef ENOSYS
#define ENOSYS          38
#endif
#ifndef EWOULDBLOCK
#define EWOULDBLOCK     EAGAIN
#endif
#ifndef ENODATA
#define ENODATA         61
#endif
#ifndef ENFILE
#define ENFILE          23
#endif
#ifndef EMSGSIZE
#define EMSGSIZE        90
#endif
#ifndef ENOPROTOOPT
#define ENOPROTOOPT     92
#endif
#ifndef EOPNOTSUPP
#define EOPNOTSUPP      95
#endif
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT    97
#endif
#ifndef EADDRINUSE
#define EADDRINUSE      98
#endif
#ifndef ECONNABORTED
#define ECONNABORTED    103
#endif
#ifndef ECONNRESET
#define ECONNRESET      104
#endif
#ifndef ENOBUFS
#define ENOBUFS         105
#endif
#ifndef EISCONN
#define EISCONN         106
#endif
#ifndef ENOTCONN
#define ENOTCONN        107
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT       110
#endif
#ifndef EHOSTUNREACH
#define EHOSTUNREACH    113
#endif
#ifndef EALREADY
#define EALREADY        114
#endif
#ifndef EINPROGRESS
#define EINPROGRESS     115
#endif

#endif
