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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <poll.h>

#if defined(__DragonFly__)  ||                                            \
    defined(__FreeBSD__)    ||                                            \
    defined(__OpenBSD__)    ||                                            \
    defined(__NetBSD__)
# define HAVE_PREADV 1
#elif defined(__linux__)
# include <linux/version.h>
# if defined(__GLIBC_PREREQ)
#   if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30) &&                    \
       __GLIBC_PREREQ(2,10)
#    define HAVE_PREADV 1
#   else
#    define HAVE_PREADV 0
#   endif
# else
#  define HAVE_PREADV 0
# endif
#else
# define HAVE_PREADV 0
#endif

#if defined(__linux__) || defined(__sun)
# include <sys/sendfile.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
# include <sys/socket.h>
#endif

#if HAVE_PREADV || defined(__APPLE__)
# include <sys/uio.h>
#endif

#define INIT(type)                                                            \
  do {                                                                        \
    uv__req_init((loop), (req), UV_FS);                                       \
    (req)->fs_type = UV_FS_ ## type;                                          \
    (req)->result = 0;                                                        \
    (req)->ptr = NULL;                                                        \
    (req)->loop = loop;                                                       \
    (req)->path = NULL;                                                       \
    (req)->new_path = NULL;                                                   \
    (req)->cb = (cb);                                                         \
  }                                                                           \
  while (0)

#define PATH                                                                  \
  do {                                                                        \
    (req)->path = strdup(path);                                               \
    if ((req)->path == NULL)                                                  \
      return -ENOMEM;                                                         \
  }                                                                           \
  while (0)

#define PATH2                                                                 \
  do {                                                                        \
    size_t path_len;                                                          \
    size_t new_path_len;                                                      \
    path_len = strlen((path)) + 1;                                            \
    new_path_len = strlen((new_path)) + 1;                                    \
    (req)->path = malloc(path_len + new_path_len);                            \
    if ((req)->path == NULL)                                                  \
      return -ENOMEM;                                                         \
    (req)->new_path = (req)->path + path_len;                                 \
    memcpy((void*) (req)->path, (path), path_len);                            \
    memcpy((void*) (req)->new_path, (new_path), new_path_len);                \
  }                                                                           \
  while (0)

#define POST                                                                  \
  do {                                                                        \
    if ((cb) != NULL) {                                                       \
      uv__work_submit((loop), &(req)->work_req, uv__fs_work, uv__fs_done);    \
      return 0;                                                               \
    }                                                                         \
    else {                                                                    \
      uv__fs_work(&(req)->work_req);                                          \
      uv__fs_done(&(req)->work_req, 0);                                       \
      return (req)->result;                                                   \
    }                                                                         \
  }                                                                           \
  while (0)


static ssize_t uv__fs_fdatasync(uv_fs_t* req) {
#if defined(__linux__) || defined(__sun) || defined(__NetBSD__)
  return fdatasync(req->file);
#elif defined(__APPLE__) && defined(F_FULLFSYNC)
  return fcntl(req->file, F_FULLFSYNC);
#else
  return fsync(req->file);
#endif
}


