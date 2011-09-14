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
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <limits.h>
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
#define UV_FS_LAST_ERROR_SET     0x0020

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

#define QUEUE_FS_TP_JOB(loop, req)                                          \
  if (!QueueUserWorkItem(&uv_fs_thread_proc,                                \
                         req,                                               \
                         WT_EXECUTELONGFUNCTION)) {                         \
    uv_set_sys_error((loop), GetLastError());                               \
    return -1;                                                              \
  }                                                                         \
  req->flags |= UV_FS_ASYNC_QUEUED;                                         \
  uv_ref((loop));


#define SET_UV_LAST_ERROR_FROM_REQ(req)                                     \
  if (req->flags & UV_FS_LAST_ERROR_SET) {                                  \
    uv_set_sys_error(req->loop, req->last_error);                           \
  }

#define SET_REQ_LAST_ERROR(req, error)                                      \
  req->last_error = error;                                                  \
  req->flags |= UV_FS_LAST_ERROR_SET;

#define SET_REQ_RESULT(req, result_value)                                   \
  req->result = (result_value);                                             \
  if (req->result == -1) {                                                  \
    req->errorno = uv_translate_sys_error(_doserrno);                       \
  }

#define SET_REQ_RESULT_WIN32_ERROR(req, sys_errno)                          \
  req->result = -1;                                                         \
  req->errorno = uv_translate_sys_error(sys_errno);                         \
  SET_REQ_LAST_ERROR(req, sys_errno);


void uv_fs_init() {
  _fmode = _O_BINARY;
}


static void uv_fs_req_init_async(uv_loop_t* loop, uv_fs_t* req,
    uv_fs_type fs_type, const char* path, uv_fs_cb cb) {
  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_FS;
  req->loop = loop;
  req->flags = 0;
  req->fs_type = fs_type;
  req->cb = cb;
  req->result = 0;
  req->ptr = NULL;
  req->path = path ? strdup(path) : NULL;
  req->errorno = 0;
  req->last_error = 0;
  memset(&req->overlapped, 0, sizeof(req->overlapped));
}


static void uv_fs_req_init_sync(uv_loop_t* loop, uv_fs_t* req,
    uv_fs_type fs_type) {
  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_FS;
  req->loop = loop;
  req->flags = 0;
  req->fs_type = fs_type;
  req->result = 0;
  req->ptr = NULL;
  req->path = NULL;
  req->errorno = 0;
}


void fs__open(uv_fs_t* req, const char* path, int flags, int mode) {
  DWORD access;
  DWORD share;
  DWORD disposition;
  DWORD attributes;
  HANDLE file;
  int result, current_umask;

  /* Obtain the active umask. umask() never fails and returns the previous */
  /* umask. */
  current_umask = umask(0);
  umask(current_umask);

  /* convert flags and mode to CreateFile parameters */
  switch (flags & (_O_RDONLY | _O_WRONLY | _O_RDWR)) {
  case _O_RDONLY:
    access = GENERIC_READ;
    break;
  case _O_WRONLY:
    access = GENERIC_WRITE;
    break;
  case _O_RDWR:
    access = GENERIC_READ | GENERIC_WRITE;
    break;
  default:
    result  = -1;
    goto end;
  }

  /*
   * Here is where we deviate significantly from what CRT's _open()
   * does. We indiscriminately use all the sharing modes, to match
   * UNIX semantics. In particular, this ensures that the file can
   * be deleted even whilst it's open, fixing issue #1449.
   */
  share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

  switch (flags & (_O_CREAT | _O_EXCL | _O_TRUNC)) {
  case 0:
  case _O_EXCL:
    disposition = OPEN_EXISTING;
    break;
  case _O_CREAT:
    disposition = OPEN_ALWAYS;
    break;
  case _O_CREAT | _O_EXCL:
  case _O_CREAT | _O_TRUNC | _O_EXCL:
    disposition = CREATE_NEW;
    break;
  case _O_TRUNC:
  case _O_TRUNC | _O_EXCL:
    disposition = TRUNCATE_EXISTING;
    break;
  case _O_CREAT | _O_TRUNC:
    disposition = CREATE_ALWAYS;
    break;
  default:
    result = -1;
    goto end;
  }

  attributes = FILE_ATTRIBUTE_NORMAL;
  if (flags & _O_CREAT) {
    if (!((mode & ~current_umask) & _S_IWRITE)) {
      attributes |= FILE_ATTRIBUTE_READONLY;
    }
  }

  if (flags & _O_TEMPORARY ) {
    attributes |= FILE_FLAG_DELETE_ON_CLOSE | FILE_ATTRIBUTE_TEMPORARY;
    access |= DELETE;
  }

  if (flags & _O_SHORT_LIVED) {
    attributes |= FILE_ATTRIBUTE_TEMPORARY;
  }

  switch (flags & (_O_SEQUENTIAL | _O_RANDOM)) {
  case 0:
    break;
  case _O_SEQUENTIAL:
    attributes |= FILE_FLAG_SEQUENTIAL_SCAN;
    break;
  case _O_RANDOM:
    attributes |= FILE_FLAG_RANDOM_ACCESS;
    break;
  default:
    result = -1;
    goto end;
  }

  file = CreateFileA(path,
                     access,
                     share,
                     NULL,
                     disposition,
                     attributes,
                     NULL);
  if (file == INVALID_HANDLE_VALUE) {
    SET_REQ_RESULT_WIN32_ERROR(req, GetLastError());
    return;
  }
  result = _open_osfhandle((intptr_t)file, flags);
end:
  SET_REQ_RESULT(req, result);
}

