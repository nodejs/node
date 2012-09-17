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
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "uv.h"
#include "internal.h"
#include "handle-inl.h"
#include "req-inl.h"


const unsigned int uv_directory_watcher_buffer_size = 4096;


static void uv_fs_event_init_handle(uv_loop_t* loop, uv_fs_event_t* handle,
    const char* filename, uv_fs_event_cb cb) {
  uv__handle_init(loop, (uv_handle_t*) handle, UV_FS_EVENT);
  handle->cb = cb;
  handle->dir_handle = INVALID_HANDLE_VALUE;
  handle->buffer = NULL;
  handle->req_pending = 0;
  handle->filew = NULL;
  handle->short_filew = NULL;
  handle->dirw = NULL;

  uv_req_init(loop, (uv_req_t*)&handle->req);
  handle->req.type = UV_FS_EVENT_REQ;
  handle->req.data = (void*)handle;

  handle->filename = strdup(filename);
  if (!handle->filename) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  uv__handle_start(handle);
}


static void uv_fs_event_queue_readdirchanges(uv_loop_t* loop,
    uv_fs_event_t* handle) {
  assert(handle->dir_handle != INVALID_HANDLE_VALUE);
  assert(!handle->req_pending);

  memset(&(handle->req.overlapped), 0, sizeof(handle->req.overlapped));
  if (!ReadDirectoryChangesW(handle->dir_handle,
                             handle->buffer,
                             uv_directory_watcher_buffer_size,
                             FALSE,
                             FILE_NOTIFY_CHANGE_FILE_NAME      |
                               FILE_NOTIFY_CHANGE_DIR_NAME     |
                               FILE_NOTIFY_CHANGE_ATTRIBUTES   |
                               FILE_NOTIFY_CHANGE_SIZE         |
                               FILE_NOTIFY_CHANGE_LAST_WRITE   |
                               FILE_NOTIFY_CHANGE_LAST_ACCESS  |
                               FILE_NOTIFY_CHANGE_CREATION     |
                               FILE_NOTIFY_CHANGE_SECURITY,
                             NULL,
                             &handle->req.overlapped,
                             NULL)) {
    /* Make this req pending reporting an error. */
    SET_REQ_ERROR(&handle->req, GetLastError());
    uv_insert_pending_req(loop, (uv_req_t*)&handle->req);
  }

  handle->req_pending = 1;
}


static int uv_split_path(const WCHAR* filename, WCHAR** dir,
    WCHAR** file) {
  int len = wcslen(filename);
  int i = len;
  while (i > 0 && filename[--i] != '\\' && filename[i] != '/');

  if (i == 0) {
    if (dir) {
      *dir = (WCHAR*)malloc((MAX_PATH + 1) * sizeof(WCHAR));
      if (!*dir) {
        uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
      }

      if (!GetCurrentDirectoryW(MAX_PATH, *dir)) {
        free(*dir);
        *dir = NULL;
        return -1;
      }
    }

    *file = wcsdup(filename);
  } else {
    if (dir) {
      *dir = (WCHAR*)malloc((i + 1) * sizeof(WCHAR));
      if (!*dir) {
        uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
      }
      wcsncpy(*dir, filename, i);
      (*dir)[i] = L'\0';
    }

    *file = (WCHAR*)malloc((len - i) * sizeof(WCHAR));
    if (!*file) {
      uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
    }
    wcsncpy(*file, filename + i + 1, len - i - 1);
    (*file)[len - i - 1] = L'\0';
  }

  return 0;
}


