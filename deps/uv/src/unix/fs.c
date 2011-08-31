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


static void uv_fs_req_init(uv_loop_t* loop, uv_fs_t* req, uv_fs_type fs_type,
    uv_fs_cb cb) {
  /* Make sure the thread pool is initialized. */
  uv_eio_init(loop);

  uv__req_init((uv_req_t*) req);
  req->type = UV_FS;
  req->loop = loop;
  req->fs_type = fs_type;
  req->cb = cb;
  req->result = 0;
  req->ptr = NULL;
  req->errorno = 0;
  req->eio = NULL;
}


void uv_fs_req_cleanup(uv_fs_t* req) {
  switch (req->fs_type) {
    case UV_FS_READDIR:
      assert(req->ptr);
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
  req->errorno = req->eio->errorno;

  if (req->fs_type == UV_FS_READDIR) {
    /*
     * XXX This is pretty bad.
     * We alloc and copy the large null termiated string list from libeio.
     * This is done because libeio is going to free eio->ptr2 after this
     * callback. We must keep it until uv_fs_req_cleanup. If we get rid of
     * libeio this can be avoided.
     */
    buflen = 0;
    name = req->eio->ptr2;
    for (i = 0; i < req->result; i++) {
      namelen = strlen(name);
      buflen += namelen + 1;
      /* TODO check ENOMEM */
      name += namelen;
      assert(*name == '\0');
      name++;
    }
    req->ptr = malloc(buflen);
    memcpy(req->ptr, req->eio->ptr2, buflen);
  } else if (req->fs_type == UV_FS_STAT || req->fs_type == UV_FS_LSTAT) {
    req->ptr = req->eio->ptr2;
  }

  uv_unref(req->loop);
  req->eio = NULL; /* Freed by libeio */

  req->cb(req);
  return 0;
}


int uv_fs_close(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  uv_fs_req_init(loop, req, UV_FS_CLOSE, cb);
  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_close(file, EIO_PRI_DEFAULT, uv__fs_after, req);
    if (!req->eio) {
      uv_err_new(loop, ENOMEM);
      return -1;
    }
  } else {
    /* sync */
    if ((req->result = uv__close(file))) {
      uv_err_new(loop, errno);
      return -1;
    }
  }

  return 0;
}


