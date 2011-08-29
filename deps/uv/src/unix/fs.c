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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>


static void uv_fs_req_init(uv_fs_t* req, uv_fs_type fs_type, uv_fs_cb cb) {
  uv__req_init((uv_req_t*) req);
  req->type = UV_FS;
  req->fs_type = fs_type;
  req->cb = cb;
  req->result = 0;
  req->ptr = NULL;
  req->errorno = 0;
  req->eio = NULL;
}


void uv_fs_req_cleanup(uv_fs_t* req) {
  assert(0 && "implement me");
}


static int uv__fs_after(eio_req* eio) {
  uv_fs_t* req = eio->data;
  assert(req->cb);

  req->result = req->eio->result;
  req->eio = NULL; /* Freed by libeio */

  req->cb(req);
  return 0;
}


int uv_fs_close(uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  uv_fs_req_init(req, UV_FS_CLOSE, cb);
  if (cb) {
    /* async */
    req->eio = eio_close(file, EIO_PRI_DEFAULT, uv__fs_after, req);
    if (!req->eio) {
      uv_err_new(NULL, ENOMEM);
      return -1;
    }
  } else {
    /* sync */
    if ((req->result = uv__close(file))) {
      uv_err_new(NULL, errno);
      return -1;
    }
  }

  return 0;
}


int uv_fs_open(uv_fs_t* req, const char* path, int flags, int mode,
    uv_fs_cb cb) {
  uv_fs_req_init(req, UV_FS_OPEN, cb);

  if (cb) {
    /* async */
    req->eio = eio_open(path, flags, mode, EIO_PRI_DEFAULT, uv__fs_after, req);
    if (!req->eio) {
      uv_err_new(NULL, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = open(path, flags, mode);
    if (req->result < 0) {
      uv_err_new(NULL, errno);
      return -1;
    }

    uv__cloexec(req->result, 1);
  }

  return 0;
}


int uv_fs_read(uv_fs_t* req, uv_file fd, void* buf, size_t length,
    off_t offset, uv_fs_cb cb) {
  uv_fs_req_init(req, UV_FS_READ, cb);

  if (cb) {
    /* async */
    req->eio = eio_read(fd, buf, length, offset, EIO_PRI_DEFAULT,
        uv__fs_after, req);

    if (!req->eio) {
      uv_err_new(NULL, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = offset < 0 ?
      read(fd, buf, length) :
      pread(fd, buf, length, offset);

    if (req->result < 0) {
      uv_err_new(NULL, errno);
      return -1;
    }
  }

  return 0;
}


int uv_fs_unlink(uv_fs_t* req, const char* path, uv_fs_cb cb) {
  uv_fs_req_init(req, UV_FS_UNLINK, cb);

  if (cb) {
    /* async */
    req->eio = eio_unlink(path, EIO_PRI_DEFAULT, uv__fs_after, req);
    if (!req->eio) {
      uv_err_new(NULL, ENOMEM);
      return -1;
    }

  } else {
    /* sync */
    req->result = unlink(path);

    if (req->result) {
      uv_err_new(NULL, errno);
      return -1;
    }
  }

  return 0;
}


int uv_fs_write(uv_fs_t* req, uv_file file, void* buf, size_t length, off_t offset, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_mkdir(uv_fs_t* req, const char* path, int mode, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_rmdir(uv_fs_t* req, const char* path, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_readdir(uv_fs_t* req, const char* path, int flags, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_stat(uv_fs_t* req, const char* path, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_fstat(uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_rename(uv_fs_t* req, const char* path, const char* new_path, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_fsync(uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_fdatasync(uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_ftruncate(uv_fs_t* req, uv_file file, off_t offset, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_sendfile(uv_fs_t* req, uv_file out_fd, uv_file in_fd, off_t in_offset, size_t length, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_chmod(uv_fs_t* req, const char* path, int mode, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_utime(uv_fs_t* req, const char* path, double atime, double mtime, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_futime(uv_fs_t* req, uv_file file, double atime, double mtime, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_lstat(uv_fs_t* req, const char* path, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_link(uv_fs_t* req, const char* path, const char* new_path, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_symlink(uv_fs_t* req, const char* path, const char* new_path, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_readlink(uv_fs_t* req, const char* path, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_fchmod(uv_fs_t* req, uv_file file, int mode, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_chown(uv_fs_t* req, const char* path, int uid, int gid, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_fs_fchown(uv_fs_t* req, uv_file file, int uid, int gid, uv_fs_cb cb) {
  assert(0 && "implement me");
  return -1;
}


int uv_queue_work(uv_work_t* req, uv_work_cb work_cb,
    uv_after_work_cb after_work_cb) {
  assert(0 && "implement me");
  return -1;
}