int uv_fs_event_init(uv_loop_t* loop, uv_fs_event_t* handle,
    const char* filename, uv_fs_event_cb cb, int flags) {
  int name_size, is_path_dir;
  DWORD attr, last_error;
  WCHAR* dir = NULL, *dir_to_watch, *filenamew = NULL;
  WCHAR short_path[MAX_PATH];

  uv_fs_event_init_handle(loop, handle, filename, cb);

  /* Convert name to UTF16. */
  name_size = uv_utf8_to_utf16(filename, NULL, 0) * sizeof(WCHAR);
  filenamew = (WCHAR*)malloc(name_size);
  if (!filenamew) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  if (!uv_utf8_to_utf16(filename, filenamew,
      name_size / sizeof(WCHAR))) {
    uv__set_sys_error(loop, GetLastError());
    return -1;
  }

  /* Determine whether filename is a file or a directory. */
  attr = GetFileAttributesW(filenamew);
  if (attr == INVALID_FILE_ATTRIBUTES) {
    last_error = GetLastError();
    goto error;
  }

  is_path_dir = (attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;

  if (is_path_dir) {
     /* filename is a directory, so that's the directory that we will watch. */
    handle->dirw = filenamew;
    dir_to_watch = filenamew;
  } else {
    /*
     * filename is a file.  So we split filename into dir & file parts, and
     * watch the dir directory.
     */

    /* Convert to short path. */
    if (!GetShortPathNameW(filenamew, short_path, ARRAY_SIZE(short_path))) {
      last_error = GetLastError();
      goto error;
    }

    if (uv_split_path(filenamew, &dir, &handle->filew) != 0) {
      last_error = GetLastError();
      goto error;
    }

    if (uv_split_path(short_path, NULL, &handle->short_filew) != 0) {
      last_error = GetLastError();
      goto error;
    }

    dir_to_watch = dir;
    free(filenamew);
    filenamew = NULL;
  }

  handle->dir_handle = CreateFileW(dir_to_watch,
                                   FILE_LIST_DIRECTORY,
                                   FILE_SHARE_READ | FILE_SHARE_DELETE |
                                     FILE_SHARE_WRITE,
                                   NULL,
                                   OPEN_EXISTING,
                                   FILE_FLAG_BACKUP_SEMANTICS |
                                     FILE_FLAG_OVERLAPPED,
                                   NULL);

  if (dir) {
    free(dir);
    dir = NULL;
  }

  if (handle->dir_handle == INVALID_HANDLE_VALUE) {
    last_error = GetLastError();
    goto error;
  }

  if (CreateIoCompletionPort(handle->dir_handle,
                             loop->iocp,
                             (ULONG_PTR)handle,
                             0) == NULL) {
    last_error = GetLastError();
    goto error;
  }

  handle->buffer = (char*)_aligned_malloc(uv_directory_watcher_buffer_size,
    sizeof(DWORD));
  if (!handle->buffer) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  memset(&(handle->req.overlapped), 0, sizeof(handle->req.overlapped));

  if (!ReadDirectoryChangesW(handle->dir_handle,
                             handle->buffer,
                             uv_directory_watcher_buffer_size,
                             FALSE,
                             FILE_NOTIFY_CHANGE_FILE_NAME      |
                               FILE_NOTIFY_CHANGE_DIR_NAME     |
                               FILE_NOTIFY_CHANGE_ATTRIBUTES   |
                               FILE_NOTIFY_CHANGE_SIZE         |
                               FILE_NOTIFY_CHANGE_LAST_WRITE   |
                               FILE_NOTIFY_CHANGE_LAST_ACCESS  |
                               FILE_NOTIFY_CHANGE_CREATION     |
                               FILE_NOTIFY_CHANGE_SECURITY,
                             NULL,
                             &handle->req.overlapped,
                             NULL)) {
    last_error = GetLastError();
    goto error;
  }

  handle->req_pending = 1;
  return 0;

error:
  if (handle->filename) {
    free(handle->filename);
    handle->filename = NULL;
  }

  if (handle->filew) {
    free(handle->filew);
    handle->filew = NULL;
  }

  if (handle->short_filew) {
    free(handle->short_filew);
    handle->short_filew = NULL;
  }

  free(filenamew);

  if (handle->dir_handle != INVALID_HANDLE_VALUE) {
    CloseHandle(handle->dir_handle);
    handle->dir_handle = INVALID_HANDLE_VALUE;
  }

  if (handle->buffer) {
    _aligned_free(handle->buffer);
    handle->buffer = NULL;
  }

  uv__set_sys_error(loop, last_error);
  return -1;
}


void uv_process_fs_event_req(uv_loop_t* loop, uv_req_t* req,
    uv_fs_event_t* handle) {
  FILE_NOTIFY_INFORMATION* file_info;
  int sizew, size, result;
  char* filename = NULL;
  WCHAR* filenamew, *long_filenamew = NULL;
  DWORD offset = 0;

  assert(req->type == UV_FS_EVENT_REQ);
  assert(handle->req_pending);
  handle->req_pending = 0;

  /* If we're closing, don't report any callbacks, and just push the handle */
  /* onto the endgame queue. */
  if (handle->flags & UV__HANDLE_CLOSING) {
    uv_want_endgame(loop, (uv_handle_t*) handle);
    return;
  };

  file_info = (FILE_NOTIFY_INFORMATION*)(handle->buffer + offset);

  if (REQ_SUCCESS(req)) {
    if (req->overlapped.InternalHigh > 0) {
      do {
        file_info = (FILE_NOTIFY_INFORMATION*)((char*)file_info + offset);
        assert(!filename);
        assert(!long_filenamew);

        /*
         * Fire the event only if we were asked to watch a directory,
         * or if the filename filter matches.
         */
        if (handle->dirw ||
          _wcsnicmp(handle->filew, file_info->FileName,
            file_info->FileNameLength / sizeof(WCHAR)) == 0 ||
          _wcsnicmp(handle->short_filew, file_info->FileName,
            file_info->FileNameLength / sizeof(WCHAR)) == 0) {

          if (handle->dirw) {
            /*
             * We attempt to convert the file name to its long form for
             * events that still point to valid files on disk.
             * For removed and renamed events, we do not provide the file name.
             */
            if (file_info->Action != FILE_ACTION_REMOVED &&
              file_info->Action != FILE_ACTION_RENAMED_OLD_NAME) {
              /* Construct a full path to the file. */
              size = wcslen(handle->dirw) +
                file_info->FileNameLength / sizeof(WCHAR) + 2;

              filenamew = (WCHAR*)malloc(size * sizeof(WCHAR));
              if (!filenamew) {
                uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
              }

              _snwprintf(filenamew, size, L"%s\\%s", handle->dirw,
                file_info->FileName);

              filenamew[size - 1] = L'\0';

              /* Convert to long name. */
              size = GetLongPathNameW(filenamew, NULL, 0);

              if (size) {
                long_filenamew = (WCHAR*)malloc(size * sizeof(WCHAR));
                if (!long_filenamew) {
                  uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
                }

                size = GetLongPathNameW(filenamew, long_filenamew, size);
                if (size) {
                  long_filenamew[size] = '\0';
                } else {
                  free(long_filenamew);
                  long_filenamew = NULL;
                }
              }

              free(filenamew);

              if (long_filenamew) {
                /* Get the file name out of the long path. */
                result = uv_split_path(long_filenamew, NULL, &filenamew);
                free(long_filenamew);

                if (result == 0) {
                  long_filenamew = filenamew;
                  sizew = -1;
                } else {
                  long_filenamew = NULL;
                }
              }

              /*
               * If we couldn't get the long name - just use the name
               * provided by ReadDirectoryChangesW.
               */
              if (!long_filenamew) {
                filenamew = file_info->FileName;
                sizew = file_info->FileNameLength / sizeof(WCHAR);
              }
            } else {
              /* Removed or renamed callbacks don't provide filename. */
              filenamew = NULL;
            }
          } else {
            /* We already have the long name of the file, so just use it. */
            filenamew = handle->filew;
            sizew = -1;
          }

          if (filenamew) {
            /* Convert the filename to utf8. */
            size = uv_utf16_to_utf8(filenamew,
                                    sizew,
                                    NULL,
                                    0);
            if (size) {
              filename = (char*)malloc(size + 1);
              if (!filename) {
                uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
              }

              size = uv_utf16_to_utf8(filenamew,
                                      sizew,
                                      filename,
                                      size);
              if (size) {
                filename[size] = '\0';
              } else {
                free(filename);
                filename = NULL;
              }
            }
          }

          switch (file_info->Action) {
            case FILE_ACTION_ADDED:
            case FILE_ACTION_REMOVED:
            case FILE_ACTION_RENAMED_OLD_NAME:
            case FILE_ACTION_RENAMED_NEW_NAME:
              handle->cb(handle, filename, UV_RENAME, 0);
              break;

            case FILE_ACTION_MODIFIED:
              handle->cb(handle, filename, UV_CHANGE, 0);
              break;
          }

          free(filename);
          filename = NULL;
          free(long_filenamew);
          long_filenamew = NULL;
        }

        offset = file_info->NextEntryOffset;
      } while (offset && !(handle->flags & UV__HANDLE_CLOSING));
    } else {
      handle->cb(handle, NULL, UV_CHANGE, 0);
    }
  } else {
    uv__set_sys_error(loop, GET_REQ_ERROR(req));
    handle->cb(handle, NULL, 0, -1);
  }

  if (!(handle->flags & UV__HANDLE_CLOSING)) {
    uv_fs_event_queue_readdirchanges(loop, handle);
  } else {
    uv_want_endgame(loop, (uv_handle_t*)handle);
  }
}


void uv_fs_event_close(uv_loop_t* loop, uv_fs_event_t* handle) {
  if (handle->dir_handle != INVALID_HANDLE_VALUE) {
    CloseHandle(handle->dir_handle);
    handle->dir_handle = INVALID_HANDLE_VALUE;
  }

  if (!handle->req_pending) {
    uv_want_endgame(loop, (uv_handle_t*)handle);
  }

  uv__handle_closing(handle);
}


void uv_fs_event_endgame(uv_loop_t* loop, uv_fs_event_t* handle) {
  if (handle->flags & UV__HANDLE_CLOSING &&
      !handle->req_pending) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));

    if (handle->buffer) {
      _aligned_free(handle->buffer);
      handle->buffer = NULL;
    }

    if (handle->filew) {
      free(handle->filew);
      handle->filew = NULL;
    }

    if (handle->short_filew) {
      free(handle->short_filew);
      handle->short_filew = NULL;
    }

    if (handle->filename) {
      free(handle->filename);
      handle->filename = NULL;
    }

    if (handle->dirw) {
      free(handle->dirw);
      handle->dirw = NULL;
    }

    uv__handle_close(handle);
  }
}
