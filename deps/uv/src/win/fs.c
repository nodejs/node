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

#include <assert.h>
#include <malloc.h>
#include <direct.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/utime.h>
#include <stdio.h>

#include "uv.h"
#include "internal.h"

#define UV_FS_ASYNC_QUEUED       0x0001
#define UV_FS_FREE_ARG0          0x0002
#define UV_FS_FREE_ARG1          0x0004
#define UV_FS_FREE_PTR           0x0008
#define UV_FS_CLEANEDUP          0x0010

#define STRDUP_ARG(req, i)                                                  \
  req->arg##i = (void*)strdup((const char*)req->arg##i);                    \
  if (!req->arg##i) {                                                       \
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");                            \
  }                                                                         \
  req->flags |= UV_FS_FREE_ARG##i;

#define WRAP_REQ_ARGS1(req, a0)                                             \
  req->arg0 = (void*)a0;

#define WRAP_REQ_ARGS2(req, a0, a1)                                         \
  WRAP_REQ_ARGS1(req, a0)                                                   \
  req->arg1 = (void*)a1;

#define WRAP_REQ_ARGS3(req, a0, a1, a2)                                     \
  WRAP_REQ_ARGS2(req, a0, a1)                                               \
  req->arg2 = (void*)a2;

#define WRAP_REQ_ARGS4(req, a0, a1, a2, a3)                                 \
  WRAP_REQ_ARGS3(req, a0, a1, a2)                                           \
  req->arg3 = (void*)a3;

#define QUEUE_FS_TP_JOB(req)                                                \
  if (!QueueUserWorkItem(&uv_fs_thread_proc, req, WT_EXECUTELONGFUNCTION)) {\
    uv_set_sys_error(GetLastError());                                       \
    return -1;                                                              \
  }                                                                         \
  req->flags |= UV_FS_ASYNC_QUEUED;                                         \
  uv_ref();


void uv_fs_init() {
  _fmode = _O_BINARY;
}


static void uv_fs_req_init_async(uv_fs_t* req, uv_fs_type fs_type, uv_fs_cb cb) {
  uv_req_init((uv_req_t*) req);
  req->type = UV_FS;
  req->flags = 0;
  req->fs_type = fs_type;
  req->cb = cb;
  req->result = 0;
  req->ptr = NULL;
  req->errorno = 0;
  memset(&req->overlapped, 0, sizeof(req->overlapped));
}


static void uv_fs_req_init_sync(uv_fs_t* req, uv_fs_type fs_type) {
  uv_req_init((uv_req_t*) req);
  req->type = UV_FS;
  req->flags = 0;
  req->fs_type = fs_type;
  req->result = 0;
  req->ptr = NULL;
  req->errorno = 0;
}


void fs__open(uv_fs_t* req, const char* path, int flags, int mode) {
  int result = _open(path, flags, mode);
  SET_REQ_RESULT(req, result);
}


void fs__close(uv_fs_t* req, uv_file file) {
  int result = _close(file);
  SET_REQ_RESULT(req, result);
}


void fs__read(uv_fs_t* req, uv_file file, void *buf, size_t length, off_t offset) {
  int result = 0;

  if (offset != -1) {
    result = _lseek(file, offset, SEEK_SET);
  }

  if (result != -1) {
    result = _read(file, buf, length);
  }

  SET_REQ_RESULT(req, result);
}


void fs__write(uv_fs_t* req, uv_file file, void *buf, size_t length, off_t offset) {
  int result = 0;

  if (offset != -1) {
    result = _lseek(file, offset, SEEK_SET);
  }

  if (result != -1) {
    result = _write(file, buf, length);
  }

  SET_REQ_RESULT(req, result);
}


void fs__unlink(uv_fs_t* req, const char* path) {
  int result = _unlink(path);
  SET_REQ_RESULT(req, result);
}


void fs__mkdir(uv_fs_t* req, const char* path, int mode) {
  int result = _mkdir(path);
  SET_REQ_RESULT(req, result);
}


void fs__rmdir(uv_fs_t* req, const char* path) {
  int result = _rmdir(path);
  SET_REQ_RESULT(req, result);
}


void fs__readdir(uv_fs_t* req, const char* path, int flags) {
  int result;
  char* buf, *ptr, *name;
  HANDLE dir;
  WIN32_FIND_DATAA ent = {0};
  size_t len = strlen(path);
  size_t buf_size = 4096;
  const char* fmt = !len                                            ? "./*"
                  : (path[len - 1] == '/' || path[len - 1] == '\\') ? "%s*"
                  :                                                   "%s\\*";

  char* path2 = (char*)malloc(len + 4);
  if (!path2) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  sprintf(path2, fmt, path);
  dir = FindFirstFileA(path2, &ent);
  free(path2);

  if(dir == INVALID_HANDLE_VALUE) {
    result = -1;
    goto done;
  }

  buf = (char*)malloc(buf_size);
  if (!buf) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  ptr = buf;
  result = 0;

  do {
    name = ent.cFileName;

    if (name[0] != '.' || (name[1] && (name[1] != '.' || name[2]))) {
      len = strlen(name);

      while ((ptr - buf) + len + 1 > buf_size) {
        buf_size *= 2;
        path2 = buf;
        buf = (char*)realloc(buf, buf_size);
        if (!buf) {
          uv_fatal_error(ERROR_OUTOFMEMORY, "realloc");
        }

        ptr = buf + (ptr - path2);
      }

      strcpy(ptr, name);
      ptr += len + 1;
      result++;
    }
  } while(FindNextFileA(dir, &ent));

  FindClose(dir);

  req->ptr = buf;
  req->flags |= UV_FS_FREE_PTR;

done:
  SET_REQ_RESULT(req, result);
}


void fs__stat(uv_fs_t* req, const char* path) {
  int result;

  req->ptr = malloc(sizeof(struct _stat));
  if (!req->ptr) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  result = _stat(path, (struct _stat*)req->ptr);
  if (result == -1) {
    free(req->ptr);
    req->ptr = NULL;
  } else {
    req->flags |= UV_FS_FREE_PTR;
  }

  SET_REQ_RESULT(req, result);
}


void fs__fstat(uv_fs_t* req, uv_file file) {
  int result;

  req->ptr = malloc(sizeof(struct _stat));
  if (!req->ptr) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  result = _fstat(file, (struct _stat*)req->ptr);
  if (result == -1) {
    free(req->ptr);
    req->ptr = NULL;
  } else {
    req->flags |= UV_FS_FREE_PTR;
  }

  SET_REQ_RESULT(req, result);
}


void fs__rename(uv_fs_t* req, const char* path, const char* new_path) {
  int result = rename(path, new_path);
  SET_REQ_RESULT(req, result);
}


void fs__fsync(uv_fs_t* req, uv_file file) {
  int result = FlushFileBuffers((HANDLE)_get_osfhandle(file)) ? 0 : -1;
  SET_REQ_RESULT(req, result);
}


void fs__ftruncate(uv_fs_t* req, uv_file file, off_t offset) {
  int result = _chsize(file, offset);
  SET_REQ_RESULT(req, result);
}


void fs__sendfile(uv_fs_t* req, uv_file out_file, uv_file in_file, off_t in_offset, size_t length) {
  const size_t max_buf_size = 65536;
  size_t buf_size = length < max_buf_size ? length : max_buf_size;
  int n, result = 0;
  char* buf = (char*)malloc(buf_size);
  if (!buf) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  if (in_offset != -1) {
    result = _lseek(in_file, in_offset, SEEK_SET);
  }
  
  if (result != -1) {
    while (length > 0) {
      n = _read(in_file, buf, length < buf_size ? length : buf_size);
      if (n == 0) {
        break;
      } else if (n == -1) {
        result = -1;
        break;
      }

      length -= n;

      n = _write(out_file, buf, n);
      if (n == -1) {
        result = -1;
        break;
      }

      result += n;
    }
  }

  SET_REQ_RESULT(req, result);
}


void fs__chmod(uv_fs_t* req, const char* path, int mode) {
  int result = _chmod(path, mode);
  SET_REQ_RESULT(req, result);
}


void fs__utime(uv_fs_t* req, const char* path, double atime, double mtime) {
  int result;
  struct _utimbuf b = {(time_t)atime, (time_t)mtime};
  result = _utime(path, &b);
  SET_REQ_RESULT(req, result);
}


void fs__futime(uv_fs_t* req, uv_file file, double atime, double mtime) {
  int result;
  struct _utimbuf b = {(time_t)atime, (time_t)mtime};
  result = _futime(file, &b);
  SET_REQ_RESULT(req, result);
}


static DWORD WINAPI uv_fs_thread_proc(void* parameter) {
  uv_fs_t* req = (uv_fs_t*)parameter;

  assert(req != NULL);
  assert(req->type == UV_FS);

  switch (req->fs_type) {
    case UV_FS_OPEN:
      fs__open(req, (const char*)req->arg0, (int)req->arg1, (int)req->arg2);
      break;
    case UV_FS_CLOSE:
      fs__close(req, (uv_file)req->arg0);
      break;
    case UV_FS_READ:
      fs__read(req, (uv_file)req->arg0, req->arg1, (size_t)req->arg2, (off_t)req->arg3);
      break;
    case UV_FS_WRITE:
      fs__write(req, (uv_file)req->arg0, req->arg1, (size_t)req->arg2, (off_t)req->arg3);
      break;
    case UV_FS_UNLINK:
      fs__unlink(req, (const char*)req->arg0);
      break;
    case UV_FS_MKDIR:
      fs__mkdir(req, (const char*)req->arg0, (int)req->arg1);
      break;
    case UV_FS_RMDIR:
      fs__rmdir(req, (const char*)req->arg0);
      break;
    case UV_FS_READDIR:
      fs__readdir(req, (const char*)req->arg0, (int)req->arg1);
      break;
    case UV_FS_STAT:
      fs__stat(req, (const char*)req->arg0);
      break;
    case UV_FS_FSTAT:
      fs__fstat(req, (uv_file)req->arg0);
      break;
    case UV_FS_RENAME:
      fs__rename(req, (const char*)req->arg0, (const char*)req->arg1);
      break;
    case UV_FS_FSYNC:
    case UV_FS_FDATASYNC:
      fs__fsync(req, (uv_file)req->arg0);
      break;
    case UV_FS_FTRUNCATE:
      fs__ftruncate(req, (uv_file)req->arg0, (off_t)req->arg1);
      break;
    case UV_FS_SENDFILE:
      fs__sendfile(req, (uv_file)req->arg0, (uv_file)req->arg1, (off_t)req->arg2, (size_t)req->arg3);
      break;
    case UV_FS_CHMOD:
      fs__chmod(req, (const char*)req->arg0, (int)req->arg1);
      break;
    case UV_FS_UTIME:
      fs__utime(req, (const char*)req->arg0, req->arg4, req->arg5);
      break;
    case UV_FS_FUTIME:
      fs__futime(req, (uv_file)req->arg0, req->arg4, req->arg5);
      break;
    default:
      assert(!"bad uv_fs_type");
  }

  POST_COMPLETION_FOR_REQ(req);

  return 0;
}


int uv_fs_open(uv_fs_t* req, const char* path, int flags, int mode, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_OPEN, cb);
    WRAP_REQ_ARGS3(req, path, flags, mode);
    STRDUP_ARG(req, 0);
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_OPEN);
    fs__open(req, path, flags, mode);
  }

  return 0;
}


