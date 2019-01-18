/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* Caveat emptor: this file deviates from the libuv convention of returning
 * negated errno codes. Most uv_fs_*() functions map directly to the system
 * call of the same name. For more complex wrappers, it's easier to just
 * return -1 with errno set. The dispatcher in uv__fs_work() takes care of
 * getting the errno to the right place (req->result or as the return value.)
 */

#include "uv.h"
#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> /* PATH_MAX */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#if defined(__DragonFly__)        ||                                      \
    defined(__FreeBSD__)          ||                                      \
    defined(__FreeBSD_kernel_)    ||                                      \
    defined(__OpenBSD__)          ||                                      \
    defined(__NetBSD__)
# define HAVE_PREADV 1
#else
# define HAVE_PREADV 0
#endif

#if defined(__linux__) || defined(__sun)
# include <sys/sendfile.h>
#endif

#if defined(__APPLE__)
# include <copyfile.h>
# include <sys/sysctl.h>
#elif defined(__linux__) && !defined(FICLONE)
# include <sys/ioctl.h>
# define FICLONE _IOW(0x94, 9, int)
#endif

#if defined(_AIX) && !defined(_AIX71)
# include <utime.h>
#endif

#if defined(_AIX) && _XOPEN_SOURCE <= 600
extern char *mkdtemp(char *template); /* See issue #740 on AIX < 7 */
#endif

#define INIT(subtype)                                                         \
  do {                                                                        \
    if (req == NULL)                                                          \
      return UV_EINVAL;                                                       \
    UV_REQ_INIT(req, UV_FS);                                                  \
    req->fs_type = UV_FS_ ## subtype;                                         \
    req->result = 0;                                                          \
    req->ptr = NULL;                                                          \
    req->loop = loop;                                                         \
    req->path = NULL;                                                         \
    req->new_path = NULL;                                                     \
    req->bufs = NULL;                                                         \
    req->cb = cb;                                                             \
  }                                                                           \
  while (0)

#define PATH                                                                  \
  do {                                                                        \
    assert(path != NULL);                                                     \
    if (cb == NULL) {                                                         \
      req->path = path;                                                       \
    } else {                                                                  \
      req->path = uv__strdup(path);                                           \
      if (req->path == NULL)                                                  \
        return UV_ENOMEM;                                                     \
    }                                                                         \
  }                                                                           \
  while (0)

#define PATH2                                                                 \
  do {                                                                        \
    if (cb == NULL) {                                                         \
      req->path = path;                                                       \
      req->new_path = new_path;                                               \
    } else {                                                                  \
      size_t path_len;                                                        \
      size_t new_path_len;                                                    \
      path_len = strlen(path) + 1;                                            \
      new_path_len = strlen(new_path) + 1;                                    \
      req->path = uv__malloc(path_len + new_path_len);                        \
      if (req->path == NULL)                                                  \
        return UV_ENOMEM;                                                     \
      req->new_path = req->path + path_len;                                   \
      memcpy((void*) req->path, path, path_len);                              \
      memcpy((void*) req->new_path, new_path, new_path_len);                  \
    }                                                                         \
  }                                                                           \
  while (0)

#define POST                                                                  \
  do {                                                                        \
    if (cb != NULL) {                                                         \
      uv__req_register(loop, req);                                            \
      uv__work_submit(loop,                                                   \
                      &req->work_req,                                         \
                      UV__WORK_FAST_IO,                                       \
                      uv__fs_work,                                            \
                      uv__fs_done);                                           \
      return 0;                                                               \
    }                                                                         \
    else {                                                                    \
      uv__fs_work(&req->work_req);                                            \
      return req->result;                                                     \
    }                                                                         \
  }                                                                           \
  while (0)


static ssize_t uv__fs_fsync(uv_fs_t* req) {
#if defined(__APPLE__)
  /* Apple's fdatasync and fsync explicitly do NOT flush the drive write cache
   * to the drive platters. This is in contrast to Linux's fdatasync and fsync
   * which do, according to recent man pages. F_FULLFSYNC is Apple's equivalent
   * for flushing buffered data to permanent storage. If F_FULLFSYNC is not
   * supported by the file system we should fall back to fsync(). This is the
   * same approach taken by sqlite.
   */
  int r;

  r = fcntl(req->file, F_FULLFSYNC);
  if (r != 0)
    r = fsync(req->file);
  return r;
#else
  return fsync(req->file);
#endif
}


static ssize_t uv__fs_fdatasync(uv_fs_t* req) {
#if defined(__linux__) || defined(__sun) || defined(__NetBSD__)
  return fdatasync(req->file);
#elif defined(__APPLE__)
  /* See the comment in uv__fs_fsync. */
  return uv__fs_fsync(req);
#else
  return fsync(req->file);
#endif
}


static ssize_t uv__fs_futime(uv_fs_t* req) {
#if defined(__linux__)                                                        \
    || defined(_AIX71)
  /* utimesat() has nanosecond resolution but we stick to microseconds
   * for the sake of consistency with other platforms.
   */
  struct timespec ts[2];
  ts[0].tv_sec  = req->atime;
  ts[0].tv_nsec = (uint64_t)(req->atime * 1000000) % 1000000 * 1000;
  ts[1].tv_sec  = req->mtime;
  ts[1].tv_nsec = (uint64_t)(req->mtime * 1000000) % 1000000 * 1000;
  return futimens(req->file, ts);
#elif defined(__APPLE__)                                                      \
    || defined(__DragonFly__)                                                 \
    || defined(__FreeBSD__)                                                   \
    || defined(__FreeBSD_kernel__)                                            \
    || defined(__NetBSD__)                                                    \
    || defined(__OpenBSD__)                                                   \
    || defined(__sun)
  struct timeval tv[2];
  tv[0].tv_sec  = req->atime;
  tv[0].tv_usec = (uint64_t)(req->atime * 1000000) % 1000000;
  tv[1].tv_sec  = req->mtime;
  tv[1].tv_usec = (uint64_t)(req->mtime * 1000000) % 1000000;
# if defined(__sun)
  return futimesat(req->file, NULL, tv);
# else
  return futimes(req->file, tv);
# endif
#elif defined(__MVS__)
  attrib_t atr;
  memset(&atr, 0, sizeof(atr));
  atr.att_mtimechg = 1;
  atr.att_atimechg = 1;
  atr.att_mtime = req->mtime;
  atr.att_atime = req->atime;
  return __fchattr(req->file, &atr, sizeof(atr));
#else
  errno = ENOSYS;
  return -1;
#endif
}


