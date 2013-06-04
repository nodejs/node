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
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <poll.h>

#if defined(__linux__) || defined(__sun)
# include <sys/sendfile.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
# include <sys/socket.h>
# include <sys/uio.h>
#endif

#define INIT(type)                                                            \
  do {                                                                        \
    uv__req_init((loop), (req), UV_FS);                                       \
    (req)->fs_type = UV_FS_ ## type;                                          \
    (req)->errorno = 0;                                                       \
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
    if (NULL == ((req)->path = strdup((path))))                               \
      return uv__set_sys_error((loop), ENOMEM);                               \
  }                                                                           \
  while (0)

#define PATH2                                                                 \
  do {                                                                        \
    size_t path_len;                                                          \
    size_t new_path_len;                                                      \
                                                                              \
    path_len = strlen((path)) + 1;                                            \
    new_path_len = strlen((new_path)) + 1;                                    \
                                                                              \
    if (NULL == ((req)->path = malloc(path_len + new_path_len)))              \
      return uv__set_sys_error((loop), ENOMEM);                               \
                                                                              \
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
    || defined(__sun)
  struct timeval tv[2];
  tv[0].tv_sec  = req->atime;
  tv[0].tv_usec = (unsigned long)(req->atime * 1000000) % 1000000;
  tv[1].tv_sec  = req->mtime;
  tv[1].tv_usec = (unsigned long)(req->mtime * 1000000) % 1000000;
  return futimes(req->file, tv);
#else
  errno = ENOSYS;
  return -1;
#endif
}


static ssize_t uv__fs_read(uv_fs_t* req) {
  if (req->off < 0)
    return read(req->file, req->buf, req->len);
  else
    return pread(req->file, req->buf, req->len, req->off);
}


static int uv__fs_readdir_filter(const struct dirent* dent) {
  return strcmp(dent->d_name, ".") != 0 && strcmp(dent->d_name, "..") != 0;
}


/* This should have been called uv__fs_scandir(). */
static ssize_t uv__fs_readdir(uv_fs_t* req) {
  struct dirent **dents;
  int saved_errno;
  size_t off;
  size_t len;
  char *buf;
  int i;
  int n;

  n = scandir(req->path, &dents, uv__fs_readdir_filter, alphasort);

  if (n == -1 || n == 0)
    return n;

  len = 0;

  for (i = 0; i < n; i++)
    len += strlen(dents[i]->d_name) + 1;

  buf = malloc(len);

  if (buf == NULL) {
    errno = ENOMEM;
    n = -1;
    goto out;
  }

  off = 0;

  for (i = 0; i < n; i++) {
    len = strlen(dents[i]->d_name) + 1;
    memcpy(buf + off, dents[i]->d_name, len);
    off += len;
  }

  req->ptr = buf;

out:
  saved_errno = errno;
  {
    for (i = 0; i < n; i++)
      free(dents[i]);
    free(dents);
  }
  errno = saved_errno;

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

  len = req->len;
  in_fd = req->flags;
  out_fd = req->file;
  offset = req->off;
  use_pread = 1;

  /* Here are the rules regarding errors:
   *
   * 1. Read errors are reported only if nsent==0, otherwise we return nsent.
   *    The user needs to know that some data has already been sent, to stop
   *    him from sending it twice.
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
    r = sendfile(out_fd, in_fd, &off, req->len);

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
    r = sendfile(in_fd, out_fd, req->off, req->len, NULL, &len, 0);
#else
    /* The darwin sendfile takes len as an input for the length to send,
     * so make sure to initialize it with the caller's value. */
    len = req->len;
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

  if (req->off < 0)
    r = write(req->file, req->buf, req->len);
  else
    r = pwrite(req->file, req->buf, req->len, req->off);

#if defined(__APPLE__)
  pthread_mutex_unlock(&lock);
#endif

  return r;
}


static void uv__fs_work(struct uv__work* w) {
  int retry_on_eintr;
  uv_fs_t* req;
  ssize_t r;

  req = container_of(w, uv_fs_t, work_req);
  retry_on_eintr = !(req->fs_type == UV_FS_CLOSE);

  do {
    errno = 0;

#define X(type, action)                                                       \
  case UV_FS_ ## type:                                                        \
    r = action;                                                               \
    break;

    switch (req->fs_type) {
    X(CHMOD, chmod(req->path, req->mode));
    X(CHOWN, chown(req->path, req->uid, req->gid));
    X(CLOSE, close(req->file));
    X(FCHMOD, fchmod(req->file, req->mode));
    X(FCHOWN, fchown(req->file, req->uid, req->gid));
    X(FDATASYNC, uv__fs_fdatasync(req));
    X(FSTAT, fstat(req->file, &req->statbuf));
    X(FSYNC, fsync(req->file));
    X(FTRUNCATE, ftruncate(req->file, req->off));
    X(FUTIME, uv__fs_futime(req));
    X(LSTAT, lstat(req->path, &req->statbuf));
    X(LINK, link(req->path, req->new_path));
    X(MKDIR, mkdir(req->path, req->mode));
    X(OPEN, open(req->path, req->flags, req->mode));
    X(READ, uv__fs_read(req));
    X(READDIR, uv__fs_readdir(req));
    X(READLINK, uv__fs_readlink(req));
    X(RENAME, rename(req->path, req->new_path));
    X(RMDIR, rmdir(req->path));
    X(SENDFILE, uv__fs_sendfile(req));
    X(STAT, stat(req->path, &req->statbuf));
    X(SYMLINK, symlink(req->path, req->new_path));
    X(UNLINK, unlink(req->path));
    X(UTIME, uv__fs_utime(req));
    X(WRITE, uv__fs_write(req));
    default: abort();
    }

#undef X
  }
  while (r == -1 && errno == EINTR && retry_on_eintr);

  req->errorno = errno;
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

  if (req->errorno != 0) {
    req->errorno = uv_translate_sys_error(req->errorno);
    uv__set_artificial_error(req->loop, req->errorno);
  }

  if (status == -UV_ECANCELED) {
    assert(req->errorno == 0);
    req->errorno = UV_ECANCELED;
    uv__set_artificial_error(req->loop, UV_ECANCELED);
  }

  if (req->cb != NULL)
    req->cb(req);
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
                int uid,
                int gid,
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
                 int uid,
                 int gid,
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
               void* buf,
               size_t len,
               int64_t off,
               uv_fs_cb cb) {
  INIT(READ);
  req->file = file;
  req->buf = buf;
  req->len = len;
  req->off = off;
  POST;
}


int uv_fs_readdir(uv_loop_t* loop,
                  uv_fs_t* req,
                  const char* path,
                  int flags,
                  uv_fs_cb cb) {
  INIT(READDIR);
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
  req->len = len;
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
                void* buf,
                size_t len,
                int64_t off,
                uv_fs_cb cb) {
  INIT(WRITE);
  req->file = file;
  req->buf = buf;
  req->len = len;
  req->off = off;
  POST;
}


void uv_fs_req_cleanup(uv_fs_t* req) {
  free((void*) req->path);
  req->path = NULL;
  req->new_path = NULL;

  if (req->ptr != &req->statbuf)
    free(req->ptr);
  req->ptr = NULL;
}