int uv_fs_open(uv_loop_t* loop, uv_fs_t* req, const char* path, int flags,
    int mode, uv_fs_cb cb) {
  uv_fs_req_init(loop, req, UV_FS_OPEN, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_open(path, flags, mode, EIO_PRI_DEFAULT, uv__fs_after, req);
    if (!req->eio) {
      uv_err_new(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = open(path, flags, mode);
    if (req->result < 0) {
      uv_err_new(loop, errno);
      return -1;
    }

    uv__cloexec(req->result, 1);
  }

  return 0;
}


int uv_fs_read(uv_loop_t* loop, uv_fs_t* req, uv_file fd, void* buf,
    size_t length, off_t offset, uv_fs_cb cb) {
  uv_fs_req_init(loop, req, UV_FS_READ, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_read(fd, buf, length, offset, EIO_PRI_DEFAULT,
        uv__fs_after, req);

    if (!req->eio) {
      uv_err_new(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = offset < 0 ?
      read(fd, buf, length) :
      pread(fd, buf, length, offset);

    if (req->result < 0) {
      uv_err_new(loop, errno);
      return -1;
    }
  }

  return 0;
}


int uv_fs_unlink(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
  uv_fs_req_init(loop, req, UV_FS_UNLINK, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_unlink(path, EIO_PRI_DEFAULT, uv__fs_after, req);
    if (!req->eio) {
      uv_err_new(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = unlink(path);

    if (req->result) {
      uv_err_new(loop, errno);
      return -1;
    }
  }

  return 0;
}


int uv_fs_write(uv_loop_t* loop, uv_fs_t* req, uv_file file, void* buf,
    size_t length, off_t offset, uv_fs_cb cb) {
  uv_fs_req_init(loop, req, UV_FS_WRITE, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_write(file, buf, length, offset, EIO_PRI_DEFAULT,
        uv__fs_after, req);
    if (!req->eio) {
      uv_err_new(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = offset < 0 ?
        write(file, buf, length) :
        pwrite(file, buf, length, offset);

    if (req->result < 0) {
      uv_err_new(loop, errno);
      return -1;
    }
  }

  return 0;
}


int uv_fs_mkdir(uv_loop_t* loop, uv_fs_t* req, const char* path, int mode,
    uv_fs_cb cb) {
  uv_fs_req_init(loop, req, UV_FS_MKDIR, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_mkdir(path, mode, EIO_PRI_DEFAULT, uv__fs_after, req);
    if (!req->eio) {
      uv_err_new(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = mkdir(path, mode);

    if (req->result < 0) {
      uv_err_new(loop, errno);
      return -1;
    }
  }

  return 0;
}


int uv_fs_rmdir(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
  uv_fs_req_init(loop, req, UV_FS_RMDIR, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_rmdir(path, EIO_PRI_DEFAULT, uv__fs_after, req);
    if (!req->eio) {
      uv_err_new(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = rmdir(path);

    if (req->result < 0) {
      uv_err_new(loop, errno);
      return -1;
    }
  }

  return 0;
}


int uv_fs_readdir(uv_loop_t* loop, uv_fs_t* req, const char* path, int flags,
    uv_fs_cb cb) {
  int r;
  struct dirent* entry;
  size_t size = 0;
  size_t d_namlen = 0;

  uv_fs_req_init(loop, req, UV_FS_READDIR, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_readdir(path, flags, EIO_PRI_DEFAULT, uv__fs_after, req);
    if (!req->eio) {
      uv_err_new(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    DIR* dir = opendir(path);
    if (!dir) {
      uv_err_new(loop, errno);
      return -1;
    }

    while ((entry = readdir(dir))) {
      d_namlen = strlen(entry->d_name);
      req->ptr = realloc(req->ptr, size + d_namlen + 1);
      /* TODO check ENOMEM */
      /* TODO skip . and .. */
      memcpy((char*)req->ptr + size, entry->d_name, d_namlen);
      size += d_namlen;
      ((char*)req->ptr)[size] = '\0';
      size++;
    }

    r = closedir(dir);
    if (r) {
      uv_err_new(loop, errno);
      return -1;
    }
  }

  return 0;
}


int uv_fs_stat(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
  char* pathdup = path;
  int pathlen;

  uv_fs_req_init(loop, req, UV_FS_STAT, cb);

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
      uv_err_new(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = stat(pathdup, &req->statbuf);

    free(pathdup);

    if (req->result < 0) {
      uv_err_new(loop, errno);
      return -1;
    }

    req->ptr = &req->statbuf;
  }

  return 0;
}


int uv_fs_fstat(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_rename(uv_loop_t* loop, uv_fs_t* req, const char* path, const char* new_path,
    uv_fs_cb cb) {
  uv_fs_req_init(loop, req, UV_FS_RENAME, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_rename(path, new_path, EIO_PRI_DEFAULT, uv__fs_after, req);
    if (!req->eio) {
      uv_err_new(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = rename(path, new_path);

    if (req->result) {
      uv_err_new(loop, errno);
      return -1;
    }
  }

  return 0;
}


int uv_fs_fsync(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  uv_fs_req_init(loop, req, UV_FS_FSYNC, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_fsync(file, EIO_PRI_DEFAULT, uv__fs_after, req);
    if (!req->eio) {
      uv_err_new(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = fsync(file);

    if (req->result) {
      uv_err_new(loop, errno);
      return -1;
    }
  }

  return 0;
}


int uv_fs_fdatasync(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  uv_fs_req_init(loop, req, UV_FS_FDATASYNC, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_fdatasync(file, EIO_PRI_DEFAULT, uv__fs_after, req);
    if (!req->eio) {
      uv_err_new(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = fdatasync(file);

    if (req->result) {
      uv_err_new(loop, errno);
      return -1;
    }
  }

  return 0;
}


int uv_fs_ftruncate(uv_loop_t* loop, uv_fs_t* req, uv_file file, off_t offset,
    uv_fs_cb cb) {
  uv_fs_req_init(loop, req, UV_FS_FTRUNCATE, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_ftruncate(file, offset, EIO_PRI_DEFAULT, uv__fs_after, req);
    if (!req->eio) {
      uv_err_new(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = ftruncate(file, offset);

    if (req->result) {
      uv_err_new(loop, errno);
      return -1;
    }
  }

  return 0;
}


int uv_fs_sendfile(uv_loop_t* loop, uv_fs_t* req, uv_file out_fd, uv_file in_fd,
    off_t in_offset, size_t length, uv_fs_cb cb) {
  uv_fs_req_init(loop, req, UV_FS_SENDFILE, cb);

  if (cb) {
    /* async */
    uv_ref(loop);
    req->eio = eio_sendfile(out_fd, in_fd, in_offset, length, EIO_PRI_DEFAULT,
        uv__fs_after, req);
    if (!req->eio) {
      uv_err_new(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = eio_sendfile_sync(out_fd, in_fd, in_offset, length);

    if (req->result) {
      uv_err_new(loop, errno);
      return -1;
    }
  }

  return 0;
}


int uv_fs_chmod(uv_loop_t* loop, uv_fs_t* req, const char* path, int mode,
    uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_utime(uv_loop_t* loop, uv_fs_t* req, const char* path, double atime,
    double mtime, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_futime(uv_loop_t* loop, uv_fs_t* req, uv_file file, double atime,
    double mtime, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_lstat(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
  char* pathdup = path;
  int pathlen;

  uv_fs_req_init(loop, req, UV_FS_LSTAT, cb);

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
      uv_err_new(loop, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = lstat(pathdup, &req->statbuf);

    free(pathdup);

    if (req->result < 0) {
      uv_err_new(loop, errno);
      return -1;
    }

    req->ptr = &req->statbuf;
  }

  return 0;
}


int uv_fs_link(uv_loop_t* loop, uv_fs_t* req, const char* path,
    const char* new_path, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_symlink(uv_loop_t* loop, uv_fs_t* req, const char* path,
    const char* new_path, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_readlink(uv_loop_t* loop, uv_fs_t* req, const char* path,
    uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_fchmod(uv_loop_t* loop, uv_fs_t* req, uv_file file, int mode,
    uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_chown(uv_loop_t* loop, uv_fs_t* req, const char* path, int uid,
    int gid, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_fchown(uv_loop_t* loop, uv_fs_t* req, uv_file file, int uid, int gid,
    uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
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
    uv_err_new(loop, ENOMEM);
    return -1;
  }

  return 0;
}