static ssize_t uv__fs_mkdtemp(uv_fs_t* req) {
  return mkdtemp((char*) req->path) ? 0 : -1;
}


static ssize_t uv__fs_open(uv_fs_t* req) {
  static int no_cloexec_support;
  int r;

  /* Try O_CLOEXEC before entering locks */
  if (no_cloexec_support == 0) {
#ifdef O_CLOEXEC
    r = open(req->path, req->flags | O_CLOEXEC, req->mode);
    if (r >= 0)
      return r;
    if (errno != EINVAL)
      return r;
    no_cloexec_support = 1;
#endif  /* O_CLOEXEC */
  }

  if (req->cb != NULL)
    uv_rwlock_rdlock(&req->loop->cloexec_lock);

  r = open(req->path, req->flags, req->mode);

  /* In case of failure `uv__cloexec` will leave error in `errno`,
   * so it is enough to just set `r` to `-1`.
   */
  if (r >= 0 && uv__cloexec(r, 1) != 0) {
    r = uv__close(r);
    if (r != 0)
      abort();
    r = -1;
  }

  if (req->cb != NULL)
    uv_rwlock_rdunlock(&req->loop->cloexec_lock);

  return r;
}


static ssize_t uv__fs_read(uv_fs_t* req) {
#if defined(__linux__)
  static int no_preadv;
#endif
  unsigned int iovmax;
  ssize_t result;

  iovmax = uv__getiovmax();
  if (req->nbufs > iovmax)
    req->nbufs = iovmax;

  if (req->off < 0) {
    if (req->nbufs == 1)
      result = read(req->file, req->bufs[0].base, req->bufs[0].len);
    else
      result = readv(req->file, (struct iovec*) req->bufs, req->nbufs);
  } else {
    if (req->nbufs == 1) {
      result = pread(req->file, req->bufs[0].base, req->bufs[0].len, req->off);
      goto done;
    }

#if HAVE_PREADV
    result = preadv(req->file, (struct iovec*) req->bufs, req->nbufs, req->off);
#else
# if defined(__linux__)
    if (no_preadv) retry:
# endif
    {
      result = pread(req->file, req->bufs[0].base, req->bufs[0].len, req->off);
    }
# if defined(__linux__)
    else {
      result = uv__preadv(req->file,
                          (struct iovec*)req->bufs,
                          req->nbufs,
                          req->off);
      if (result == -1 && errno == ENOSYS) {
        no_preadv = 1;
        goto retry;
      }
    }
# endif
#endif
  }

done:
  /* Early cleanup of bufs allocation, since we're done with it. */
  if (req->bufs != req->bufsml)
    uv__free(req->bufs);

  req->bufs = NULL;
  req->nbufs = 0;

#ifdef __PASE__
  /* PASE returns EOPNOTSUPP when reading a directory, convert to EISDIR */
  if (result == -1 && errno == EOPNOTSUPP) {
    struct stat buf;
    ssize_t rc;
    rc = fstat(req->file, &buf);
    if (rc == 0 && S_ISDIR(buf.st_mode)) {
      errno = EISDIR;
    }
  }
#endif

  return result;
}


#if defined(__APPLE__) && !defined(MAC_OS_X_VERSION_10_8)
#define UV_CONST_DIRENT uv__dirent_t
#else
#define UV_CONST_DIRENT const uv__dirent_t
#endif


static int uv__fs_scandir_filter(UV_CONST_DIRENT* dent) {
  return strcmp(dent->d_name, ".") != 0 && strcmp(dent->d_name, "..") != 0;
}


static int uv__fs_scandir_sort(UV_CONST_DIRENT** a, UV_CONST_DIRENT** b) {
  return strcmp((*a)->d_name, (*b)->d_name);
}


static ssize_t uv__fs_scandir(uv_fs_t* req) {
  uv__dirent_t **dents;
  int n;

  dents = NULL;
  n = scandir(req->path, &dents, uv__fs_scandir_filter, uv__fs_scandir_sort);

  /* NOTE: We will use nbufs as an index field */
  req->nbufs = 0;

  if (n == 0) {
    /* OS X still needs to deallocate some memory.
     * Memory was allocated using the system allocator, so use free() here.
     */
    free(dents);
    dents = NULL;
  } else if (n == -1) {
    return n;
  }

  req->ptr = dents;

  return n;
}

#if defined(_POSIX_PATH_MAX)
# define UV__FS_PATH_MAX _POSIX_PATH_MAX
#elif defined(PATH_MAX)
# define UV__FS_PATH_MAX PATH_MAX
#else
# define UV__FS_PATH_MAX_FALLBACK 8192
# define UV__FS_PATH_MAX UV__FS_PATH_MAX_FALLBACK
#endif

static ssize_t uv__fs_pathmax_size(const char* path) {
  ssize_t pathmax;

  pathmax = pathconf(path, _PC_PATH_MAX);

  if (pathmax == -1)
    pathmax = UV__FS_PATH_MAX;

  return pathmax;
}

