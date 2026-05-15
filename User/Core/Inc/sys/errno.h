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
#ifndef ENODATA
#define ENODATA         61
#endif
#ifndef ETIMEDOUT
#define ETIMEDOUT       110
#endif
#ifndef ENOBUFS
#define ENOBUFS         105
#endif

#endif