int uv_fs_close(uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_CLOSE, cb);
    WRAP_REQ_ARGS1(req, file);
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_CLOSE);
    fs__close(req, file);
  }

  return 0;
}


int uv_fs_read(uv_fs_t* req, uv_file file, void* buf, size_t length, off_t offset, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_READ, cb);
    WRAP_REQ_ARGS4(req, file, buf, length, offset);
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_READ);
    fs__read(req, file, buf, length, offset);
  }

  return 0;
}


int uv_fs_write(uv_fs_t* req, uv_file file, void* buf, size_t length, off_t offset, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_WRITE, cb);
    WRAP_REQ_ARGS4(req, file, buf, length, offset);
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_WRITE);
    fs__write(req, file, buf, length, offset);
  }

  return 0;
}


int uv_fs_unlink(uv_fs_t* req, const char* path, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_UNLINK, cb);
    WRAP_REQ_ARGS1(req, path);
    STRDUP_ARG(req, 0);
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_UNLINK);
    fs__unlink(req, path);
  }

  return 0;
}


int uv_fs_mkdir(uv_fs_t* req, const char* path, int mode, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_MKDIR, cb);
    WRAP_REQ_ARGS2(req, path, mode);
    STRDUP_ARG(req, 0);
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_MKDIR);
    fs__mkdir(req, path, mode);
  }

  return 0;
}