static ssize_t uv__fs_readlink(uv_fs_t* req) {
  ssize_t maxlen;
  ssize_t len;
  char* buf;
  char* newbuf;

#if defined(UV__FS_PATH_MAX_FALLBACK)
  /* We may not have a real PATH_MAX.  Read size of link.  */
  struct stat st;
  int ret;
  ret = lstat(req->path, &st);
  if (ret != 0)
    return -1;
  if (!S_ISLNK(st.st_mode)) {
    errno = EINVAL;
    return -1;
  }

  maxlen = st.st_size;

  /* According to readlink(2) lstat can report st_size == 0
     for some symlinks, such as those in /proc or /sys.  */
  if (maxlen == 0)
    maxlen = uv__fs_pathmax_size(req->path);
#else
  maxlen = uv__fs_pathmax_size(req->path);
#endif

  buf = uv__malloc(maxlen);

  if (buf == NULL) {
    errno = ENOMEM;
    return -1;
  }

#if defined(__MVS__)
  len = os390_readlink(req->path, buf, maxlen);
#else
  len = readlink(req->path, buf, maxlen);
#endif

  if (len == -1) {
    uv__free(buf);
    return -1;
  }

  /* Uncommon case: resize to make room for the trailing nul byte. */
  if (len == maxlen) {
    newbuf = uv__realloc(buf, len + 1);

    if (newbuf == NULL) {
      uv__free(buf);
      return -1;
    }

    buf = newbuf;
  }

  buf[len] = '\0';
  req->ptr = buf;

  return 0;
}

static ssize_t uv__fs_realpath(uv_fs_t* req) {
  char* buf;

#if defined(_POSIX_VERSION) && _POSIX_VERSION >= 200809L
  buf = realpath(req->path, NULL);
  if (buf == NULL)
    return -1;
#else
  ssize_t len;

  len = uv__fs_pathmax_size(req->path);
  buf = uv__malloc(len + 1);

  if (buf == NULL) {
    errno = ENOMEM;
    return -1;
  }

  if (realpath(req->path, buf) == NULL) {
    uv__free(buf);
    return -1;
  }
#endif

  req->ptr = buf;

  return 0;
}

static ssize_t uv__fs_sendfile_emul(uv_fs_t* req) {
  struct pollfd pfd;
  int use_pread;
  off_t offset;
  ssize_t nsent;
  ssize_t nread;
  ssize_t nwritten;
  size_t buflen;
  size_t len;
  ssize_t n;
  int in_fd;
  int out_fd;
  char buf[8192];

  len = req->bufsml[0].len;
  in_fd = req->flags;
  out_fd = req->file;
  offset = req->off;
  use_pread = 1;

  /* Here are the rules regarding errors:
   *
   * 1. Read errors are reported only if nsent==0, otherwise we return nsent.
   *    The user needs to know that some data has already been sent, to stop
   *    them from sending it twice.
   *
   * 2. Write errors are always reported. Write errors are bad because they
   *    mean data loss: we've read data but now we can't write it out.
   *
   * We try to use pread() and fall back to regular read() if the source fd
   * doesn't support positional reads, for example when it's a pipe fd.
   *
   * If we get EAGAIN when writing to the target fd, we poll() on it until
   * it becomes writable again.
   *
   * FIXME: If we get a write error when use_pread==1, it should be safe to
   *        return the number of sent bytes instead of an error because pread()
   *        is, in theory, idempotent. However, special files in /dev or /proc
   *        may support pread() but not necessarily return the same data on
   *        successive reads.
   *
   * FIXME: There is no way now to signal that we managed to send *some* data
   *        before a write error.
   */
  for (nsent = 0; (size_t) nsent < len; ) {
    buflen = len - nsent;

    if (buflen > sizeof(buf))
      buflen = sizeof(buf);

    do
      if (use_pread)
        nread = pread(in_fd, buf, buflen, offset);
      else
        nread = read(in_fd, buf, buflen);
    while (nread == -1 && errno == EINTR);

    if (nread == 0)
      goto out;

    if (nread == -1) {
      if (use_pread && nsent == 0 && (errno == EIO || errno == ESPIPE)) {
        use_pread = 0;
        continue;
      }

      if (nsent == 0)
        nsent = -1;

      goto out;
    }

    for (nwritten = 0; nwritten < nread; ) {
      do
        n = write(out_fd, buf + nwritten, nread - nwritten);
      while (n == -1 && errno == EINTR);

      if (n != -1) {
        nwritten += n;
        continue;
      }

      if (errno != EAGAIN && errno != EWOULDBLOCK) {
        nsent = -1;
        goto out;
      }

      pfd.fd = out_fd;
      pfd.events = POLLOUT;
      pfd.revents = 0;

      do
        n = poll(&pfd, 1, -1);
      while (n == -1 && errno == EINTR);

      if (n == -1 || (pfd.revents & ~POLLOUT) != 0) {
        errno = EIO;
        nsent = -1;
        goto out;
      }
    }

    offset += nread;
    nsent += nread;
  }

out:
  if (nsent != -1)
    req->off = offset;

  return nsent;
}