static ssize_t uv__fs_futime(uv_fs_t* req) {
#if defined(__linux__)
  /* utimesat() has nanosecond resolution but we stick to microseconds
   * for the sake of consistency with other platforms.
   */
  static int no_utimesat;
  struct timespec ts[2];
  struct timeval tv[2];
  char path[sizeof("/proc/self/fd/") + 3 * sizeof(int)];
  int r;

  if (no_utimesat)
    goto skip;

  ts[0].tv_sec  = req->atime;
  ts[0].tv_nsec = (unsigned long)(req->atime * 1000000) % 1000000 * 1000;
  ts[1].tv_sec  = req->mtime;
  ts[1].tv_nsec = (unsigned long)(req->mtime * 1000000) % 1000000 * 1000;

  r = uv__utimesat(req->file, NULL, ts, 0);
  if (r == 0)
    return r;

  if (errno != ENOSYS)
    return r;

  no_utimesat = 1;

skip:

  tv[0].tv_sec  = req->atime;
  tv[0].tv_usec = (unsigned long)(req->atime * 1000000) % 1000000;
  tv[1].tv_sec  = req->mtime;
  tv[1].tv_usec = (unsigned long)(req->mtime * 1000000) % 1000000;
  snprintf(path, sizeof(path), "/proc/self/fd/%d", (int) req->file);

  r = utimes(path, tv);
  if (r == 0)
    return r;

  switch (errno) {
  case ENOENT:
    if (fcntl(req->file, F_GETFL) == -1 && errno == EBADF)
      break;
    /* Fall through. */

  case EACCES:
  case ENOTDIR:
    errno = ENOSYS;
    break;
  }

  return r;

#elif defined(__APPLE__)                                                      \
    || defined(__DragonFly__)                                                 \
    || defined(__FreeBSD__)                                                   \
    || defined(__NetBSD__)                                                    \
    || defined(__OpenBSD__)                                                   \
    || defined(__sun)
  struct timeval tv[2];
  tv[0].tv_sec  = req->atime;
  tv[0].tv_usec = (unsigned long)(req->atime * 1000000) % 1000000;
  tv[1].tv_sec  = req->mtime;
  tv[1].tv_usec = (unsigned long)(req->mtime * 1000000) % 1000000;
# if defined(__sun)
  return futimesat(req->file, NULL, tv);
# else
  return futimes(req->file, tv);
# endif
#else
  errno = ENOSYS;
  return -1;
#endif
}


static ssize_t uv__fs_mkdtemp(uv_fs_t* req) {
  return mkdtemp((char*) req->path) ? 0 : -1;
}