void fs__close(uv_fs_t* req, uv_file file) {
  int result = _close(file);
  SET_REQ_RESULT(req, result);
}


void fs__read(uv_fs_t* req, uv_file file, void *buf, size_t length,
    off_t offset) {
  HANDLE handle;
  OVERLAPPED overlapped, *overlapped_ptr;
  LARGE_INTEGER offset_;
  DWORD bytes;

  handle = (HANDLE) _get_osfhandle(file);
  if (handle == INVALID_HANDLE_VALUE) {
    SET_REQ_RESULT(req, -1);
    return;
  }

  if (length > INT_MAX) {
    SET_REQ_ERROR(req, ERROR_INSUFFICIENT_BUFFER);
    return;
  }

  if (offset != -1) {
    memset(&overlapped, 0, sizeof overlapped);

    offset_.QuadPart = offset;
    overlapped.Offset = offset_.LowPart;
    overlapped.OffsetHigh = offset_.HighPart;

    overlapped_ptr = &overlapped;
  } else {
    overlapped_ptr = NULL;
  }

  if (ReadFile(handle, buf, length, &bytes, overlapped_ptr)) {
    SET_REQ_RESULT(req, bytes);
  } else {
    SET_REQ_ERROR(req, GetLastError());
  }
}


void fs__write(uv_fs_t* req, uv_file file, void *buf, size_t length,
    off_t offset) {
  HANDLE handle;
  OVERLAPPED overlapped, *overlapped_ptr;
  LARGE_INTEGER offset_;
  DWORD bytes;

  handle = (HANDLE) _get_osfhandle(file);
  if (handle == INVALID_HANDLE_VALUE) {
    SET_REQ_RESULT(req, -1);
    return;
  }

  if (length > INT_MAX) {
    SET_REQ_ERROR(req, ERROR_INSUFFICIENT_BUFFER);
    return;
  }

  if (offset != -1) {
    memset(&overlapped, 0, sizeof overlapped);

    offset_.QuadPart = offset;
    overlapped.Offset = offset_.LowPart;
    overlapped.OffsetHigh = offset_.HighPart;

    overlapped_ptr = &overlapped;
  } else {
    overlapped_ptr = NULL;
  }

  if (WriteFile(handle, buf, length, &bytes, overlapped_ptr)) {
    SET_REQ_RESULT(req, bytes);
  } else {
    SET_REQ_ERROR(req, GetLastError());
  }
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
    SET_REQ_RESULT_WIN32_ERROR(req, GetLastError());
    return;
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

  SET_REQ_RESULT(req, result);
}