static ssize_t uv__fs_sendfile(uv_fs_t* req) {
  int in_fd;
  int out_fd;

  in_fd = req->flags;
  out_fd = req->file;

#if defined(__linux__) || defined(__sun)
  {
    off_t off;
    ssize_t r;

    off = req->off;
    r = sendfile(out_fd, in_fd, &off, req->bufsml[0].len);

    /* sendfile() on SunOS returns EINVAL if the target fd is not a socket but
     * it still writes out data. Fortunately, we can detect it by checking if
     * the offset has been updated.
     */
    if (r != -1 || off > req->off) {
      r = off - req->off;
      req->off = off;
      return r;
    }

    if (errno == EINVAL ||
        errno == EIO ||
        errno == ENOTSOCK ||
        errno == EXDEV) {
      errno = 0;
      return uv__fs_sendfile_emul(req);
    }

    return -1;
  }
#elif defined(__APPLE__)           || \
      defined(__DragonFly__)       || \
      defined(__FreeBSD__)         || \
      defined(__FreeBSD_kernel__)
  {
    off_t len;
    ssize_t r;

    /* sendfile() on FreeBSD and Darwin returns EAGAIN if the target fd is in
     * non-blocking mode and not all data could be written. If a non-zero
     * number of bytes have been sent, we don't consider it an error.
     */

#if defined(__FreeBSD__) || defined(__DragonFly__)
    len = 0;
    r = sendfile(in_fd, out_fd, req->off, req->bufsml[0].len, NULL, &len, 0);
#elif defined(__FreeBSD_kernel__)
    len = 0;
    r = bsd_sendfile(in_fd,
                     out_fd,
                     req->off,
                     req->bufsml[0].len,
                     NULL,
                     &len,
                     0);
#else
    /* The darwin sendfile takes len as an input for the length to send,
     * so make sure to initialize it with the caller's value. */
    len = req->bufsml[0].len;
    r = sendfile(in_fd, out_fd, req->off, &len, NULL, 0);
#endif

     /*
     * The man page for sendfile(2) on DragonFly states that `len` contains
     * a meaningful value ONLY in case of EAGAIN and EINTR.
     * Nothing is said about it's value in case of other errors, so better
     * not depend on the potential wrong assumption that is was not modified
     * by the syscall.
     */
    if (r == 0 || ((errno == EAGAIN || errno == EINTR) && len != 0)) {
      req->off += len;
      return (ssize_t) len;
    }

    if (errno == EINVAL ||
        errno == EIO ||
        errno == ENOTSOCK ||
        errno == EXDEV) {
      errno = 0;
      return uv__fs_sendfile_emul(req);
    }

    return -1;
  }
#else
  /* Squelch compiler warnings. */
  (void) &in_fd;
  (void) &out_fd;

  return uv__fs_sendfile_emul(req);
#endif
}


static ssize_t uv__fs_utime(uv_fs_t* req) {
#if defined(__linux__)                                                         \
    || defined(_AIX71)                                                         \
    || defined(__sun)
  /* utimesat() has nanosecond resolution but we stick to microseconds
   * for the sake of consistency with other platforms.
   */
  struct timespec ts[2];
  ts[0].tv_sec  = req->atime;
  ts[0].tv_nsec = (uint64_t)(req->atime * 1000000) % 1000000 * 1000;
  ts[1].tv_sec  = req->mtime;
  ts[1].tv_nsec = (uint64_t)(req->mtime * 1000000) % 1000000 * 1000;
  return utimensat(AT_FDCWD, req->path, ts, 0);
#elif defined(__APPLE__)                                                      \
    || defined(__DragonFly__)                                                 \
    || defined(__FreeBSD__)                                                   \
    || defined(__FreeBSD_kernel__)                                            \
    || defined(__NetBSD__)                                                    \
    || defined(__OpenBSD__)
  struct timeval tv[2];
  tv[0].tv_sec  = req->atime;
  tv[0].tv_usec = (uint64_t)(req->atime * 1000000) % 1000000;
  tv[1].tv_sec  = req->mtime;
  tv[1].tv_usec = (uint64_t)(req->mtime * 1000000) % 1000000;
  return utimes(req->path, tv);
#elif defined(_AIX)                                                           \
    && !defined(_AIX71)
  struct utimbuf buf;
  buf.actime = req->atime;
  buf.modtime = req->mtime;
  return utime(req->path, &buf);
#elif defined(__MVS__)
  attrib_t atr;
  memset(&atr, 0, sizeof(atr));
  atr.att_mtimechg = 1;
  atr.att_atimechg = 1;
  atr.att_mtime = req->mtime;
  atr.att_atime = req->atime;
  return __lchattr((char*) req->path, &atr, sizeof(atr));
#else
  errno = ENOSYS;
  return -1;
#endif
}


static ssize_t uv__fs_write(uv_fs_t* req) {
#if defined(__linux__)
  static int no_pwritev;
#endif
  ssize_t r;

  /* Serialize writes on OS X, concurrent write() and pwrite() calls result in
   * data loss. We can't use a per-file descriptor lock, the descriptor may be
   * a dup().
   */
#if defined(__APPLE__)
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

  if (pthread_mutex_lock(&lock))
    abort();
#endif

  if (req->off < 0) {
    if (req->nbufs == 1)
      r = write(req->file, req->bufs[0].base, req->bufs[0].len);
    else
      r = writev(req->file, (struct iovec*) req->bufs, req->nbufs);
  } else {
    if (req->nbufs == 1) {
      r = pwrite(req->file, req->bufs[0].base, req->bufs[0].len, req->off);
      goto done;
    }
#if HAVE_PREADV
    r = pwritev(req->file, (struct iovec*) req->bufs, req->nbufs, req->off);
#else
# if defined(__linux__)
    if (no_pwritev) retry:
# endif
    {
      r = pwrite(req->file, req->bufs[0].base, req->bufs[0].len, req->off);
    }
# if defined(__linux__)
    else {
      r = uv__pwritev(req->file,
                      (struct iovec*) req->bufs,
                      req->nbufs,
                      req->off);
      if (r == -1 && errno == ENOSYS) {
        no_pwritev = 1;
        goto retry;
      }
    }
# endif
#endif
  }

done:
#if defined(__APPLE__)
  if (pthread_mutex_unlock(&lock))
    abort();
#endif

  return r;
}

