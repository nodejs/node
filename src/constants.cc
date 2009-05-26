#include "node.h"
#include "constants.h"

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

using namespace v8;
using namespace node;

void
node::DefineConstants (Handle<Object> target)
{
  NODE_DEFINE_CONSTANT(target, RAW);
  NODE_DEFINE_CONSTANT(target, UTF8);
  NODE_DEFINE_CONSTANT(target, ASCII);

  NODE_DEFINE_CONSTANT(target, STDIN_FILENO);
  NODE_DEFINE_CONSTANT(target, STDOUT_FILENO);
  NODE_DEFINE_CONSTANT(target, STDERR_FILENO);

  // file access modes
  NODE_DEFINE_CONSTANT(target, O_RDONLY);
  NODE_DEFINE_CONSTANT(target, O_WRONLY);
  NODE_DEFINE_CONSTANT(target, O_RDWR);

#ifdef O_CREAT
  NODE_DEFINE_CONSTANT(target, O_CREAT);
#endif

#ifdef O_EXCL
  NODE_DEFINE_CONSTANT(target, O_EXCL);
#endif

#ifdef O_NOCTTY
  NODE_DEFINE_CONSTANT(target, O_NOCTTY);
#endif

#ifdef O_TRUNC
  NODE_DEFINE_CONSTANT(target, O_TRUNC);
#endif

#ifdef O_APPEND
  NODE_DEFINE_CONSTANT(target, O_APPEND);
#endif

#ifdef O_DIRECTORY
  NODE_DEFINE_CONSTANT(target, O_DIRECTORY);
#endif

#ifdef O_EXCL
  NODE_DEFINE_CONSTANT(target, O_EXCL);
#endif

#ifdef O_NOFOLLOW
  NODE_DEFINE_CONSTANT(target, O_NOFOLLOW);
#endif

#ifdef O_SYNC
  NODE_DEFINE_CONSTANT(target, O_SYNC);
#endif

#ifdef S_IRWXU
  NODE_DEFINE_CONSTANT(target, S_IRWXU);
#endif


#ifdef S_IRUSR
  NODE_DEFINE_CONSTANT(target, S_IRUSR);
#endif

#ifdef S_IWUSR
  NODE_DEFINE_CONSTANT(target, S_IWUSR);
#endif

#ifdef S_IXUSR
  NODE_DEFINE_CONSTANT(target, S_IXUSR);
#endif


#ifdef S_IRWXG
  NODE_DEFINE_CONSTANT(target, S_IRWXG);
#endif


#ifdef S_IRGRP
  NODE_DEFINE_CONSTANT(target, S_IRGRP);
#endif

#ifdef S_IWGRP
  NODE_DEFINE_CONSTANT(target, S_IWGRP);
#endif

#ifdef S_IXGRP
  NODE_DEFINE_CONSTANT(target, S_IXGRP);
#endif


#ifdef S_IRWXO
  NODE_DEFINE_CONSTANT(target, S_IRWXO);
#endif


#ifdef S_IROTH
  NODE_DEFINE_CONSTANT(target, S_IROTH);
#endif

#ifdef S_IWOTH
  NODE_DEFINE_CONSTANT(target, S_IWOTH);
#endif

#ifdef S_IXOTH
  NODE_DEFINE_CONSTANT(target, S_IXOTH);
#endif

#ifdef E2BIG
  NODE_DEFINE_CONSTANT(target, E2BIG);
#endif

#ifdef EACCES
  NODE_DEFINE_CONSTANT(target, EACCES);
#endif

#ifdef EADDRINUSE
  NODE_DEFINE_CONSTANT(target, EADDRINUSE);
#endif

#ifdef EADDRNOTAVAIL
  NODE_DEFINE_CONSTANT(target, EADDRNOTAVAIL);
#endif

#ifdef EAFNOSUPPORT
  NODE_DEFINE_CONSTANT(target, EAFNOSUPPORT);
#endif

#ifdef EAGAIN
  NODE_DEFINE_CONSTANT(target, EAGAIN);
#endif

#ifdef EALREADY
  NODE_DEFINE_CONSTANT(target, EALREADY);
#endif

#ifdef EBADF
  NODE_DEFINE_CONSTANT(target, EBADF);
#endif

#ifdef EBADMSG
  NODE_DEFINE_CONSTANT(target, EBADMSG);
#endif

#ifdef EBUSY
  NODE_DEFINE_CONSTANT(target, EBUSY);
#endif

#ifdef ECANCELED
  NODE_DEFINE_CONSTANT(target, ECANCELED);
#endif

#ifdef ECHILD
  NODE_DEFINE_CONSTANT(target, ECHILD);
#endif

#ifdef ECONNABORTED
  NODE_DEFINE_CONSTANT(target, ECONNABORTED);
#endif

#ifdef ECONNREFUSED
  NODE_DEFINE_CONSTANT(target, ECONNREFUSED);
#endif

#ifdef ECONNRESET
  NODE_DEFINE_CONSTANT(target, ECONNRESET);
#endif

#ifdef EDEADLK
  NODE_DEFINE_CONSTANT(target, EDEADLK);
#endif

#ifdef EDESTADDRREQ
  NODE_DEFINE_CONSTANT(target, EDESTADDRREQ);
#endif

#ifdef EDOM
  NODE_DEFINE_CONSTANT(target, EDOM);
#endif

#ifdef EDQUOT
  NODE_DEFINE_CONSTANT(target, EDQUOT);
#endif

#ifdef EEXIST
  NODE_DEFINE_CONSTANT(target, EEXIST);
#endif

#ifdef EFAULT
  NODE_DEFINE_CONSTANT(target, EFAULT);
#endif

#ifdef EFBIG
  NODE_DEFINE_CONSTANT(target, EFBIG);
#endif

#ifdef EHOSTUNREACH
  NODE_DEFINE_CONSTANT(target, EHOSTUNREACH);
#endif

#ifdef EIDRM
  NODE_DEFINE_CONSTANT(target, EIDRM);
#endif

#ifdef EILSEQ
  NODE_DEFINE_CONSTANT(target, EILSEQ);
#endif

#ifdef EINPROGRESS
  NODE_DEFINE_CONSTANT(target, EINPROGRESS);
#endif

#ifdef EINTR
  NODE_DEFINE_CONSTANT(target, EINTR);
#endif

#ifdef EINVAL
  NODE_DEFINE_CONSTANT(target, EINVAL);
#endif

#ifdef EIO
  NODE_DEFINE_CONSTANT(target, EIO);
#endif

#ifdef EISCONN
  NODE_DEFINE_CONSTANT(target, EISCONN);
#endif

#ifdef EISDIR
  NODE_DEFINE_CONSTANT(target, EISDIR);
#endif

#ifdef ELOOP
  NODE_DEFINE_CONSTANT(target, ELOOP);
#endif

#ifdef EMFILE
  NODE_DEFINE_CONSTANT(target, EMFILE);
#endif

#ifdef EMLINK
  NODE_DEFINE_CONSTANT(target, EMLINK);
#endif

#ifdef EMSGSIZE
  NODE_DEFINE_CONSTANT(target, EMSGSIZE);
#endif

#ifdef EMULTIHOP
  NODE_DEFINE_CONSTANT(target, EMULTIHOP);
#endif

#ifdef ENAMETOOLONG
  NODE_DEFINE_CONSTANT(target, ENAMETOOLONG);
#endif

#ifdef ENETDOWN
  NODE_DEFINE_CONSTANT(target, ENETDOWN);
#endif

#ifdef ENETRESET
  NODE_DEFINE_CONSTANT(target, ENETRESET);
#endif

#ifdef ENETUNREACH
  NODE_DEFINE_CONSTANT(target, ENETUNREACH);
#endif

#ifdef ENFILE
  NODE_DEFINE_CONSTANT(target, ENFILE);
#endif

#ifdef ENOBUFS
  NODE_DEFINE_CONSTANT(target, ENOBUFS);
#endif

#ifdef ENODATA
  NODE_DEFINE_CONSTANT(target, ENODATA);
#endif

#ifdef ENODEV
  NODE_DEFINE_CONSTANT(target, ENODEV);
#endif

#ifdef ENOENT
  NODE_DEFINE_CONSTANT(target, ENOENT);
#endif

#ifdef ENOEXEC
  NODE_DEFINE_CONSTANT(target, ENOEXEC);
#endif

#ifdef ENOLCK
  NODE_DEFINE_CONSTANT(target, ENOLCK);
#endif

#ifdef ENOLINK
  NODE_DEFINE_CONSTANT(target, ENOLINK);
#endif

#ifdef ENOMEM
  NODE_DEFINE_CONSTANT(target, ENOMEM);
#endif

#ifdef ENOMSG
  NODE_DEFINE_CONSTANT(target, ENOMSG);
#endif

#ifdef ENOPROTOOPT
  NODE_DEFINE_CONSTANT(target, ENOPROTOOPT);
#endif

#ifdef ENOSPC
  NODE_DEFINE_CONSTANT(target, ENOSPC);
#endif

#ifdef ENOSR
  NODE_DEFINE_CONSTANT(target, ENOSR);
#endif

#ifdef ENOSTR
  NODE_DEFINE_CONSTANT(target, ENOSTR);
#endif

#ifdef ENOSYS
  NODE_DEFINE_CONSTANT(target, ENOSYS);
#endif

#ifdef ENOTCONN
  NODE_DEFINE_CONSTANT(target, ENOTCONN);
#endif

#ifdef ENOTDIR
  NODE_DEFINE_CONSTANT(target, ENOTDIR);
#endif

#ifdef ENOTEMPTY
  NODE_DEFINE_CONSTANT(target, ENOTEMPTY);
#endif

#ifdef ENOTSOCK
  NODE_DEFINE_CONSTANT(target, ENOTSOCK);
#endif

#ifdef ENOTSUP
  NODE_DEFINE_CONSTANT(target, ENOTSUP);
#endif

#ifdef ENOTTY
  NODE_DEFINE_CONSTANT(target, ENOTTY);
#endif

#ifdef ENXIO
  NODE_DEFINE_CONSTANT(target, ENXIO);
#endif

#ifdef EOPNOTSUPP
  NODE_DEFINE_CONSTANT(target, EOPNOTSUPP);
#endif

#ifdef EOVERFLOW
  NODE_DEFINE_CONSTANT(target, EOVERFLOW);
#endif

#ifdef EPERM
  NODE_DEFINE_CONSTANT(target, EPERM);
#endif

#ifdef EPIPE
  NODE_DEFINE_CONSTANT(target, EPIPE);
#endif

#ifdef EPROTO
  NODE_DEFINE_CONSTANT(target, EPROTO);
#endif

#ifdef EPROTONOSUPPORT
  NODE_DEFINE_CONSTANT(target, EPROTONOSUPPORT);
#endif

#ifdef EPROTOTYPE
  NODE_DEFINE_CONSTANT(target, EPROTOTYPE);
#endif

#ifdef ERANGE
  NODE_DEFINE_CONSTANT(target, ERANGE);
#endif

#ifdef EROFS
  NODE_DEFINE_CONSTANT(target, EROFS);
#endif

#ifdef ESPIPE
  NODE_DEFINE_CONSTANT(target, ESPIPE);
#endif

#ifdef ESRCH
  NODE_DEFINE_CONSTANT(target, ESRCH);
#endif

#ifdef ESTALE
  NODE_DEFINE_CONSTANT(target, ESTALE);
#endif

#ifdef ETIME
  NODE_DEFINE_CONSTANT(target, ETIME);
#endif

#ifdef ETIMEDOUT
  NODE_DEFINE_CONSTANT(target, ETIMEDOUT);
#endif

#ifdef ETXTBSY
  NODE_DEFINE_CONSTANT(target, ETXTBSY);
#endif

#ifdef EWOULDBLOCK
  NODE_DEFINE_CONSTANT(target, EWOULDBLOCK);
#endif

#ifdef EXDEV
  NODE_DEFINE_CONSTANT(target, EXDEV);
#endif

}