void fs__stat(uv_fs_t* req, const char* path) {
  int result;

  result = _stati64(path, &req->stat);
  if (result == -1) {
    req->ptr = NULL;
  } else {
    req->ptr = &req->stat;
  }

  SET_REQ_RESULT(req, result);
}


void fs__fstat(uv_fs_t* req, uv_file file) {
  int result;

  result = _fstati64(file, &req->stat);
  if (result == -1) {
    req->ptr = NULL;
  } else {
    req->ptr = &req->stat;
  }

  SET_REQ_RESULT(req, result);
}


void fs__rename(uv_fs_t* req, const char* path, const char* new_path) {
  int result = rename(path, new_path);
  SET_REQ_RESULT(req, result);
}


void fs__fsync(uv_fs_t* req, uv_file file) {
  int result = FlushFileBuffers((HANDLE)_get_osfhandle(file)) ? 0 : -1;
  if (result == -1) {
    SET_REQ_RESULT_WIN32_ERROR(req, GetLastError());
  } else {
    SET_REQ_RESULT(req, result);
  }
}


void fs__ftruncate(uv_fs_t* req, uv_file file, off_t offset) {
  int result = _chsize(file, offset);
  SET_REQ_RESULT(req, result);
}


void fs__sendfile(uv_fs_t* req, uv_file out_file, uv_file in_file,
    off_t in_offset, size_t length) {
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


void fs__fchmod(uv_fs_t* req, uv_file file, int mode) {
  int result;
  HANDLE handle;
  NTSTATUS nt_status;
  IO_STATUS_BLOCK io_status;
  FILE_BASIC_INFORMATION file_info;

  handle = (HANDLE)_get_osfhandle(file);

  nt_status = pNtQueryInformationFile(handle,
                                      &io_status,
                                      &file_info,
                                      sizeof file_info,
                                      FileBasicInformation);

  if (nt_status != STATUS_SUCCESS) {
    result = -1;
    goto done;
  }

  if (mode & _S_IWRITE) {
    file_info.FileAttributes &= ~FILE_ATTRIBUTE_READONLY;
  } else {
    file_info.FileAttributes |= FILE_ATTRIBUTE_READONLY;
  }

  nt_status = pNtSetInformationFile(handle,
                                    &io_status,
                                    &file_info,
                                    sizeof file_info,
                                    FileBasicInformation);

  if (nt_status != STATUS_SUCCESS) {
    result = -1;
    goto done;
  }

  result = 0;

done:
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


void fs__link(uv_fs_t* req, const char* path, const char* new_path) {
  int result = CreateHardLinkA(new_path, path, NULL) ? 0 : -1;
  if (result == -1) {
    SET_REQ_RESULT_WIN32_ERROR(req, GetLastError());
  } else {
    SET_REQ_RESULT(req, result);
  }
}


void fs__symlink(uv_fs_t* req, const char* path, const char* new_path,
                 int flags) {
  int result;
  if (pCreateSymbolicLinkA) {
    result = pCreateSymbolicLinkA(new_path,
                                  path,
                                  flags & UV_FS_SYMLINK_DIR ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0) ? 0 : -1;
    if (result == -1) {
      SET_REQ_LAST_ERROR(req, GetLastError());
    }
  } else {
    result = -1;
    errno = ENOSYS;
  }
  
  SET_REQ_RESULT(req, result);
}


void fs__readlink(uv_fs_t* req, const char* path) {
  int result = -1;
  BOOL rv;
  HANDLE symlink;
  void* buffer = NULL;
  DWORD bytes_returned;
  REPARSE_DATA_BUFFER* reparse_data;
  int utf8size;
  wchar_t* substitute_name;
  int substitute_name_length;

  symlink = CreateFileA(path,
                        0,
                        0,
                        NULL,
                        OPEN_EXISTING,
                        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
                        NULL);

  if (INVALID_HANDLE_VALUE == symlink) {
    result = -1;
    SET_REQ_LAST_ERROR(req, GetLastError());
    goto done;
  }

  buffer = malloc(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
  if (!buffer) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  rv = DeviceIoControl(symlink,
                       FSCTL_GET_REPARSE_POINT,
                       NULL,
                       0,
                       buffer, 
                       MAXIMUM_REPARSE_DATA_BUFFER_SIZE,
                       &bytes_returned,
                       NULL);

  if (!rv) {
    result = -1;
    SET_REQ_LAST_ERROR(req, GetLastError());
    goto done;
  }

  reparse_data = buffer;
  if (reparse_data->ReparseTag != IO_REPARSE_TAG_SYMLINK) {
    result = -1;
    /* something is seriously wrong */
    SET_REQ_LAST_ERROR(req, GetLastError());
    goto done;
  }

  substitute_name = reparse_data->SymbolicLinkReparseBuffer.PathBuffer + (reparse_data->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(wchar_t));
  substitute_name_length = reparse_data->SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(wchar_t);

  /* Strip off the leading \??\ from the substitute name buffer.*/
  if (memcmp(substitute_name, L"\\??\\", 8) == 0) {
    substitute_name += 4;
    substitute_name_length -= 4;
  }

  utf8size = uv_utf16_to_utf8(substitute_name,
                              substitute_name_length,
                              NULL,
                              0);
  if (!utf8size) {
    result = -1;
    SET_REQ_LAST_ERROR(req, GetLastError());
    goto done;
  }

  req->ptr = malloc(utf8size + 1);
  if (!req->ptr) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  utf8size = uv_utf16_to_utf8(substitute_name,
                              substitute_name_length,
                              req->ptr,
                              utf8size);
  if (!utf8size) {
    result = -1;
    SET_REQ_LAST_ERROR(req, GetLastError());
    goto done;
  }

  req->flags |= UV_FS_FREE_PTR;
  ((char*)req->ptr)[utf8size] = '\0';
  result = 0;

done:
  if (buffer) {
    free(buffer);
  }

  if (symlink != INVALID_HANDLE_VALUE) {
    CloseHandle(symlink);
  }

  SET_REQ_RESULT(req, result);
}


void fs__nop(uv_fs_t* req) {
  req->result = 0;
}


static DWORD WINAPI uv_fs_thread_proc(void* parameter) {
  uv_fs_t* req = (uv_fs_t*) parameter;
  uv_loop_t* loop = req->loop;

  assert(req != NULL);
  assert(req->type == UV_FS);

  switch (req->fs_type) {
    case UV_FS_OPEN:
      fs__open(req, req->path, (int)req->arg0, (int)req->arg1);
      break;
    case UV_FS_CLOSE:
      fs__close(req, (uv_file)req->arg0);
      break;
    case UV_FS_READ:
      fs__read(req,
               (uv_file) req->arg0,
               req->arg1,
               (size_t) req->arg2,
               (off_t) req->arg3);
      break;
    case UV_FS_WRITE:
      fs__write(req,
                (uv_file)req->arg0,
                req->arg1,
                (size_t) req->arg2,
                (off_t) req->arg3);
      break;
    case UV_FS_UNLINK:
      fs__unlink(req, req->path);
      break;
    case UV_FS_MKDIR:
      fs__mkdir(req, req->path, (int)req->arg0);
      break;
    case UV_FS_RMDIR:
      fs__rmdir(req, req->path);
      break;
    case UV_FS_READDIR:
      fs__readdir(req, req->path, (int)req->arg0);
      break;
    case UV_FS_STAT:
    case UV_FS_LSTAT:
      fs__stat(req, req->path);
      break;
    case UV_FS_FSTAT:
      fs__fstat(req, (uv_file)req->arg0);
      break;
    case UV_FS_RENAME:
      fs__rename(req, req->path, (const char*)req->arg0);
      break;
    case UV_FS_FSYNC:
    case UV_FS_FDATASYNC:
      fs__fsync(req, (uv_file)req->arg0);
      break;
    case UV_FS_FTRUNCATE:
      fs__ftruncate(req, (uv_file)req->arg0, (off_t)req->arg1);
      break;
    case UV_FS_SENDFILE:
      fs__sendfile(req,
        (uv_file) req->arg0,
        (uv_file) req->arg1,
        (off_t) req->arg2,
        (size_t) req->arg3);
      break;
    case UV_FS_CHMOD:
      fs__chmod(req, req->path, (int)req->arg0);
      break;
    case UV_FS_FCHMOD:
      fs__fchmod(req, (uv_file)req->arg0, (int)req->arg1);
      break;
    case UV_FS_UTIME:
      fs__utime(req, req->path, req->arg4, req->arg5);
      break;
    case UV_FS_FUTIME:
      fs__futime(req, (uv_file)req->arg0, req->arg4, req->arg5);
      break;
    case UV_FS_LINK:
      fs__link(req, req->path, (const char*)req->arg0);
      break;
    case UV_FS_SYMLINK:
      fs__symlink(req, req->path, (const char*)req->arg0, (int)req->arg1);
      break;
    case UV_FS_READLINK:
      fs__readlink(req, req->path);
      break;
    case UV_FS_CHOWN:
    case UV_FS_FCHOWN:
      fs__nop(req);
      break;
    default:
      assert(!"bad uv_fs_type");
  }

  POST_COMPLETION_FOR_REQ(loop, req);

  return 0;
}


int uv_fs_open(uv_loop_t* loop, uv_fs_t* req, const char* path, int flags,
    int mode, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_OPEN, path, cb);
    WRAP_REQ_ARGS2(req, flags, mode);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_OPEN);
    fs__open(req, path, flags, mode);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_close(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_CLOSE, NULL, cb);
    WRAP_REQ_ARGS1(req, file);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_CLOSE);
    fs__close(req, file);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_read(uv_loop_t* loop, uv_fs_t* req, uv_file file, void* buf,
    size_t length, off_t offset, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_READ, NULL, cb);
    WRAP_REQ_ARGS4(req, file, buf, length, offset);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_READ);
    fs__read(req, file, buf, length, offset);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_write(uv_loop_t* loop, uv_fs_t* req, uv_file file, void* buf,
    size_t length, off_t offset, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_WRITE, NULL, cb);
    WRAP_REQ_ARGS4(req, file, buf, length, offset);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_WRITE);
    fs__write(req, file, buf, length, offset);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_unlink(uv_loop_t* loop, uv_fs_t* req, const char* path,
    uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_UNLINK, path, cb);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_UNLINK);
    fs__unlink(req, path);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_mkdir(uv_loop_t* loop, uv_fs_t* req, const char* path, int mode,
    uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_MKDIR, path, cb);
    WRAP_REQ_ARGS1(req, mode);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_MKDIR);
    fs__mkdir(req, path, mode);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_rmdir(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_RMDIR, path, cb);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_RMDIR);
    fs__rmdir(req, path);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_readdir(uv_loop_t* loop, uv_fs_t* req, const char* path, int flags,
    uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_READDIR, path, cb);
    WRAP_REQ_ARGS1(req, flags);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_READDIR);
    fs__readdir(req, path, flags);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_link(uv_loop_t* loop, uv_fs_t* req, const char* path,
    const char* new_path, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_LINK, path, cb);
    WRAP_REQ_ARGS1(req, new_path);
    STRDUP_ARG(req, 0);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_LINK);
    fs__link(req, path, new_path);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_symlink(uv_loop_t* loop, uv_fs_t* req, const char* path,
    const char* new_path, int flags, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_SYMLINK, path, cb);
    WRAP_REQ_ARGS2(req, new_path, flags);
    STRDUP_ARG(req, 0);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_SYMLINK);
    fs__symlink(req, path, new_path, flags);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_readlink(uv_loop_t* loop, uv_fs_t* req, const char* path,
    uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_READLINK, path, cb);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_READLINK);
    fs__readlink(req, path);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_chown(uv_loop_t* loop, uv_fs_t* req, const char* path, int uid,
    int gid, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_CHOWN, path, cb);
    WRAP_REQ_ARGS2(req, uid, gid);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_CHOWN);
    fs__nop(req);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_fchown(uv_loop_t* loop, uv_fs_t* req, uv_file file, int uid,
    int gid, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_FCHOWN, NULL, cb);
    WRAP_REQ_ARGS3(req, file, uid, gid);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_FCHOWN);
    fs__nop(req);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_stat(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
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
    uv_fs_req_init_async(loop, req, UV_FS_STAT, NULL, cb);
    if (path2) {
      req->path = path2;
    } else {
      req->path = strdup(path);
    }

    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_STAT);
    fs__stat(req, path2 ? path2 : path);
    if (path2) {
      free(path2);
    }
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