static ssize_t uv__fs_copyfile(uv_fs_t* req) {
#if defined(__APPLE__) && !TARGET_OS_IPHONE
  /* On macOS, use the native copyfile(3). */
  static int can_clone;
  copyfile_flags_t flags;
  char buf[64];
  size_t len;
  int major;

  flags = COPYFILE_ALL;

  if (req->flags & UV_FS_COPYFILE_EXCL)
    flags |= COPYFILE_EXCL;

  /* Check OS version. Cloning is only supported on macOS >= 10.12. */
  if (req->flags & UV_FS_COPYFILE_FICLONE_FORCE) {
    if (can_clone == 0) {
      len = sizeof(buf);
      if (sysctlbyname("kern.osrelease", buf, &len, NULL, 0))
        return UV__ERR(errno);

      if (1 != sscanf(buf, "%d", &major))
        abort();

      can_clone = -1 + 2 * (major >= 16);  /* macOS >= 10.12 */
    }

    if (can_clone < 0)
      return UV_ENOSYS;
  }

  /* copyfile() simply ignores COPYFILE_CLONE if it's not supported. */
  if (req->flags & UV_FS_COPYFILE_FICLONE)
    flags |= 1 << 24;  /* COPYFILE_CLONE */

  if (req->flags & UV_FS_COPYFILE_FICLONE_FORCE)
    flags |= 1 << 25;  /* COPYFILE_CLONE_FORCE */

  return copyfile(req->path, req->new_path, NULL, flags);
#else
  uv_fs_t fs_req;
  uv_file srcfd;
  uv_file dstfd;
  struct stat statsbuf;
  int dst_flags;
  int result;
  int err;
  size_t bytes_to_send;
  int64_t in_offset;

  dstfd = -1;
  err = 0;

  /* Open the source file. */
  srcfd = uv_fs_open(NULL, &fs_req, req->path, O_RDONLY, 0, NULL);
  uv_fs_req_cleanup(&fs_req);

  if (srcfd < 0)
    return srcfd;

  /* Get the source file's mode. */
  if (fstat(srcfd, &statsbuf)) {
    err = UV__ERR(errno);
    goto out;
  }

  dst_flags = O_WRONLY | O_CREAT | O_TRUNC;

  if (req->flags & UV_FS_COPYFILE_EXCL)
    dst_flags |= O_EXCL;

  /* Open the destination file. */
  dstfd = uv_fs_open(NULL,
                     &fs_req,
                     req->new_path,
                     dst_flags,
                     statsbuf.st_mode,
                     NULL);
  uv_fs_req_cleanup(&fs_req);

  if (dstfd < 0) {
    err = dstfd;
    goto out;
  }

  if (fchmod(dstfd, statsbuf.st_mode) == -1) {
    err = UV__ERR(errno);
    goto out;
  }

#ifdef FICLONE
  if (req->flags & UV_FS_COPYFILE_FICLONE ||
      req->flags & UV_FS_COPYFILE_FICLONE_FORCE) {
    if (ioctl(dstfd, FICLONE, srcfd) == -1) {
      /* If an error occurred that the sendfile fallback also won't handle, or
         this is a force clone then exit. Otherwise, fall through to try using
         sendfile(). */
      if (errno != ENOTTY && errno != EOPNOTSUPP && errno != EXDEV) {
        err = UV__ERR(errno);
        goto out;
      } else if (req->flags & UV_FS_COPYFILE_FICLONE_FORCE) {
        err = UV_ENOTSUP;
        goto out;
      }
    } else {
      goto out;
    }
  }
#else
  if (req->flags & UV_FS_COPYFILE_FICLONE_FORCE) {
    err = UV_ENOSYS;
    goto out;
  }
#endif

  bytes_to_send = statsbuf.st_size;
  in_offset = 0;
  while (bytes_to_send != 0) {
    err = uv_fs_sendfile(NULL,
                         &fs_req,
                         dstfd,
                         srcfd,
                         in_offset,
                         bytes_to_send,
                         NULL);
    uv_fs_req_cleanup(&fs_req);
    if (err < 0)
      break;
    bytes_to_send -= fs_req.result;
    in_offset += fs_req.result;
  }

out:
  if (err < 0)
    result = err;
  else
    result = 0;

  /* Close the source file. */
  err = uv__close_nocheckstdio(srcfd);

  /* Don't overwrite any existing errors. */
  if (err != 0 && result == 0)
    result = err;

  /* Close the destination file if it is open. */
  if (dstfd >= 0) {
    err = uv__close_nocheckstdio(dstfd);

    /* Don't overwrite any existing errors. */
    if (err != 0 && result == 0)
      result = err;

    /* Remove the destination file if something went wrong. */
    if (result != 0) {
      uv_fs_unlink(NULL, &fs_req, req->new_path, NULL);
      /* Ignore the unlink return value, as an error already happened. */
      uv_fs_req_cleanup(&fs_req);
    }
  }

  if (result == 0)
    return 0;

  errno = UV__ERR(result);
  return -1;
#endif
}

