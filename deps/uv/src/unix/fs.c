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
#include "eio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <utime.h>
#include <sys/time.h>


#define ARGS1(a)       (a)
#define ARGS2(a,b)     (a), (b)
#define ARGS3(a,b,c)   (a), (b), (c)
#define ARGS4(a,b,c,d) (a), (b), (c), (d)

#define WRAP_EIO(type, eiofunc, func, args) \
  uv_fs_req_init(loop, req, type, path, cb); \
  if (cb) { \
    /* async */ \
    req->eio = eiofunc(args, EIO_PRI_DEFAULT, uv__fs_after, req); \
    if (!req->eio) { \
      uv__set_sys_error(loop, ENOMEM); \
      return -1; \
    } \
    uv_ref(loop); \
  } else { \
    /* sync */ \
    req->result = func(args); \
    if (req->result) { \
      uv__set_sys_error(loop, errno); \
    }  \
    return req->result; \
  } \
  return 0;


static void uv_fs_req_init(uv_loop_t* loop, uv_fs_t* req, uv_fs_type fs_type,
    const char* path, uv_fs_cb cb) {
  /* Make sure the thread pool is initialized. */
  uv_eio_init(loop);

  uv__req_init((uv_req_t*) req);
  req->type = UV_FS;
  req->loop = loop;
  req->fs_type = fs_type;
  req->cb = cb;
  req->result = 0;
  req->ptr = NULL;
  req->path = path ? strdup(path) : NULL;
  req->errorno = 0;
  req->eio = NULL;
}


void uv_fs_req_cleanup(uv_fs_t* req) {
  free(req->path);
  req->path = NULL;

  switch (req->fs_type) {
    case UV_FS_READDIR:
      assert(req->result > 0 ? (req->ptr != NULL) : (req->ptr == NULL));
      free(req->ptr);
      req->ptr = NULL;
      break;

    case UV_FS_STAT:
    case UV_FS_LSTAT:
      req->ptr = NULL;
      break;

    default:
      break;
  }
}


static int uv__fs_after(eio_req* eio) {
  char* name;
  int namelen;
  int buflen = 0;
  uv_fs_t* req = eio->data;
  int i;

  assert(req->cb);

  req->result = req->eio->result;
  req->errorno = uv_translate_sys_error(req->eio->errorno);

  switch (req->fs_type) {
    case UV_FS_READDIR:
      /*
       * XXX This is pretty bad.
       * We alloc and copy the large null terminated string list from libeio.
       * This is done because libeio is going to free eio->ptr2 after this
       * callback. We must keep it until uv_fs_req_cleanup. If we get rid of
       * libeio this can be avoided.
       */
      buflen = 0;
      name = req->eio->ptr2;

      for (i = 0; i < req->result; i++) {
        namelen = strlen(name);
        buflen += namelen + 1;
        name += namelen;
        assert(*name == '\0');
        name++;
      }

      if (buflen) {
        if ((req->ptr = malloc(buflen)))
          memcpy(req->ptr, req->eio->ptr2, buflen);
        else
          uv__set_sys_error(req->loop, ENOMEM);
      }
      break;

    case UV_FS_STAT:
    case UV_FS_LSTAT:
    case UV_FS_FSTAT:
      req->ptr = req->eio->ptr2;
      break;

    case UV_FS_READLINK:
      if (req->result == -1) {
        req->ptr = NULL;
        break;
      }
      assert(req->result > 0);

      /* Make zero-terminated copy of req->eio->ptr2 */
      if ((req->ptr = name = malloc(req->result + 1))) {
        memcpy(name, req->eio->ptr2, req->result);
        name[req->result] = '\0';
        req->result = 0;
      }
      else {
        req->errorno = ENOMEM;
        req->result = -1;
      }
      break;

    default:
      break;
  }

  uv_unref(req->loop);
  req->eio = NULL; /* Freed by libeio */

  req->cb(req);
  return 0;
}


int uv_fs_close(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  char* path = NULL;
  WRAP_EIO(UV_FS_CLOSE, eio_close, close, ARGS1(file));
}