static ssize_t uv__fs_read(uv_fs_t* req) {
  ssize_t result;

#if defined(_AIX)
  struct stat buf;
  if(fstat(req->file, &buf))
    return -1;
  if(S_ISDIR(buf.st_mode)) {
    errno = EISDIR;
    return -1;
  }
#endif /* defined(_AIX) */
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
    static int no_preadv;
    if (no_preadv)
# endif
    {
      off_t nread;
      size_t index;

# if defined(__linux__)
    retry:
# endif
      nread = 0;
      index = 0;
      result = 1;
      do {
        if (req->bufs[index].len > 0) {
          result = pread(req->file,
                         req->bufs[index].base,
                         req->bufs[index].len,
                         req->off + nread);
          if (result > 0)
            nread += result;
        }
        index++;
      } while (index < req->nbufs && result > 0);
      if (nread > 0)
        result = nread;
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
  if (req->bufs != req->bufsml)
    free(req->bufs);
  return result;
}


#if defined(__OpenBSD__) || (defined(__APPLE__) && !defined(MAC_OS_X_VERSION_10_8))
static int uv__fs_scandir_filter(uv__dirent_t* dent) {
#else
static int uv__fs_scandir_filter(const uv__dirent_t* dent) {
#endif
  return strcmp(dent->d_name, ".") != 0 && strcmp(dent->d_name, "..") != 0;
}


static ssize_t uv__fs_scandir(uv_fs_t* req) {
  uv__dirent_t **dents;
  int saved_errno;
  int n;

  dents = NULL;
  n = scandir(req->path, &dents, uv__fs_scandir_filter, alphasort);

  /* NOTE: We will use nbufs as an index field */
  req->nbufs = 0;

  if (n == 0)
    goto out; /* osx still needs to deallocate some memory */
  else if (n == -1)
    return n;

  req->ptr = dents;

  return n;

out:
  saved_errno = errno;
  if (dents != NULL) {
    int i;

    for (i = 0; i < n; i++)
      free(dents[i]);
    free(dents);
  }
  errno = saved_errno;

  req->ptr = NULL;

  return n;
}


static ssize_t uv__fs_readlink(uv_fs_t* req) {
  ssize_t len;
  char* buf;

  len = pathconf(req->path, _PC_PATH_MAX);

  if (len == -1) {
#if defined(PATH_MAX)
    len = PATH_MAX;
#else
    len = 4096;
#endif
  }

  buf = malloc(len + 1);

  if (buf == NULL) {
    errno = ENOMEM;
    return -1;
  }

  len = readlink(req->path, buf, len);

  if (len == -1) {
    free(buf);
    return -1;
  }

  buf[len] = '\0';
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
#elif defined(__FreeBSD__) || defined(__APPLE__)
  {
    off_t len;
    ssize_t r;

    /* sendfile() on FreeBSD and Darwin returns EAGAIN if the target fd is in
     * non-blocking mode and not all data could be written. If a non-zero
     * number of bytes have been sent, we don't consider it an error.
     */

#if defined(__FreeBSD__)
    len = 0;
    r = sendfile(in_fd, out_fd, req->off, req->bufsml[0].len, NULL, &len, 0);
#else
    /* The darwin sendfile takes len as an input for the length to send,
     * so make sure to initialize it with the caller's value. */
    len = req->bufsml[0].len;
    r = sendfile(in_fd, out_fd, req->off, &len, NULL, 0);
#endif

    if (r != -1 || len != 0) {
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
  struct utimbuf buf;
  buf.actime = req->atime;
  buf.modtime = req->mtime;
  return utime(req->path, &buf); /* TODO use utimes() where available */
}


static ssize_t uv__fs_write(uv_fs_t* req) {
  ssize_t r;

  /* Serialize writes on OS X, concurrent write() and pwrite() calls result in
   * data loss. We can't use a per-file descriptor lock, the descriptor may be
   * a dup().
   */
#if defined(__APPLE__)
  static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&lock);
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
    static int no_pwritev;
    if (no_pwritev)
# endif
    {
      off_t written;
      size_t index;

# if defined(__linux__)
    retry:
# endif
      written = 0;
      index = 0;
      r = 0;
      do {
        if (req->bufs[index].len > 0) {
          r = pwrite(req->file,
                     req->bufs[index].base,
                     req->bufs[index].len,
                     req->off + written);
          if (r > 0)
            written += r;
        }
        index++;
      } while (index < req->nbufs && r >= 0);
      if (written > 0)
        r = written;
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
  pthread_mutex_unlock(&lock);
#endif

  if (req->bufs != req->bufsml)
    free(req->bufs);

  return r;
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
#elif !defined(_AIX) && \
  (defined(_BSD_SOURCE) || defined(_SVID_SOURCE) || defined(_XOPEN_SOURCE))
  dst->st_atim.tv_sec = src->st_atim.tv_sec;
  dst->st_atim.tv_nsec = src->st_atim.tv_nsec;
  dst->st_mtim.tv_sec = src->st_mtim.tv_sec;
  dst->st_mtim.tv_nsec = src->st_mtim.tv_nsec;
  dst->st_ctim.tv_sec = src->st_ctim.tv_sec;
  dst->st_ctim.tv_nsec = src->st_ctim.tv_nsec;
# if defined(__DragonFly__)  || \
     defined(__FreeBSD__)    || \
     defined(__OpenBSD__)    || \
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
  uv__to_stat(&pbuf, buf);
  return ret;
}


static int uv__fs_lstat(const char *path, uv_stat_t *buf) {
  struct stat pbuf;
  int ret;
  ret = lstat(path, &pbuf);
  uv__to_stat(&pbuf, buf);
  return ret;
}


static int uv__fs_fstat(int fd, uv_stat_t *buf) {
  struct stat pbuf;
  int ret;
  ret = fstat(fd, &pbuf);
  uv__to_stat(&pbuf, buf);
  return ret;
}


static void uv__fs_work(struct uv__work* w) {
  int retry_on_eintr;
  uv_fs_t* req;
  ssize_t r;
#ifdef O_CLOEXEC
  static int no_cloexec_support;
#endif  /* O_CLOEXEC */

  req = container_of(w, uv_fs_t, work_req);
  retry_on_eintr = !(req->fs_type == UV_FS_CLOSE);

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
    X(FCHMOD, fchmod(req->file, req->mode));
    X(FCHOWN, fchown(req->file, req->uid, req->gid));
    X(FDATASYNC, uv__fs_fdatasync(req));
    X(FSTAT, uv__fs_fstat(req->file, &req->statbuf));
    X(FSYNC, fsync(req->file));
    X(FTRUNCATE, ftruncate(req->file, req->off));
    X(FUTIME, uv__fs_futime(req));
    X(LSTAT, uv__fs_lstat(req->path, &req->statbuf));
    X(LINK, link(req->path, req->new_path));
    X(MKDIR, mkdir(req->path, req->mode));
    X(MKDTEMP, uv__fs_mkdtemp(req));
    X(READ, uv__fs_read(req));
    X(SCANDIR, uv__fs_scandir(req));
    X(READLINK, uv__fs_readlink(req));
    X(RENAME, rename(req->path, req->new_path));
    X(RMDIR, rmdir(req->path));
    X(SENDFILE, uv__fs_sendfile(req));
    X(STAT, uv__fs_stat(req->path, &req->statbuf));
    X(SYMLINK, symlink(req->path, req->new_path));
    X(UNLINK, unlink(req->path));
    X(UTIME, uv__fs_utime(req));
    X(WRITE, uv__fs_write(req));
    case UV_FS_OPEN:
#ifdef O_CLOEXEC
      /* Try O_CLOEXEC before entering locks */
      if (!no_cloexec_support) {
        r = open(req->path, req->flags | O_CLOEXEC, req->mode);
        if (r >= 0)
          break;
        if (errno != EINVAL)
          break;
        no_cloexec_support = 1;
      }
#endif  /* O_CLOEXEC */
      if (req->cb != NULL)
        uv_rwlock_rdlock(&req->loop->cloexec_lock);
      r = open(req->path, req->flags, req->mode);

      /*
       * In case of failure `uv__cloexec` will leave error in `errno`,
       * so it is enough to just set `r` to `-1`.
       */
      if (r >= 0 && uv__cloexec(r, 1) != 0) {
        r = uv__close(r);
        if (r != 0 && r != -EINPROGRESS)
          abort();
        r = -1;
      }
      if (req->cb != NULL)
        uv_rwlock_rdunlock(&req->loop->cloexec_lock);
      break;
    default: abort();
    }

#undef X
  }
  while (r == -1 && errno == EINTR && retry_on_eintr);

  if (r == -1)
    req->result = -errno;
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

  if (status == -ECANCELED) {
    assert(req->result == 0);
    req->result = -ECANCELED;
  }

  if (req->cb != NULL)
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
  req->path = strdup(tpl);
  if (req->path == NULL)
    return -ENOMEM;
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
  req->file = file;

  req->nbufs = nbufs;
  req->bufs = req->bufsml;
  if (nbufs > ARRAY_SIZE(req->bufsml))
    req->bufs = malloc(nbufs * sizeof(*bufs));

  if (req->bufs == NULL)
    return -ENOMEM;

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
  req->file = file;

  req->nbufs = nbufs;
  req->bufs = req->bufsml;
  if (nbufs > ARRAY_SIZE(req->bufsml))
    req->bufs = malloc(nbufs * sizeof(*bufs));

  if (req->bufs == NULL)
    return -ENOMEM;

  memcpy(req->bufs, bufs, nbufs * sizeof(*bufs));

  req->off = off;
  POST;
}


void uv_fs_req_cleanup(uv_fs_t* req) {
  free((void*) req->path);
  req->path = NULL;
  req->new_path = NULL;

  if (req->fs_type == UV_FS_SCANDIR && req->ptr != NULL)
    uv__fs_scandir_cleanup(req);

  if (req->ptr != &req->statbuf)
    free(req->ptr);
  req->ptr = NULL;
}