static void uv__to_stat(struct stat* src, uv_stat_t* dst) {
  dst->st_dev = src->st_dev;
  dst->st_mode = src->st_mode;
  dst->st_nlink = src->st_nlink;
  dst->st_uid = src->st_uid;
  dst->st_gid = src->st_gid;
  dst->st_rdev = src->st_rdev;
  dst->st_ino = src->st_ino;
  dst->st_size = src->st_size;
  dst->st_blksize = src->st_blksize;
  dst->st_blocks = src->st_blocks;

#if defined(__APPLE__)
  dst->st_atim.tv_sec = src->st_atimespec.tv_sec;
  dst->st_atim.tv_nsec = src->st_atimespec.tv_nsec;
  dst->st_mtim.tv_sec = src->st_mtimespec.tv_sec;
  dst->st_mtim.tv_nsec = src->st_mtimespec.tv_nsec;
  dst->st_ctim.tv_sec = src->st_ctimespec.tv_sec;
  dst->st_ctim.tv_nsec = src->st_ctimespec.tv_nsec;
  dst->st_birthtim.tv_sec = src->st_birthtimespec.tv_sec;
  dst->st_birthtim.tv_nsec = src->st_birthtimespec.tv_nsec;
  dst->st_flags = src->st_flags;
  dst->st_gen = src->st_gen;
#elif defined(__ANDROID__)
  dst->st_atim.tv_sec = src->st_atime;
  dst->st_atim.tv_nsec = src->st_atimensec;
  dst->st_mtim.tv_sec = src->st_mtime;
  dst->st_mtim.tv_nsec = src->st_mtimensec;
  dst->st_ctim.tv_sec = src->st_ctime;
  dst->st_ctim.tv_nsec = src->st_ctimensec;
  dst->st_birthtim.tv_sec = src->st_ctime;
  dst->st_birthtim.tv_nsec = src->st_ctimensec;
  dst->st_flags = 0;
  dst->st_gen = 0;
#elif !defined(_AIX) && (       \
    defined(__DragonFly__)   || \
    defined(__FreeBSD__)     || \
    defined(__OpenBSD__)     || \
    defined(__NetBSD__)      || \
    defined(_GNU_SOURCE)     || \
    defined(_BSD_SOURCE)     || \
    defined(_SVID_SOURCE)    || \
    defined(_XOPEN_SOURCE)   || \
    defined(_DEFAULT_SOURCE))
  dst->st_atim.tv_sec = src->st_atim.tv_sec;
  dst->st_atim.tv_nsec = src->st_atim.tv_nsec;
  dst->st_mtim.tv_sec = src->st_mtim.tv_sec;
  dst->st_mtim.tv_nsec = src->st_mtim.tv_nsec;
  dst->st_ctim.tv_sec = src->st_ctim.tv_sec;
  dst->st_ctim.tv_nsec = src->st_ctim.tv_nsec;
# if defined(__FreeBSD__)    || \
     defined(__NetBSD__)
  dst->st_birthtim.tv_sec = src->st_birthtim.tv_sec;
  dst->st_birthtim.tv_nsec = src->st_birthtim.tv_nsec;
  dst->st_flags = src->st_flags;
  dst->st_gen = src->st_gen;
# else
  dst->st_birthtim.tv_sec = src->st_ctim.tv_sec;
  dst->st_birthtim.tv_nsec = src->st_ctim.tv_nsec;
  dst->st_flags = 0;
  dst->st_gen = 0;
# endif
#else
  dst->st_atim.tv_sec = src->st_atime;
  dst->st_atim.tv_nsec = 0;
  dst->st_mtim.tv_sec = src->st_mtime;
  dst->st_mtim.tv_nsec = 0;
  dst->st_ctim.tv_sec = src->st_ctime;
  dst->st_ctim.tv_nsec = 0;
  dst->st_birthtim.tv_sec = src->st_ctime;
  dst->st_birthtim.tv_nsec = 0;
  dst->st_flags = 0;
  dst->st_gen = 0;
#endif
}


static int uv__fs_stat(const char *path, uv_stat_t *buf) {
  struct stat pbuf;
  int ret;

  ret = stat(path, &pbuf);
  if (ret == 0)
    uv__to_stat(&pbuf, buf);

  return ret;
}


static int uv__fs_lstat(const char *path, uv_stat_t *buf) {
  struct stat pbuf;
  int ret;

  ret = lstat(path, &pbuf);
  if (ret == 0)
    uv__to_stat(&pbuf, buf);

  return ret;
}


static int uv__fs_fstat(int fd, uv_stat_t *buf) {
  struct stat pbuf;
  int ret;

  ret = fstat(fd, &pbuf);
  if (ret == 0)
    uv__to_stat(&pbuf, buf);

  return ret;
}

static size_t uv__fs_buf_offset(uv_buf_t* bufs, size_t size) {
  size_t offset;
  /* Figure out which bufs are done */
  for (offset = 0; size > 0 && bufs[offset].len <= size; ++offset)
    size -= bufs[offset].len;

  /* Fix a partial read/write */
  if (size > 0) {
    bufs[offset].base += size;
    bufs[offset].len -= size;
  }
  return offset;
}

static ssize_t uv__fs_write_all(uv_fs_t* req) {
  unsigned int iovmax;
  unsigned int nbufs;
  uv_buf_t* bufs;
  ssize_t total;
  ssize_t result;

  iovmax = uv__getiovmax();
  nbufs = req->nbufs;
  bufs = req->bufs;
  total = 0;

  while (nbufs > 0) {
    req->nbufs = nbufs;
    if (req->nbufs > iovmax)
      req->nbufs = iovmax;

    do
      result = uv__fs_write(req);
    while (result < 0 && errno == EINTR);

    if (result <= 0) {
      if (total == 0)
        total = result;
      break;
    }

    if (req->off >= 0)
      req->off += result;

    req->nbufs = uv__fs_buf_offset(req->bufs, result);
    req->bufs += req->nbufs;
    nbufs -= req->nbufs;
    total += result;
  }

  if (bufs != req->bufsml)
    uv__free(bufs);

  req->bufs = NULL;
  req->nbufs = 0;

  return total;
}