/* TODO: add support for links. */
int uv_fs_lstat(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb) {
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
    uv_fs_req_init_async(loop, req, UV_FS_LSTAT, NULL, cb);
     if (path2) {
      req->path = path2;
    } else {
      req->path = strdup(path);
    }

    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_LSTAT);
    fs__stat(req, path2 ? path2 : path);
    if (path2) {
      free(path2);
    }
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_fstat(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_FSTAT, NULL, cb);
    WRAP_REQ_ARGS1(req, file);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_FSTAT);
    fs__fstat(req, file);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_rename(uv_loop_t* loop, uv_fs_t* req, const char* path,
    const char* new_path, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_RENAME, path, cb);
    WRAP_REQ_ARGS1(req, new_path);
    STRDUP_ARG(req, 0);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_RENAME);
    fs__rename(req, path, new_path);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_fdatasync(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_FDATASYNC, NULL, cb);
    WRAP_REQ_ARGS1(req, file);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_FDATASYNC);
    fs__fsync(req, file);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_fsync(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_FSYNC, NULL, cb);
    WRAP_REQ_ARGS1(req, file);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_FSYNC);
    fs__fsync(req, file);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_ftruncate(uv_loop_t* loop, uv_fs_t* req, uv_file file,
    off_t offset, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_FTRUNCATE, NULL, cb);
    WRAP_REQ_ARGS2(req, file, offset);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_FTRUNCATE);
    fs__ftruncate(req, file, offset);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_sendfile(uv_loop_t* loop, uv_fs_t* req, uv_file out_fd,
    uv_file in_fd, off_t in_offset, size_t length, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_SENDFILE, NULL, cb);
    WRAP_REQ_ARGS4(req, out_fd, in_fd, in_offset, length);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_SENDFILE);
    fs__sendfile(req, out_fd, in_fd, in_offset, length);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_chmod(uv_loop_t* loop, uv_fs_t* req, const char* path, int mode,
    uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_CHMOD, path, cb);
    WRAP_REQ_ARGS1(req, mode);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_CHMOD);
    fs__chmod(req, path, mode);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_fchmod(uv_loop_t* loop, uv_fs_t* req, uv_file file, int mode,
    uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_FCHMOD, NULL, cb);
    WRAP_REQ_ARGS2(req, file, mode);
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_FCHMOD);
    fs__fchmod(req, file, mode);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_utime(uv_loop_t* loop, uv_fs_t* req, const char* path, double atime,
    double mtime, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_UTIME, path, cb);
    req->arg4 = (ssize_t)atime;
    req->arg5 = (ssize_t)mtime;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_UTIME);
    fs__utime(req, path, atime, mtime);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