int uv_fs_rmdir(uv_fs_t* req, const char* path, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_RMDIR, cb);
    WRAP_REQ_ARGS1(req, path);
    STRDUP_ARG(req, 0);
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_RMDIR);
    fs__rmdir(req, path);
  }

  return 0;
}


int uv_fs_readdir(uv_fs_t* req, const char* path, int flags, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_READDIR, cb);
    WRAP_REQ_ARGS2(req, path, flags);
    STRDUP_ARG(req, 0);
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_READDIR);
    fs__readdir(req, path, flags);
  }

  return 0;
}


int uv_fs_stat(uv_fs_t* req, const char* path, uv_fs_cb cb) {
  int len = strlen(path);
  char* path2 = NULL;
  int has_backslash = (path[len - 1] == '\\' || path[len - 1] == '/');

  if (path[len - 1] == '\\' || path[len - 1] == '/') {
    path2 = strdup(path);
    if (!path2) {
      uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
    }

    path2[len - 1] = '\0';
  }

  if (cb) {
    uv_fs_req_init_async(req, UV_FS_STAT, cb);
    if (path2) {
      WRAP_REQ_ARGS1(req, path2);
      req->flags |= UV_FS_FREE_ARG0;
    } else {
      WRAP_REQ_ARGS1(req, path);
      STRDUP_ARG(req, 0);
    }
    
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_STAT);
    fs__stat(req, path2 ? path2 : path);
    if (path2) {
      free(path2);
    }
  }

  return 0;
}