static void uv__fs_work(struct uv__work* w) {
  int retry_on_eintr;
  uv_fs_t* req;
  ssize_t r;

  req = container_of(w, uv_fs_t, work_req);
  retry_on_eintr = !(req->fs_type == UV_FS_CLOSE ||
                     req->fs_type == UV_FS_READ);

  do {
    errno = 0;

#define X(type, action)                                                       \
  case UV_FS_ ## type:                                                        \
    r = action;                                                               \
    break;

    switch (req->fs_type) {
    X(ACCESS, access(req->path, req->flags));
    X(CHMOD, chmod(req->path, req->mode));
    X(CHOWN, chown(req->path, req->uid, req->gid));
    X(CLOSE, close(req->file));
    X(COPYFILE, uv__fs_copyfile(req));
    X(FCHMOD, fchmod(req->file, req->mode));
    X(FCHOWN, fchown(req->file, req->uid, req->gid));
    X(LCHOWN, lchown(req->path, req->uid, req->gid));
    X(FDATASYNC, uv__fs_fdatasync(req));
    X(FSTAT, uv__fs_fstat(req->file, &req->statbuf));
    X(FSYNC, uv__fs_fsync(req));
    X(FTRUNCATE, ftruncate(req->file, req->off));
    X(FUTIME, uv__fs_futime(req));
    X(LSTAT, uv__fs_lstat(req->path, &req->statbuf));
    X(LINK, link(req->path, req->new_path));
    X(MKDIR, mkdir(req->path, req->mode));
    X(MKDTEMP, uv__fs_mkdtemp(req));
    X(OPEN, uv__fs_open(req));
    X(READ, uv__fs_read(req));
    X(SCANDIR, uv__fs_scandir(req));
    X(READLINK, uv__fs_readlink(req));
    X(REALPATH, uv__fs_realpath(req));
    X(RENAME, rename(req->path, req->new_path));
    X(RMDIR, rmdir(req->path));
    X(SENDFILE, uv__fs_sendfile(req));
    X(STAT, uv__fs_stat(req->path, &req->statbuf));
    X(SYMLINK, symlink(req->path, req->new_path));
    X(UNLINK, unlink(req->path));
    X(UTIME, uv__fs_utime(req));
    X(WRITE, uv__fs_write_all(req));
    default: abort();
    }
#undef X
  } while (r == -1 && errno == EINTR && retry_on_eintr);

  if (r == -1)
    req->result = UV__ERR(errno);
  else
    req->result = r;

  if (r == 0 && (req->fs_type == UV_FS_STAT ||
                 req->fs_type == UV_FS_FSTAT ||
                 req->fs_type == UV_FS_LSTAT)) {
    req->ptr = &req->statbuf;
  }
}


static void uv__fs_done(struct uv__work* w, int status) {
  uv_fs_t* req;

  req = container_of(w, uv_fs_t, work_req);
  uv__req_unregister(req->loop, req);

  if (status == UV_ECANCELED) {
    assert(req->result == 0);
    req->result = UV_ECANCELED;
  }

  req->cb(req);
}


int uv_fs_access(uv_loop_t* loop,
                 uv_fs_t* req,
                 const char* path,
                 int flags,
                 uv_fs_cb cb) {
  INIT(ACCESS);
  PATH;
  req->flags = flags;
  POST;
}


int uv_fs_chmod(uv_loop_t* loop,
                uv_fs_t* req,
                const char* path,
                int mode,
                uv_fs_cb cb) {
  INIT(CHMOD);
  PATH;
  req->mode = mode;
  POST;
}


int uv_fs_chown(uv_loop_t* loop,
                uv_fs_t* req,
                const char* path,
                uv_uid_t uid,
                uv_gid_t gid,
                uv_fs_cb cb) {
  INIT(CHOWN);
  PATH;
  req->uid = uid;
  req->gid = gid;
  POST;
}


int uv_fs_close(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  INIT(CLOSE);
  req->file = file;
  POST;
}


int uv_fs_fchmod(uv_loop_t* loop,
                 uv_fs_t* req,
                 uv_file file,
                 int mode,
                 uv_fs_cb cb) {
  INIT(FCHMOD);
  req->file = file;
  req->mode = mode;
  POST;
}


int uv_fs_fchown(uv_loop_t* loop,
                 uv_fs_t* req,
                 uv_file file,
                 uv_uid_t uid,
                 uv_gid_t gid,
                 uv_fs_cb cb) {
  INIT(FCHOWN);
  req->file = file;
  req->uid = uid;
  req->gid = gid;
  POST;
}


int uv_fs_lchown(uv_loop_t* loop,
                 uv_fs_t* req,
                 const char* path,
                 uv_uid_t uid,
                 uv_gid_t gid,
                 uv_fs_cb cb) {
  INIT(LCHOWN);
  PATH;
  req->uid = uid;
  req->gid = gid;
  POST;
}


int uv_fs_fdatasync(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  INIT(FDATASYNC);
  req->file = file;
  POST;
}


int uv_fs_fstat(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  INIT(FSTAT);
  req->file = file;
  POST;
}


int uv_fs_fsync(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  INIT(FSYNC);
  req->file = file;
  POST;
}


int uv_fs_ftruncate(uv_loop_t* loop,
                    uv_fs_t* req,
                    uv_file file,
                    int64_t off,
                    uv_fs_cb cb) {
  INIT(FTRUNCATE);
  req->file = file;
  req->off = off;
  POST;
}


int uv_fs_futime(uv_loop_t* loop,
                 uv_fs_t* req,
                 uv_file file,
                 double atime,
                 double mtime,
                 uv_fs_cb cb) {
  INIT(FUTIME);
  req->file = file;
  req->atime = atime;
  req->mtime = mtime;
  POST;
}


int uv_fs_lstat(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
  INIT(LSTAT);
  PATH;
  POST;
}


int uv_fs_link(uv_loop_t* loop,
               uv_fs_t* req,
               const char* path,
               const char* new_path,
               uv_fs_cb cb) {
  INIT(LINK);
  PATH2;
  POST;
}


int uv_fs_mkdir(uv_loop_t* loop,
                uv_fs_t* req,
                const char* path,
                int mode,
                uv_fs_cb cb) {
  INIT(MKDIR);
  PATH;
  req->mode = mode;
  POST;
}