int uv_fs_futime(uv_loop_t* loop, uv_fs_t* req, uv_file file, double atime,
    double mtime, uv_fs_cb cb) {
  if (cb) {
    uv_fs_req_init_async(loop, req, UV_FS_FUTIME, NULL, cb);
    WRAP_REQ_ARGS1(req, file);
    req->arg4 = (ssize_t)atime;
    req->arg5 = (ssize_t)mtime;
    QUEUE_FS_TP_JOB(loop, req);
  } else {
    uv_fs_req_init_sync(loop, req, UV_FS_FUTIME);
    fs__futime(req, file, atime, mtime);
    SET_UV_LAST_ERROR_FROM_REQ(req);
    return req->result;
  }

  return 0;
}


void uv_process_fs_req(uv_loop_t* loop, uv_fs_t* req) {
  assert(req->cb);
  SET_UV_LAST_ERROR_FROM_REQ(req);
  req->cb(req);
}


void uv_fs_req_cleanup(uv_fs_t* req) {
  uv_loop_t* loop = req->loop;

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
  }

  req->ptr = NULL;

  if (req->path) {
    free(req->path);
    req->path = NULL;
  }

  if (req->flags & UV_FS_ASYNC_QUEUED) {
    uv_unref(loop);
  }

  req->flags |= UV_FS_CLEANEDUP;
}