int uv_fs_fstat(uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_FSTAT, cb);
    WRAP_REQ_ARGS1(req, file);
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_FSTAT);
    fs__fstat(req, file);
  }

  return 0;
}


int uv_fs_rename(uv_fs_t* req, const char* path, const char* new_path, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_RENAME, cb);
    WRAP_REQ_ARGS2(req, path, new_path);
    STRDUP_ARG(req, 0);
    STRDUP_ARG(req, 1);
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_RENAME);
    fs__rename(req, path, new_path);
  }

  return 0;
}


int uv_fs_fdatasync(uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_FDATASYNC, cb);
    WRAP_REQ_ARGS1(req, file);
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_FDATASYNC);
    fs__fsync(req, file);
  }

  return 0;
}


int uv_fs_fsync(uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_FSYNC, cb);
    WRAP_REQ_ARGS1(req, file);
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_FSYNC);
    fs__fsync(req, file);
  }

  return 0;
}


int uv_fs_ftruncate(uv_fs_t* req, uv_file file, off_t offset, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_FTRUNCATE, cb);
    WRAP_REQ_ARGS2(req, file, offset);
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_FTRUNCATE);
    fs__ftruncate(req, file, offset);
  }

  return 0;
}


int uv_fs_sendfile(uv_fs_t* req, uv_file out_fd, uv_file in_fd, off_t in_offset, size_t length, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_SENDFILE, cb);
    WRAP_REQ_ARGS4(req, out_fd, in_fd, in_offset, length);
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_SENDFILE);
    fs__sendfile(req, out_fd, in_fd, in_offset, length);
  }

  return 0;
}


int uv_fs_chmod(uv_fs_t* req, const char* path, int mode, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_CHMOD, cb);
    WRAP_REQ_ARGS2(req, path, mode);
    STRDUP_ARG(req, 0);
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_CHMOD);
    fs__chmod(req, path, mode);
  }

  return 0;
}


int uv_fs_utime(uv_fs_t* req, const char* path, double atime, double mtime, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_UTIME, cb);
    WRAP_REQ_ARGS1(req, path);
    STRDUP_ARG(req, 0);
    req->arg4 = (ssize_t)atime;
    req->arg5 = (ssize_t)mtime;
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_UTIME);
    fs__utime(req, path, atime, mtime);
  }

  return 0;
}


int uv_fs_futime(uv_fs_t* req, uv_file file, double atime, double mtime, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(req, UV_FS_FUTIME, cb);
    WRAP_REQ_ARGS1(req, file);
    req->arg4 = (ssize_t)atime;
    req->arg5 = (ssize_t)mtime;
    QUEUE_FS_TP_JOB(req);
  } else {
    uv_fs_req_init_sync(req, UV_FS_FUTIME);
    fs__futime(req, file, atime, mtime);
  }

  return 0;
}


void uv_process_fs_req(uv_fs_t* req) {
  assert(req->cb);
  req->cb(req);
}


void uv_fs_req_cleanup(uv_fs_t* req) {
  if (req->flags & UV_FS_CLEANEDUP) {
    return;
  }

  if (req->flags & UV_FS_FREE_ARG0 && req->arg0) {
    free(req->arg0);
    req->arg0 = NULL;
  }

  if (req->flags & UV_FS_FREE_ARG1 && req->arg1) {
    free(req->arg1);
    req->arg1 = NULL;
  }

  if (req->flags & UV_FS_FREE_PTR && req->ptr) {
    free(req->ptr);
    req->ptr = NULL;
  }

  if (req->flags & UV_FS_ASYNC_QUEUED) {
    uv_unref();
  }

  req->flags |= UV_FS_CLEANEDUP;
}