int uv_fs_mkdtemp(uv_loop_t* loop,
                  uv_fs_t* req,
                  const char* tpl,
                  uv_fs_cb cb) {
  INIT(MKDTEMP);
  req->path = uv__strdup(tpl);
  if (req->path == NULL)
    return UV_ENOMEM;
  POST;
}


int uv_fs_open(uv_loop_t* loop,
               uv_fs_t* req,
               const char* path,
               int flags,
               int mode,
               uv_fs_cb cb) {
  INIT(OPEN);
  PATH;
  req->flags = flags;
  req->mode = mode;
  POST;
}


int uv_fs_read(uv_loop_t* loop, uv_fs_t* req,
               uv_file file,
               const uv_buf_t bufs[],
               unsigned int nbufs,
               int64_t off,
               uv_fs_cb cb) {
  INIT(READ);

  if (bufs == NULL || nbufs == 0)
    return UV_EINVAL;

  req->file = file;

  req->nbufs = nbufs;
  req->bufs = req->bufsml;
  if (nbufs > ARRAY_SIZE(req->bufsml))
    req->bufs = uv__malloc(nbufs * sizeof(*bufs));

  if (req->bufs == NULL)
    return UV_ENOMEM;

  memcpy(req->bufs, bufs, nbufs * sizeof(*bufs));

  req->off = off;
  POST;
}


int uv_fs_scandir(uv_loop_t* loop,
                  uv_fs_t* req,
                  const char* path,
                  int flags,
                  uv_fs_cb cb) {
  INIT(SCANDIR);
  PATH;
  req->flags = flags;
  POST;
}


int uv_fs_readlink(uv_loop_t* loop,
                   uv_fs_t* req,
                   const char* path,
                   uv_fs_cb cb) {
  INIT(READLINK);
  PATH;
  POST;
}


int uv_fs_realpath(uv_loop_t* loop,
                  uv_fs_t* req,
                  const char * path,
                  uv_fs_cb cb) {
  INIT(REALPATH);
  PATH;
  POST;
}


int uv_fs_rename(uv_loop_t* loop,
                 uv_fs_t* req,
                 const char* path,
                 const char* new_path,
                 uv_fs_cb cb) {
  INIT(RENAME);
  PATH2;
  POST;
}


int uv_fs_rmdir(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
  INIT(RMDIR);
  PATH;
  POST;
}


int uv_fs_sendfile(uv_loop_t* loop,
                   uv_fs_t* req,
                   uv_file out_fd,
                   uv_file in_fd,
                   int64_t off,
                   size_t len,
                   uv_fs_cb cb) {
  INIT(SENDFILE);
  req->flags = in_fd; /* hack */
  req->file = out_fd;
  req->off = off;
  req->bufsml[0].len = len;
  POST;
}


int uv_fs_stat(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
  INIT(STAT);
  PATH;
  POST;
}


int uv_fs_symlink(uv_loop_t* loop,
                  uv_fs_t* req,
                  const char* path,
                  const char* new_path,
                  int flags,
                  uv_fs_cb cb) {
  INIT(SYMLINK);
  PATH2;
  req->flags = flags;
  POST;
}


int uv_fs_unlink(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
  INIT(UNLINK);
  PATH;
  POST;
}


int uv_fs_utime(uv_loop_t* loop,
                uv_fs_t* req,
                const char* path,
                double atime,
                double mtime,
                uv_fs_cb cb) {
  INIT(UTIME);
  PATH;
  req->atime = atime;
  req->mtime = mtime;
  POST;
}


int uv_fs_write(uv_loop_t* loop,
                uv_fs_t* req,
                uv_file file,
                const uv_buf_t bufs[],
                unsigned int nbufs,
                int64_t off,
                uv_fs_cb cb) {
  INIT(WRITE);

  if (bufs == NULL || nbufs == 0)
    return UV_EINVAL;

  req->file = file;

  req->nbufs = nbufs;
  req->bufs = req->bufsml;
  if (nbufs > ARRAY_SIZE(req->bufsml))
    req->bufs = uv__malloc(nbufs * sizeof(*bufs));

  if (req->bufs == NULL)
    return UV_ENOMEM;

  memcpy(req->bufs, bufs, nbufs * sizeof(*bufs));

  req->off = off;
  POST;
}


void uv_fs_req_cleanup(uv_fs_t* req) {
  if (req == NULL)
    return;

  /* Only necessary for asychronous requests, i.e., requests with a callback.
   * Synchronous ones don't copy their arguments and have req->path and
   * req->new_path pointing to user-owned memory.  UV_FS_MKDTEMP is the
   * exception to the rule, it always allocates memory.
   */
  if (req->path != NULL && (req->cb != NULL || req->fs_type == UV_FS_MKDTEMP))
    uv__free((void*) req->path);  /* Memory is shared with req->new_path. */

  req->path = NULL;
  req->new_path = NULL;

  if (req->fs_type == UV_FS_SCANDIR && req->ptr != NULL)
    uv__fs_scandir_cleanup(req);

  if (req->bufs != req->bufsml)
    uv__free(req->bufs);
  req->bufs = NULL;

  if (req->ptr != &req->statbuf)
    uv__free(req->ptr);
  req->ptr = NULL;
}


int uv_fs_copyfile(uv_loop_t* loop,
                   uv_fs_t* req,
                   const char* path,
                   const char* new_path,
                   int flags,
                   uv_fs_cb cb) {
  INIT(COPYFILE);

  if (flags & ~(UV_FS_COPYFILE_EXCL |
                UV_FS_COPYFILE_FICLONE |
                UV_FS_COPYFILE_FICLONE_FORCE)) {
    return UV_EINVAL;
  }

  PATH2;
  req->flags = flags;
  POST;
}