int uv_fs_open(uv_loop_t* loop, uv_fs_t* req, const char* path, int flags,
    int mode, uv_fs_cb cb) {
  uv_fs_req_init(loop, req, UV_FS_OPEN, path, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_open(path, flags, mode, EIO_PRI_DEFAULT, uv__fs_after, req);
    if (!req->eio) {
      uv__set_sys_error(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = open(path, flags, mode);
    if (req->result < 0) {
      uv__set_sys_error(loop, errno);
      return -1;
    }

    uv__cloexec(req->result, 1);

    return req->result;
  }

  return 0;
}


int uv_fs_read(uv_loop_t* loop, uv_fs_t* req, uv_file fd, void* buf,
    size_t length, off_t offset, uv_fs_cb cb) {
  uv_fs_req_init(loop, req, UV_FS_READ, NULL, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_read(fd, buf, length, offset, EIO_PRI_DEFAULT,
        uv__fs_after, req);

    if (!req->eio) {
      uv__set_sys_error(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = offset < 0 ?
      read(fd, buf, length) :
      pread(fd, buf, length, offset);

    if (req->result < 0) {
      uv__set_sys_error(loop, errno);
      return -1;
    }

    return req->result;
  }

  return 0;
}


int uv_fs_unlink(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
  WRAP_EIO(UV_FS_UNLINK, eio_unlink, unlink, ARGS1(path))
}


int uv_fs_write(uv_loop_t* loop, uv_fs_t* req, uv_file file, void* buf,
    size_t length, off_t offset, uv_fs_cb cb) {
  uv_fs_req_init(loop, req, UV_FS_WRITE, NULL, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_write(file, buf, length, offset, EIO_PRI_DEFAULT,
        uv__fs_after, req);
    if (!req->eio) {
      uv__set_sys_error(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = offset < 0 ?
        write(file, buf, length) :
        pwrite(file, buf, length, offset);

    if (req->result < 0) {
      uv__set_sys_error(loop, errno);
      return -1;
    }

    return req->result;
  }

  return 0;
}


int uv_fs_mkdir(uv_loop_t* loop, uv_fs_t* req, const char* path, int mode,
    uv_fs_cb cb) {
  WRAP_EIO(UV_FS_MKDIR, eio_mkdir, mkdir, ARGS2(path, mode))
}


int uv_fs_rmdir(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
  WRAP_EIO(UV_FS_RMDIR, eio_rmdir, rmdir, ARGS1(path))
}


int uv_fs_readdir(uv_loop_t* loop, uv_fs_t* req, const char* path, int flags,
    uv_fs_cb cb) {
  int r;
  struct dirent* entry;
  size_t size = 0;
  size_t d_namlen = 0;

  uv_fs_req_init(loop, req, UV_FS_READDIR, path, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_readdir(path, flags, EIO_PRI_DEFAULT, uv__fs_after, req);
    if (!req->eio) {
      uv__set_sys_error(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    DIR* dir = opendir(path);
    if (!dir) {
      uv__set_sys_error(loop, errno);
      req->result = -1;
      return -1;
    }

    /* req->result stores number of entries */
    req->result = 0;

    while ((entry = readdir(dir))) {
      d_namlen = strlen(entry->d_name);

      /* Skip . and .. */
      if ((d_namlen == 1 && entry->d_name[0] == '.') ||
          (d_namlen == 2 && entry->d_name[0] == '.' &&
           entry->d_name[1] == '.')) {
        continue;
      }

      req->ptr = realloc(req->ptr, size + d_namlen + 1);
      /* TODO check ENOMEM */
      memcpy((char*)req->ptr + size, entry->d_name, d_namlen);
      size += d_namlen;
      ((char*)req->ptr)[size] = '\0';
      size++;
      req->result++;
    }

    r = closedir(dir);
    if (r) {
      uv__set_sys_error(loop, errno);
      req->result = -1;
      return -1;
    }

    return req->result;
  }

  return 0;
}


int uv_fs_stat(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
  char* pathdup;
  int pathlen;

  uv_fs_req_init(loop, req, UV_FS_STAT, path, cb);

  /* TODO do this without duplicating the string. */
  /* TODO security */
  pathdup = strdup(path);
  pathlen = strlen(path);

  if (pathlen > 0 && path[pathlen - 1] == '\\') {
    /* TODO do not modify input string */
    pathdup[pathlen - 1] = '\0';
  }

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_stat(pathdup, EIO_PRI_DEFAULT, uv__fs_after, req);

    free(pathdup);

    if (!req->eio) {
      uv__set_sys_error(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = stat(pathdup, &req->statbuf);

    free(pathdup);

    if (req->result < 0) {
      uv__set_sys_error(loop, errno);
      return -1;
    }

    req->ptr = &req->statbuf;
    return req->result;
  }

  return 0;
}


int uv_fs_fstat(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  uv_fs_req_init(loop, req, UV_FS_FSTAT, NULL, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_fstat(file, EIO_PRI_DEFAULT, uv__fs_after, req);

    if (!req->eio) {
      uv__set_sys_error(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = fstat(file, &req->statbuf);

    if (req->result < 0) {
      uv__set_sys_error(loop, errno);
      return -1;
    }

    req->ptr = &req->statbuf;
    return req->result;
  }

  return 0;
}


int uv_fs_rename(uv_loop_t* loop, uv_fs_t* req, const char* path, const char* new_path,
    uv_fs_cb cb) {
  WRAP_EIO(UV_FS_RENAME, eio_rename, rename, ARGS2(path, new_path))
}


int uv_fs_fsync(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  char* path = NULL;
  WRAP_EIO(UV_FS_FSYNC, eio_fsync, fsync, ARGS1(file))
}


int uv_fs_fdatasync(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  char* path = NULL;
#if defined(__FreeBSD__) \
  || (__ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1060)
  /* freebsd and pre-10.6 darwin don't have fdatasync,
   * do a full fsync instead.
   */
  WRAP_EIO(UV_FS_FDATASYNC, eio_fdatasync, fsync, ARGS1(file))
#else
  WRAP_EIO(UV_FS_FDATASYNC, eio_fdatasync, fdatasync, ARGS1(file))
#endif
}


int uv_fs_ftruncate(uv_loop_t* loop, uv_fs_t* req, uv_file file, off_t offset,
    uv_fs_cb cb) {
  char* path = NULL;
  WRAP_EIO(UV_FS_FTRUNCATE, eio_ftruncate, ftruncate, ARGS2(file, offset))
}


int uv_fs_sendfile(uv_loop_t* loop, uv_fs_t* req, uv_file out_fd, uv_file in_fd,
    off_t in_offset, size_t length, uv_fs_cb cb) {
  char* path = NULL;
  WRAP_EIO(UV_FS_SENDFILE, eio_sendfile, eio_sendfile_sync,
      ARGS4(out_fd, in_fd, in_offset, length))
}


int uv_fs_chmod(uv_loop_t* loop, uv_fs_t* req, const char* path, int mode,
    uv_fs_cb cb) {
  WRAP_EIO(UV_FS_CHMOD, eio_chmod, chmod, ARGS2(path, mode))
}


static int _utime(const char* path, double atime, double mtime) {
  struct utimbuf buf;
  buf.actime = atime;
  buf.modtime = mtime;
  return utime(path, &buf);
}


int uv_fs_utime(uv_loop_t* loop, uv_fs_t* req, const char* path, double atime,
    double mtime, uv_fs_cb cb) {
  WRAP_EIO(UV_FS_UTIME, eio_utime, _utime, ARGS3(path, atime, mtime))
}


#if HAVE_FUTIMES
static int _futime(const uv_file file, double atime, double mtime) {
  struct timeval tv[2];

  /* FIXME possible loss of precision in floating-point arithmetic? */
  tv[0].tv_sec = atime;
  tv[0].tv_usec = (unsigned long)(atime * 1000000) % 1000000;

  tv[1].tv_sec = mtime;
  tv[1].tv_usec = (unsigned long)(mtime * 1000000) % 1000000;

#ifdef __sun
  return futimesat(file, NULL, tv);
#else
  return futimes(file, tv);
#endif
}
#endif


int uv_fs_futime(uv_loop_t* loop, uv_fs_t* req, uv_file file, double atime,
    double mtime, uv_fs_cb cb) {
#if HAVE_FUTIMES
  const char* path = NULL;

  uv_fs_req_init(loop, req, UV_FS_FUTIME, path, cb);

  WRAP_EIO(UV_FS_FUTIME, eio_futime, _futime, ARGS3(file, atime, mtime))
#else
  uv__set_sys_error(loop, ENOSYS);
  return -1;
#endif
}


int uv_fs_lstat(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
  char* pathdup;
  int pathlen;

  uv_fs_req_init(loop, req, UV_FS_LSTAT, path, cb);

  /* TODO do this without duplicating the string. */
  /* TODO security */
  pathdup = strdup(path);
  pathlen = strlen(path);

  if (pathlen > 0 && path[pathlen - 1] == '\\') {
    /* TODO do not modify input string */
    pathdup[pathlen - 1] = '\0';
  }

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_lstat(pathdup, EIO_PRI_DEFAULT, uv__fs_after, req);

    free(pathdup);

    if (!req->eio) {
      uv__set_sys_error(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = lstat(pathdup, &req->statbuf);

    free(pathdup);

    if (req->result < 0) {
      uv__set_sys_error(loop, errno);
      return -1;
    }

    req->ptr = &req->statbuf;
    return req->result;
  }

  return 0;
}


int uv_fs_link(uv_loop_t* loop, uv_fs_t* req, const char* path,
    const char* new_path, uv_fs_cb cb) {
  WRAP_EIO(UV_FS_LINK, eio_link, link, ARGS2(path, new_path))
}


int uv_fs_symlink(uv_loop_t* loop, uv_fs_t* req, const char* path,
    const char* new_path, int flags, uv_fs_cb cb) {
  WRAP_EIO(UV_FS_SYMLINK, eio_symlink, symlink, ARGS2(path, new_path))
}


int uv_fs_readlink(uv_loop_t* loop, uv_fs_t* req, const char* path,
    uv_fs_cb cb) {
  ssize_t size;
  char* buf;

  uv_fs_req_init(loop, req, UV_FS_READLINK, path, cb);

  if (cb) {
    if ((req->eio = eio_readlink(path, EIO_PRI_DEFAULT, uv__fs_after, req))) {
      uv_ref(loop);
      return 0;
    } else {
      uv__set_sys_error(loop, ENOMEM);
      return -1;
    }
  } else {
    /* pathconf(_PC_PATH_MAX) may return -1 to signify that path
     * lengths have no upper limit or aren't suitable for malloc'ing.
     */
    if ((size = pathconf(path, _PC_PATH_MAX)) == -1) {
#if defined(PATH_MAX)
      size = PATH_MAX;
#else
      size = 4096;
#endif
    }

    if ((buf = malloc(size + 1)) == NULL) {
      uv__set_sys_error(loop, ENOMEM);
      return -1;
    }

    if ((size = readlink(path, buf, size)) == -1) {
      req->errorno = errno;
      req->result = -1;
      free(buf);
    } else {
      /* Cannot conceivably fail since it shrinks the buffer. */
      buf = realloc(buf, size + 1);
      buf[size] = '\0';
      req->result = 0;
      req->ptr = buf;
    }

    return req->result;
  }

  assert(0 && "unreachable");
}


int uv_fs_fchmod(uv_loop_t* loop, uv_fs_t* req, uv_file file, int mode,
    uv_fs_cb cb) {
  char* path = NULL;
  WRAP_EIO(UV_FS_FCHMOD, eio_fchmod, fchmod, ARGS2(file, mode))
}


int uv_fs_chown(uv_loop_t* loop, uv_fs_t* req, const char* path, int uid,
    int gid, uv_fs_cb cb) {
  WRAP_EIO(UV_FS_CHOWN, eio_chown, chown, ARGS3(path, uid, gid))
}


int uv_fs_fchown(uv_loop_t* loop, uv_fs_t* req, uv_file file, int uid, int gid,
    uv_fs_cb cb) {
  char* path = NULL;
  WRAP_EIO(UV_FS_FCHOWN, eio_fchown, fchown, ARGS3(file, uid, gid))
}


static void uv__work(eio_req* eio) {
  uv_work_t* req = eio->data;
  if (req->work_cb) {
    req->work_cb(req);
  }
}


static int uv__after_work(eio_req *eio) {
  uv_work_t* req = eio->data;
  uv_unref(req->loop);
  if (req->after_work_cb) {
    req->after_work_cb(req);
  }
  return 0;
}


int uv_queue_work(uv_loop_t* loop, uv_work_t* req, uv_work_cb work_cb,
    uv_after_work_cb after_work_cb) {
  void* data = req->data;

  uv_eio_init(loop);

  uv__req_init((uv_req_t*) req);
  uv_ref(loop);
  req->loop = loop;
  req->data = data;
  req->work_cb = work_cb;
  req->after_work_cb = after_work_cb;

  req->eio = eio_custom(uv__work, EIO_PRI_DEFAULT, uv__after_work, req);

  if (!req->eio) {
    uv__set_sys_error(loop, ENOMEM);
    return -1;
  }

  return 0;
}
