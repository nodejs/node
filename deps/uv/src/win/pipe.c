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
#include <io.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "uv.h"
#include "internal.h"
#include "handle-inl.h"
#include "stream-inl.h"
#include "req-inl.h"

typedef struct uv__ipc_queue_item_s uv__ipc_queue_item_t;

struct uv__ipc_queue_item_s {
  /*
   * NOTE: It is important for socket_info_ex to be the first field,
   * because we will we assigning it to the pending_ipc_info.socket_info
   */
  uv__ipc_socket_info_ex socket_info_ex;
  QUEUE member;
  int tcp_connection;
};

/* A zero-size buffer for use by uv_pipe_read */
static char uv_zero_[] = "";

/* Null uv_buf_t */
static const uv_buf_t uv_null_buf_ = { 0, NULL };

/* The timeout that the pipe will wait for the remote end to write data */
/* when the local ends wants to shut it down. */
static const int64_t eof_timeout = 50; /* ms */

static const int default_pending_pipe_instances = 4;

/* Pipe prefix */
static char pipe_prefix[] = "\\\\?\\pipe";
static const int pipe_prefix_len = sizeof(pipe_prefix) - 1;

/* IPC protocol flags. */
#define UV_IPC_RAW_DATA       0x0001
#define UV_IPC_TCP_SERVER     0x0002
#define UV_IPC_TCP_CONNECTION 0x0004

/* IPC frame header. */
typedef struct {
  int flags;
  uint64_t raw_data_length;
} uv_ipc_frame_header_t;

/* IPC frame, which contains an imported TCP socket stream. */
typedef struct {
  uv_ipc_frame_header_t header;
  uv__ipc_socket_info_ex socket_info_ex;
} uv_ipc_frame_uv_stream;

static void eof_timer_init(uv_pipe_t* pipe);
static void eof_timer_start(uv_pipe_t* pipe);
static void eof_timer_stop(uv_pipe_t* pipe);
static void eof_timer_cb(uv_timer_t* timer);
static void eof_timer_destroy(uv_pipe_t* pipe);
static void eof_timer_close_cb(uv_handle_t* handle);


static void uv_unique_pipe_name(char* ptr, char* name, size_t size) {
  _snprintf(name, size, "\\\\?\\pipe\\uv\\%p-%u", ptr, GetCurrentProcessId());
}


int uv_pipe_init(uv_loop_t* loop, uv_pipe_t* handle, int ipc) {
  uv_stream_init(loop, (uv_stream_t*)handle, UV_NAMED_PIPE);

  handle->reqs_pending = 0;
  handle->handle = INVALID_HANDLE_VALUE;
  handle->name = NULL;
  handle->ipc_pid = 0;
  handle->remaining_ipc_rawdata_bytes = 0;
  QUEUE_INIT(&handle->pending_ipc_info.queue);
  handle->pending_ipc_info.queue_len = 0;
  handle->ipc = ipc;
  handle->non_overlapped_writes_tail = NULL;
  handle->readfile_thread = NULL;

  uv_req_init(loop, (uv_req_t*) &handle->ipc_header_write_req);

  return 0;
}


static void uv_pipe_connection_init(uv_pipe_t* handle) {
  uv_connection_init((uv_stream_t*) handle);
  handle->read_req.data = handle;
  handle->eof_timer = NULL;
  assert(!(handle->flags & UV_HANDLE_PIPESERVER));
  if (pCancelSynchronousIo &&
      handle->flags & UV_HANDLE_NON_OVERLAPPED_PIPE) {
      uv_mutex_init(&handle->readfile_mutex);
      handle->flags |= UV_HANDLE_PIPE_READ_CANCELABLE;
  }
}


static HANDLE open_named_pipe(const WCHAR* name, DWORD* duplex_flags) {
  HANDLE pipeHandle;

  /*
   * Assume that we have a duplex pipe first, so attempt to
   * connect with GENERIC_READ | GENERIC_WRITE.
   */
  pipeHandle = CreateFileW(name,
                           GENERIC_READ | GENERIC_WRITE,
                           0,
                           NULL,
                           OPEN_EXISTING,
                           FILE_FLAG_OVERLAPPED,
                           NULL);
  if (pipeHandle != INVALID_HANDLE_VALUE) {
    *duplex_flags = UV_HANDLE_READABLE | UV_HANDLE_WRITABLE;
    return pipeHandle;
  }

  /*
   * If the pipe is not duplex CreateFileW fails with
   * ERROR_ACCESS_DENIED.  In that case try to connect
   * as a read-only or write-only.
   */
  if (GetLastError() == ERROR_ACCESS_DENIED) {
    pipeHandle = CreateFileW(name,
                             GENERIC_READ | FILE_WRITE_ATTRIBUTES,
                             0,
                             NULL,
                             OPEN_EXISTING,
                             FILE_FLAG_OVERLAPPED,
                             NULL);

    if (pipeHandle != INVALID_HANDLE_VALUE) {
      *duplex_flags = UV_HANDLE_READABLE;
      return pipeHandle;
    }
  }

  if (GetLastError() == ERROR_ACCESS_DENIED) {
    pipeHandle = CreateFileW(name,
                             GENERIC_WRITE | FILE_READ_ATTRIBUTES,
                             0,
                             NULL,
                             OPEN_EXISTING,
                             FILE_FLAG_OVERLAPPED,
                             NULL);

    if (pipeHandle != INVALID_HANDLE_VALUE) {
      *duplex_flags = UV_HANDLE_WRITABLE;
      return pipeHandle;
    }
  }

  return INVALID_HANDLE_VALUE;
}


int uv_stdio_pipe_server(uv_loop_t* loop, uv_pipe_t* handle, DWORD access,
    char* name, size_t nameSize) {
  HANDLE pipeHandle;
  int err;
  char* ptr = (char*)handle;

  for (;;) {
    uv_unique_pipe_name(ptr, name, nameSize);

    pipeHandle = CreateNamedPipeA(name,
      access | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 65536, 65536, 0,
      NULL);

    if (pipeHandle != INVALID_HANDLE_VALUE) {
      /* No name collisions.  We're done. */
      break;
    }

    err = GetLastError();
    if (err != ERROR_PIPE_BUSY && err != ERROR_ACCESS_DENIED) {
      goto error;
    }

    /* Pipe name collision.  Increment the pointer and try again. */
    ptr++;
  }

  if (CreateIoCompletionPort(pipeHandle,
                             loop->iocp,
                             (ULONG_PTR)handle,
                             0) == NULL) {
    err = GetLastError();
    goto error;
  }

  uv_pipe_connection_init(handle);
  handle->handle = pipeHandle;

  return 0;

 error:
  if (pipeHandle != INVALID_HANDLE_VALUE) {
    CloseHandle(pipeHandle);
  }

  return err;
}


static int uv_set_pipe_handle(uv_loop_t* loop,
                              uv_pipe_t* handle,
                              HANDLE pipeHandle,
                              DWORD duplex_flags) {
  NTSTATUS nt_status;
  IO_STATUS_BLOCK io_status;
  FILE_MODE_INFORMATION mode_info;
  DWORD mode = PIPE_READMODE_BYTE | PIPE_WAIT;
  DWORD current_mode = 0;
  DWORD err = 0;

  if (!SetNamedPipeHandleState(pipeHandle, &mode, NULL, NULL)) {
    err = GetLastError();
    if (err == ERROR_ACCESS_DENIED) {
      /*
       * SetNamedPipeHandleState can fail if the handle doesn't have either
       * GENERIC_WRITE  or FILE_WRITE_ATTRIBUTES.
       * But if the handle already has the desired wait and blocking modes
       * we can continue.
       */
      if (!GetNamedPipeHandleState(pipeHandle, &current_mode, NULL, NULL,
                                   NULL, NULL, 0)) {
        return -1;
      } else if (current_mode & PIPE_NOWAIT) {
        SetLastError(ERROR_ACCESS_DENIED);
        return -1;
      }
    } else {
      /* If this returns ERROR_INVALID_PARAMETER we probably opened
       * something that is not a pipe. */
      if (err == ERROR_INVALID_PARAMETER) {
        SetLastError(WSAENOTSOCK);
      }
      return -1;
    }
  }

  /* Check if the pipe was created with FILE_FLAG_OVERLAPPED. */
  nt_status = pNtQueryInformationFile(pipeHandle,
                                      &io_status,
                                      &mode_info,
                                      sizeof(mode_info),
                                      FileModeInformation);
  if (nt_status != STATUS_SUCCESS) {
    return -1;
  }

  if (mode_info.Mode & FILE_SYNCHRONOUS_IO_ALERT ||
      mode_info.Mode & FILE_SYNCHRONOUS_IO_NONALERT) {
    /* Non-overlapped pipe. */
    handle->flags |= UV_HANDLE_NON_OVERLAPPED_PIPE;
  } else {
    /* Overlapped pipe.  Try to associate with IOCP. */
    if (CreateIoCompletionPort(pipeHandle,
                               loop->iocp,
                               (ULONG_PTR)handle,
                               0) == NULL) {
      handle->flags |= UV_HANDLE_EMULATE_IOCP;
    }
  }

  handle->handle = pipeHandle;
  handle->flags |= duplex_flags;

  return 0;
}


static DWORD WINAPI pipe_shutdown_thread_proc(void* parameter) {
  uv_loop_t* loop;
  uv_pipe_t* handle;
  uv_shutdown_t* req;

  req = (uv_shutdown_t*) parameter;
  assert(req);
  handle = (uv_pipe_t*) req->handle;
  assert(handle);
  loop = handle->loop;
  assert(loop);

  FlushFileBuffers(handle->handle);

  /* Post completed */
  POST_COMPLETION_FOR_REQ(loop, req);

  return 0;
}


void uv_pipe_endgame(uv_loop_t* loop, uv_pipe_t* handle) {
  int err;
  DWORD result;
  uv_shutdown_t* req;
  NTSTATUS nt_status;
  IO_STATUS_BLOCK io_status;
  FILE_PIPE_LOCAL_INFORMATION pipe_info;
  uv__ipc_queue_item_t* item;

  if (handle->flags & UV_HANDLE_PIPE_READ_CANCELABLE) {
    handle->flags &= ~UV_HANDLE_PIPE_READ_CANCELABLE;
    uv_mutex_destroy(&handle->readfile_mutex);
  }

  if ((handle->flags & UV_HANDLE_CONNECTION) &&
      handle->shutdown_req != NULL &&
      handle->write_reqs_pending == 0) {
    req = handle->shutdown_req;

    /* Clear the shutdown_req field so we don't go here again. */
    handle->shutdown_req = NULL;

    if (handle->flags & UV__HANDLE_CLOSING) {
      UNREGISTER_HANDLE_REQ(loop, handle, req);

      /* Already closing. Cancel the shutdown. */
      if (req->cb) {
        req->cb(req, UV_ECANCELED);
      }

      DECREASE_PENDING_REQ_COUNT(handle);
      return;
    }

    /* Try to avoid flushing the pipe buffer in the thread pool. */
    nt_status = pNtQueryInformationFile(handle->handle,
                                        &io_status,
                                        &pipe_info,
                                        sizeof pipe_info,
                                        FilePipeLocalInformation);

    if (nt_status != STATUS_SUCCESS) {
      /* Failure */
      UNREGISTER_HANDLE_REQ(loop, handle, req);

      handle->flags |= UV_HANDLE_WRITABLE; /* Questionable */
      if (req->cb) {
        err = pRtlNtStatusToDosError(nt_status);
        req->cb(req, uv_translate_sys_error(err));
      }

      DECREASE_PENDING_REQ_COUNT(handle);
      return;
    }

    if (pipe_info.OutboundQuota == pipe_info.WriteQuotaAvailable) {
      /* Short-circuit, no need to call FlushFileBuffers. */
      uv_insert_pending_req(loop, (uv_req_t*) req);
      return;
    }

    /* Run FlushFileBuffers in the thread pool. */
    result = QueueUserWorkItem(pipe_shutdown_thread_proc,
                               req,
                               WT_EXECUTELONGFUNCTION);
    if (result) {
      return;

    } else {
      /* Failure. */
      UNREGISTER_HANDLE_REQ(loop, handle, req);

      handle->flags |= UV_HANDLE_WRITABLE; /* Questionable */
      if (req->cb) {
        err = GetLastError();
        req->cb(req, uv_translate_sys_error(err));
      }

      DECREASE_PENDING_REQ_COUNT(handle);
      return;
    }
  }

  if (handle->flags & UV__HANDLE_CLOSING &&
      handle->reqs_pending == 0) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));

    if (handle->flags & UV_HANDLE_CONNECTION) {
      /* Free pending sockets */
      while (!QUEUE_EMPTY(&handle->pending_ipc_info.queue)) {
        QUEUE* q;
        SOCKET socket;

        q = QUEUE_HEAD(&handle->pending_ipc_info.queue);
        QUEUE_REMOVE(q);
        item = QUEUE_DATA(q, uv__ipc_queue_item_t, member);

        /* Materialize socket and close it */
        socket = WSASocketW(FROM_PROTOCOL_INFO,
                            FROM_PROTOCOL_INFO,
                            FROM_PROTOCOL_INFO,
                            &item->socket_info_ex.socket_info,
                            0,
                            WSA_FLAG_OVERLAPPED);
        free(item);

        if (socket != INVALID_SOCKET)
          closesocket(socket);
      }
      handle->pending_ipc_info.queue_len = 0;

      if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
        if (handle->read_req.wait_handle != INVALID_HANDLE_VALUE) {
          UnregisterWait(handle->read_req.wait_handle);
          handle->read_req.wait_handle = INVALID_HANDLE_VALUE;
        }
        if (handle->read_req.event_handle) {
          CloseHandle(handle->read_req.event_handle);
          handle->read_req.event_handle = NULL;
        }
      }
    }

    if (handle->flags & UV_HANDLE_PIPESERVER) {
      assert(handle->accept_reqs);
      free(handle->accept_reqs);
      handle->accept_reqs = NULL;
    }

    uv__handle_close(handle);
  }
}


void uv_pipe_pending_instances(uv_pipe_t* handle, int count) {
  handle->pending_instances = count;
  handle->flags |= UV_HANDLE_PIPESERVER;
}


/* Creates a pipe server. */
int uv_pipe_bind(uv_pipe_t* handle, const char* name) {
  uv_loop_t* loop = handle->loop;
  int i, err, nameSize;
  uv_pipe_accept_t* req;

  if (handle->flags & UV_HANDLE_BOUND) {
    return UV_EINVAL;
  }

  if (!name) {
    return UV_EINVAL;
  }

  if (!(handle->flags & UV_HANDLE_PIPESERVER)) {
    handle->pending_instances = default_pending_pipe_instances;
  }

  handle->accept_reqs = (uv_pipe_accept_t*)
    malloc(sizeof(uv_pipe_accept_t) * handle->pending_instances);
  if (!handle->accept_reqs) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  for (i = 0; i < handle->pending_instances; i++) {
    req = &handle->accept_reqs[i];
    uv_req_init(loop, (uv_req_t*) req);
    req->type = UV_ACCEPT;
    req->data = handle;
    req->pipeHandle = INVALID_HANDLE_VALUE;
    req->next_pending = NULL;
  }

  /* Convert name to UTF16. */
  nameSize = uv_utf8_to_utf16(name, NULL, 0) * sizeof(WCHAR);
  handle->name = (WCHAR*)malloc(nameSize);
  if (!handle->name) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  if (!uv_utf8_to_utf16(name, handle->name, nameSize / sizeof(WCHAR))) {
    err = GetLastError();
    goto error;
  }

  /*
   * Attempt to create the first pipe with FILE_FLAG_FIRST_PIPE_INSTANCE.
   * If this fails then there's already a pipe server for the given pipe name.
   */
  handle->accept_reqs[0].pipeHandle = CreateNamedPipeW(handle->name,
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
      FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
      PIPE_UNLIMITED_INSTANCES, 65536, 65536, 0, NULL);

  if (handle->accept_reqs[0].pipeHandle == INVALID_HANDLE_VALUE) {
    err = GetLastError();
    if (err == ERROR_ACCESS_DENIED) {
      err = WSAEADDRINUSE;  /* Translates to UV_EADDRINUSE. */
    } else if (err == ERROR_PATH_NOT_FOUND || err == ERROR_INVALID_NAME) {
      err = WSAEACCES;  /* Translates to UV_EACCES. */
    }
    goto error;
  }

  if (uv_set_pipe_handle(loop, handle, handle->accept_reqs[0].pipeHandle, 0)) {
    err = GetLastError();
    goto error;
  }

  handle->pending_accepts = NULL;
  handle->flags |= UV_HANDLE_PIPESERVER;
  handle->flags |= UV_HANDLE_BOUND;

  return 0;

error:
  if (handle->name) {
    free(handle->name);
    handle->name = NULL;
  }

  if (handle->accept_reqs[0].pipeHandle != INVALID_HANDLE_VALUE) {
    CloseHandle(handle->accept_reqs[0].pipeHandle);
    handle->accept_reqs[0].pipeHandle = INVALID_HANDLE_VALUE;
  }

  return uv_translate_sys_error(err);
}


static DWORD WINAPI pipe_connect_thread_proc(void* parameter) {
  uv_loop_t* loop;
  uv_pipe_t* handle;
  uv_connect_t* req;
  HANDLE pipeHandle = INVALID_HANDLE_VALUE;
  DWORD duplex_flags;

  req = (uv_connect_t*) parameter;
  assert(req);
  handle = (uv_pipe_t*) req->handle;
  assert(handle);
  loop = handle->loop;
  assert(loop);

  /* We're here because CreateFile on a pipe returned ERROR_PIPE_BUSY. */
  /* We wait for the pipe to become available with WaitNamedPipe. */
  while (WaitNamedPipeW(handle->name, 30000)) {
    /* The pipe is now available, try to connect. */
    pipeHandle = open_named_pipe(handle->name, &duplex_flags);
    if (pipeHandle != INVALID_HANDLE_VALUE) {
      break;
    }

    SwitchToThread();
  }

  if (pipeHandle != INVALID_HANDLE_VALUE &&
      !uv_set_pipe_handle(loop, handle, pipeHandle, duplex_flags)) {
    SET_REQ_SUCCESS(req);
  } else {
    SET_REQ_ERROR(req, GetLastError());
  }

  /* Post completed */
  POST_COMPLETION_FOR_REQ(loop, req);

  return 0;
}


void uv_pipe_connect(uv_connect_t* req, uv_pipe_t* handle,
    const char* name, uv_connect_cb cb) {
  uv_loop_t* loop = handle->loop;
  int err, nameSize;
  HANDLE pipeHandle = INVALID_HANDLE_VALUE;
  DWORD duplex_flags;

  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_CONNECT;
  req->handle = (uv_stream_t*) handle;
  req->cb = cb;

  /* Convert name to UTF16. */
  nameSize = uv_utf8_to_utf16(name, NULL, 0) * sizeof(WCHAR);
  handle->name = (WCHAR*)malloc(nameSize);
  if (!handle->name) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  if (!uv_utf8_to_utf16(name, handle->name, nameSize / sizeof(WCHAR))) {
    err = GetLastError();
    goto error;
  }

  pipeHandle = open_named_pipe(handle->name, &duplex_flags);
  if (pipeHandle == INVALID_HANDLE_VALUE) {
    if (GetLastError() == ERROR_PIPE_BUSY) {
      /* Wait for the server to make a pipe instance available. */
      if (!QueueUserWorkItem(&pipe_connect_thread_proc,
                             req,
                             WT_EXECUTELONGFUNCTION)) {
        err = GetLastError();
        goto error;
      }

      REGISTER_HANDLE_REQ(loop, handle, req);
      handle->reqs_pending++;

      return;
    }

    err = GetLastError();
    goto error;
  }

  assert(pipeHandle != INVALID_HANDLE_VALUE);

  if (uv_set_pipe_handle(loop,
                         (uv_pipe_t*) req->handle,
                         pipeHandle,
                         duplex_flags)) {
    err = GetLastError();
    goto error;
  }

  SET_REQ_SUCCESS(req);
  uv_insert_pending_req(loop, (uv_req_t*) req);
  handle->reqs_pending++;
  REGISTER_HANDLE_REQ(loop, handle, req);
  return;

error:
  if (handle->name) {
    free(handle->name);
    handle->name = NULL;
  }

  if (pipeHandle != INVALID_HANDLE_VALUE) {
    CloseHandle(pipeHandle);
  }

  /* Make this req pending reporting an error. */
  SET_REQ_ERROR(req, err);
  uv_insert_pending_req(loop, (uv_req_t*) req);
  handle->reqs_pending++;
  REGISTER_HANDLE_REQ(loop, handle, req);
  return;
}


void uv__pipe_pause_read(uv_pipe_t* handle) {
  if (handle->flags & UV_HANDLE_PIPE_READ_CANCELABLE) {
      /* Pause the ReadFile task briefly, to work
         around the Windows kernel bug that causes
         any access to a NamedPipe to deadlock if
         any process has called ReadFile */
      HANDLE h;
      uv_mutex_lock(&handle->readfile_mutex);
      h = handle->readfile_thread;
      while (h) {
        /* spinlock: we expect this to finish quickly,
           or we are probably about to deadlock anyways
           (in the kernel), so it doesn't matter */
        pCancelSynchronousIo(h);
        SwitchToThread(); /* yield thread control briefly */
        h = handle->readfile_thread;
      }
  }
}


void uv__pipe_unpause_read(uv_pipe_t* handle) {
  if (handle->flags & UV_HANDLE_PIPE_READ_CANCELABLE) {
    uv_mutex_unlock(&handle->readfile_mutex);
  }
}


void uv__pipe_stop_read(uv_pipe_t* handle) {
  handle->flags &= ~UV_HANDLE_READING;
  uv__pipe_pause_read((uv_pipe_t*)handle);
  uv__pipe_unpause_read((uv_pipe_t*)handle);
}


/* Cleans up uv_pipe_t (server or connection) and all resources associated */
/* with it. */
void uv_pipe_cleanup(uv_loop_t* loop, uv_pipe_t* handle) {
  int i;
  HANDLE pipeHandle;

  uv__pipe_stop_read(handle);

  if (handle->name) {
    free(handle->name);
    handle->name = NULL;
  }

  if (handle->flags & UV_HANDLE_PIPESERVER) {
    for (i = 0; i < handle->pending_instances; i++) {
      pipeHandle = handle->accept_reqs[i].pipeHandle;
      if (pipeHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(pipeHandle);
        handle->accept_reqs[i].pipeHandle = INVALID_HANDLE_VALUE;
      }
    }
  }

  if (handle->flags & UV_HANDLE_CONNECTION) {
    handle->flags &= ~UV_HANDLE_WRITABLE;
    eof_timer_destroy(handle);
  }

  if ((handle->flags & UV_HANDLE_CONNECTION)
      && handle->handle != INVALID_HANDLE_VALUE) {
    CloseHandle(handle->handle);
    handle->handle = INVALID_HANDLE_VALUE;
  }

}


void uv_pipe_close(uv_loop_t* loop, uv_pipe_t* handle) {
  if (handle->flags & UV_HANDLE_READING) {
    handle->flags &= ~UV_HANDLE_READING;
    DECREASE_ACTIVE_COUNT(loop, handle);
  }

  if (handle->flags & UV_HANDLE_LISTENING) {
    handle->flags &= ~UV_HANDLE_LISTENING;
    DECREASE_ACTIVE_COUNT(loop, handle);
  }

  uv_pipe_cleanup(loop, handle);

  if (handle->reqs_pending == 0) {
    uv_want_endgame(loop, (uv_handle_t*) handle);
  }

  handle->flags &= ~(UV_HANDLE_READABLE | UV_HANDLE_WRITABLE);
  uv__handle_closing(handle);
}


static void uv_pipe_queue_accept(uv_loop_t* loop, uv_pipe_t* handle,
    uv_pipe_accept_t* req, BOOL firstInstance) {
  assert(handle->flags & UV_HANDLE_LISTENING);

  if (!firstInstance) {
    assert(req->pipeHandle == INVALID_HANDLE_VALUE);

    req->pipeHandle = CreateNamedPipeW(handle->name,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES, 65536, 65536, 0, NULL);

    if (req->pipeHandle == INVALID_HANDLE_VALUE) {
      SET_REQ_ERROR(req, GetLastError());
      uv_insert_pending_req(loop, (uv_req_t*) req);
      handle->reqs_pending++;
      return;
    }

    if (uv_set_pipe_handle(loop, handle, req->pipeHandle, 0)) {
      CloseHandle(req->pipeHandle);
      req->pipeHandle = INVALID_HANDLE_VALUE;
      SET_REQ_ERROR(req, GetLastError());
      uv_insert_pending_req(loop, (uv_req_t*) req);
      handle->reqs_pending++;
      return;
    }
  }

  assert(req->pipeHandle != INVALID_HANDLE_VALUE);

  /* Prepare the overlapped structure. */
  memset(&(req->overlapped), 0, sizeof(req->overlapped));

  if (!ConnectNamedPipe(req->pipeHandle, &req->overlapped) &&
      GetLastError() != ERROR_IO_PENDING) {
    if (GetLastError() == ERROR_PIPE_CONNECTED) {
      SET_REQ_SUCCESS(req);
    } else {
      CloseHandle(req->pipeHandle);
      req->pipeHandle = INVALID_HANDLE_VALUE;
      /* Make this req pending reporting an error. */
      SET_REQ_ERROR(req, GetLastError());
    }
    uv_insert_pending_req(loop, (uv_req_t*) req);
    handle->reqs_pending++;
    return;
  }

  handle->reqs_pending++;
}


int uv_pipe_accept(uv_pipe_t* server, uv_stream_t* client) {
  uv_loop_t* loop = server->loop;
  uv_pipe_t* pipe_client;
  uv_pipe_accept_t* req;
  QUEUE* q;
  uv__ipc_queue_item_t* item;
  int err;

  if (server->ipc) {
    if (QUEUE_EMPTY(&server->pending_ipc_info.queue)) {
      /* No valid pending sockets. */
      return WSAEWOULDBLOCK;
    }

    q = QUEUE_HEAD(&server->pending_ipc_info.queue);
    QUEUE_REMOVE(q);
    server->pending_ipc_info.queue_len--;
    item = QUEUE_DATA(q, uv__ipc_queue_item_t, member);

    err = uv_tcp_import((uv_tcp_t*)client,
                        &item->socket_info_ex,
                        item->tcp_connection);
    if (err != 0)
      return err;

    free(item);

  } else {
    pipe_client = (uv_pipe_t*)client;

    /* Find a connection instance that has been connected, but not yet */
    /* accepted. */
    req = server->pending_accepts;

    if (!req) {
      /* No valid connections found, so we error out. */
      return WSAEWOULDBLOCK;
    }

    /* Initialize the client handle and copy the pipeHandle to the client */
    uv_pipe_connection_init(pipe_client);
    pipe_client->handle = req->pipeHandle;
    pipe_client->flags |= UV_HANDLE_READABLE | UV_HANDLE_WRITABLE;

    /* Prepare the req to pick up a new connection */
    server->pending_accepts = req->next_pending;
    req->next_pending = NULL;
    req->pipeHandle = INVALID_HANDLE_VALUE;

    if (!(server->flags & UV__HANDLE_CLOSING)) {
      uv_pipe_queue_accept(loop, server, req, FALSE);
    }
  }

  return 0;
}


/* Starts listening for connections for the given pipe. */
int uv_pipe_listen(uv_pipe_t* handle, int backlog, uv_connection_cb cb) {
  uv_loop_t* loop = handle->loop;
  int i;

  if (handle->flags & UV_HANDLE_LISTENING) {
    handle->connection_cb = cb;
  }

  if (!(handle->flags & UV_HANDLE_BOUND)) {
    return WSAEINVAL;
  }

  if (handle->flags & UV_HANDLE_READING) {
    return WSAEISCONN;
  }

  if (!(handle->flags & UV_HANDLE_PIPESERVER)) {
    return ERROR_NOT_SUPPORTED;
  }

  handle->flags |= UV_HANDLE_LISTENING;
  INCREASE_ACTIVE_COUNT(loop, handle);
  handle->connection_cb = cb;

  /* First pipe handle should have already been created in uv_pipe_bind */
  assert(handle->accept_reqs[0].pipeHandle != INVALID_HANDLE_VALUE);

  for (i = 0; i < handle->pending_instances; i++) {
    uv_pipe_queue_accept(loop, handle, &handle->accept_reqs[i], i == 0);
  }

  return 0;
}


static DWORD WINAPI uv_pipe_zero_readfile_thread_proc(void* parameter) {
  int result;
  DWORD bytes;
  uv_read_t* req = (uv_read_t*) parameter;
  uv_pipe_t* handle = (uv_pipe_t*) req->data;
  uv_loop_t* loop = handle->loop;
  HANDLE hThread = NULL;
  DWORD err;
  uv_mutex_t *m = &handle->readfile_mutex;

  assert(req != NULL);
  assert(req->type == UV_READ);
  assert(handle->type == UV_NAMED_PIPE);

  if (handle->flags & UV_HANDLE_PIPE_READ_CANCELABLE) {
    uv_mutex_lock(m); /* mutex controls *setting* of readfile_thread */
    if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
                        GetCurrentProcess(), &hThread,
                        0, TRUE, DUPLICATE_SAME_ACCESS)) {
      handle->readfile_thread = hThread;
    } else {
      hThread = NULL;
    }
    uv_mutex_unlock(m);
  }
restart_readfile:
  result = ReadFile(handle->handle,
                    &uv_zero_,
                    0,
                    &bytes,
                    NULL);
  if (!result) {
    err = GetLastError();
    if (err == ERROR_OPERATION_ABORTED &&
        handle->flags & UV_HANDLE_PIPE_READ_CANCELABLE) {
      if (handle->flags & UV_HANDLE_READING) {
        /* just a brief break to do something else */
        handle->readfile_thread = NULL;
        /* resume after it is finished */
        uv_mutex_lock(m);
        handle->readfile_thread = hThread;
        uv_mutex_unlock(m);
        goto restart_readfile;
      } else {
        result = 1; /* successfully stopped reading */
      }
    }
  }
  if (hThread) {
    assert(hThread == handle->readfile_thread);
    /* mutex does not control clearing readfile_thread */
    handle->readfile_thread = NULL;
    uv_mutex_lock(m);
    /* only when we hold the mutex lock is it safe to
       open or close the handle */
    CloseHandle(hThread);
    uv_mutex_unlock(m);
  }

  if (!result) {
    SET_REQ_ERROR(req, err);
  }

  POST_COMPLETION_FOR_REQ(loop, req);
  return 0;
}


static DWORD WINAPI uv_pipe_writefile_thread_proc(void* parameter) {
  int result;
  DWORD bytes;
  uv_write_t* req = (uv_write_t*) parameter;
  uv_pipe_t* handle = (uv_pipe_t*) req->handle;
  uv_loop_t* loop = handle->loop;

  assert(req != NULL);
  assert(req->type == UV_WRITE);
  assert(handle->type == UV_NAMED_PIPE);
  assert(req->write_buffer.base);

  result = WriteFile(handle->handle,
                     req->write_buffer.base,
                     req->write_buffer.len,
                     &bytes,
                     NULL);

  if (!result) {
    SET_REQ_ERROR(req, GetLastError());
  }

  POST_COMPLETION_FOR_REQ(loop, req);
  return 0;
}


static void CALLBACK post_completion_read_wait(void* context, BOOLEAN timed_out) {
  uv_read_t* req;
  uv_tcp_t* handle;

  req = (uv_read_t*) context;
  assert(req != NULL);
  handle = (uv_tcp_t*)req->data;
  assert(handle != NULL);
  assert(!timed_out);

  if (!PostQueuedCompletionStatus(handle->loop->iocp,
                                  req->overlapped.InternalHigh,
                                  0,
                                  &req->overlapped)) {
    uv_fatal_error(GetLastError(), "PostQueuedCompletionStatus");
  }
}


static void CALLBACK post_completion_write_wait(void* context, BOOLEAN timed_out) {
  uv_write_t* req;
  uv_tcp_t* handle;

  req = (uv_write_t*) context;
  assert(req != NULL);
  handle = (uv_tcp_t*)req->handle;
  assert(handle != NULL);
  assert(!timed_out);

  if (!PostQueuedCompletionStatus(handle->loop->iocp,
                                  req->overlapped.InternalHigh,
                                  0,
                                  &req->overlapped)) {
    uv_fatal_error(GetLastError(), "PostQueuedCompletionStatus");
  }
}


static void uv_pipe_queue_read(uv_loop_t* loop, uv_pipe_t* handle) {
  uv_read_t* req;
  int result;

  assert(handle->flags & UV_HANDLE_READING);
  assert(!(handle->flags & UV_HANDLE_READ_PENDING));

  assert(handle->handle != INVALID_HANDLE_VALUE);

  req = &handle->read_req;

  if (handle->flags & UV_HANDLE_NON_OVERLAPPED_PIPE) {
    if (!QueueUserWorkItem(&uv_pipe_zero_readfile_thread_proc,
                           req,
                           WT_EXECUTELONGFUNCTION)) {
      /* Make this req pending reporting an error. */
      SET_REQ_ERROR(req, GetLastError());
      goto error;
    }
  } else {
    memset(&req->overlapped, 0, sizeof(req->overlapped));
    if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
      req->overlapped.hEvent = (HANDLE) ((uintptr_t) req->event_handle | 1);
    }

    /* Do 0-read */
    result = ReadFile(handle->handle,
                      &uv_zero_,
                      0,
                      NULL,
                      &req->overlapped);

    if (!result && GetLastError() != ERROR_IO_PENDING) {
      /* Make this req pending reporting an error. */
      SET_REQ_ERROR(req, GetLastError());
      goto error;
    }

    if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
      if (!req->event_handle) {
        req->event_handle = CreateEvent(NULL, 0, 0, NULL);
        if (!req->event_handle) {
          uv_fatal_error(GetLastError(), "CreateEvent");
        }
      }
      if (req->wait_handle == INVALID_HANDLE_VALUE) {
        if (!RegisterWaitForSingleObject(&req->wait_handle,
            req->overlapped.hEvent, post_completion_read_wait, (void*) req,
            INFINITE, WT_EXECUTEINWAITTHREAD)) {
          SET_REQ_ERROR(req, GetLastError());
          goto error;
        }
      }
    }
  }

  /* Start the eof timer if there is one */
  eof_timer_start(handle);
  handle->flags |= UV_HANDLE_READ_PENDING;
  handle->reqs_pending++;
  return;

error:
  uv_insert_pending_req(loop, (uv_req_t*)req);
  handle->flags |= UV_HANDLE_READ_PENDING;
  handle->reqs_pending++;
}


int uv_pipe_read_start(uv_pipe_t* handle,
                       uv_alloc_cb alloc_cb,
                       uv_read_cb read_cb) {
  uv_loop_t* loop = handle->loop;

  handle->flags |= UV_HANDLE_READING;
  INCREASE_ACTIVE_COUNT(loop, handle);
  handle->read_cb = read_cb;
  handle->alloc_cb = alloc_cb;

  /* If reading was stopped and then started again, there could still be a */
  /* read request pending. */
  if (!(handle->flags & UV_HANDLE_READ_PENDING))
    uv_pipe_queue_read(loop, handle);

  return 0;
}


static void uv_insert_non_overlapped_write_req(uv_pipe_t* handle,
    uv_write_t* req) {
  req->next_req = NULL;
  if (handle->non_overlapped_writes_tail) {
    req->next_req =
      handle->non_overlapped_writes_tail->next_req;
    handle->non_overlapped_writes_tail->next_req = (uv_req_t*)req;
    handle->non_overlapped_writes_tail = req;
  } else {
    req->next_req = (uv_req_t*)req;
    handle->non_overlapped_writes_tail = req;
  }
}


static uv_write_t* uv_remove_non_overlapped_write_req(uv_pipe_t* handle) {
  uv_write_t* req;

  if (handle->non_overlapped_writes_tail) {
    req = (uv_write_t*)handle->non_overlapped_writes_tail->next_req;

    if (req == handle->non_overlapped_writes_tail) {
      handle->non_overlapped_writes_tail = NULL;
    } else {
      handle->non_overlapped_writes_tail->next_req =
        req->next_req;
    }

    return req;
  } else {
    /* queue empty */
    return NULL;
  }
}


static void uv_queue_non_overlapped_write(uv_pipe_t* handle) {
  uv_write_t* req = uv_remove_non_overlapped_write_req(handle);
  if (req) {
    if (!QueueUserWorkItem(&uv_pipe_writefile_thread_proc,
                           req,
                           WT_EXECUTELONGFUNCTION)) {
      uv_fatal_error(GetLastError(), "QueueUserWorkItem");
    }
  }
}


static int uv_pipe_write_impl(uv_loop_t* loop,
                              uv_write_t* req,
                              uv_pipe_t* handle,
                              const uv_buf_t bufs[],
                              unsigned int nbufs,
                              uv_stream_t* send_handle,
                              uv_write_cb cb) {
  int err;
  int result;
  uv_tcp_t* tcp_send_handle;
  uv_write_t* ipc_header_req = NULL;
  uv_ipc_frame_uv_stream ipc_frame;

  if (nbufs != 1 && (nbufs != 0 || !send_handle)) {
    return ERROR_NOT_SUPPORTED;
  }

  /* Only TCP handles are supported for sharing. */
  if (send_handle && ((send_handle->type != UV_TCP) ||
      (!(send_handle->flags & UV_HANDLE_BOUND) &&
       !(send_handle->flags & UV_HANDLE_CONNECTION)))) {
    return ERROR_NOT_SUPPORTED;
  }

  assert(handle->handle != INVALID_HANDLE_VALUE);

  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_WRITE;
  req->handle = (uv_stream_t*) handle;
  req->cb = cb;
  req->ipc_header = 0;
  req->event_handle = NULL;
  req->wait_handle = INVALID_HANDLE_VALUE;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

  if (handle->ipc) {
    assert(!(handle->flags & UV_HANDLE_NON_OVERLAPPED_PIPE));
    ipc_frame.header.flags = 0;

    /* Use the IPC framing protocol. */
    if (send_handle) {
      tcp_send_handle = (uv_tcp_t*)send_handle;

      err = uv_tcp_duplicate_socket(tcp_send_handle, handle->ipc_pid,
          &ipc_frame.socket_info_ex.socket_info);
      if (err) {
        return err;
      }

      ipc_frame.socket_info_ex.delayed_error = tcp_send_handle->delayed_error;

      ipc_frame.header.flags |= UV_IPC_TCP_SERVER;

      if (tcp_send_handle->flags & UV_HANDLE_CONNECTION) {
        ipc_frame.header.flags |= UV_IPC_TCP_CONNECTION;
      }
    }

    if (nbufs == 1) {
      ipc_frame.header.flags |= UV_IPC_RAW_DATA;
      ipc_frame.header.raw_data_length = bufs[0].len;
    }

    /*
     * Use the provided req if we're only doing a single write.
     * If we're doing multiple writes, use ipc_header_write_req to do
     * the first write, and then use the provided req for the second write.
     */
    if (!(ipc_frame.header.flags & UV_IPC_RAW_DATA)) {
      ipc_header_req = req;
    } else {
      /*
       * Try to use the preallocated write req if it's available.
       * Otherwise allocate a new one.
       */
      if (handle->ipc_header_write_req.type != UV_WRITE) {
        ipc_header_req = (uv_write_t*)&handle->ipc_header_write_req;
      } else {
        ipc_header_req = (uv_write_t*)malloc(sizeof(uv_write_t));
        if (!ipc_header_req) {
          uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
        }
      }

      uv_req_init(loop, (uv_req_t*) ipc_header_req);
      ipc_header_req->type = UV_WRITE;
      ipc_header_req->handle = (uv_stream_t*) handle;
      ipc_header_req->cb = NULL;
      ipc_header_req->ipc_header = 1;
    }

    /* Write the header or the whole frame. */
    memset(&ipc_header_req->overlapped, 0, sizeof(ipc_header_req->overlapped));

    /* Using overlapped IO, but wait for completion before returning.
       This write is blocking because ipc_frame is on stack. */
    ipc_header_req->overlapped.hEvent = CreateEvent(NULL, 1, 0, NULL);
    if (!ipc_header_req->overlapped.hEvent) {
      uv_fatal_error(GetLastError(), "CreateEvent");
    }

    result = WriteFile(handle->handle,
                        &ipc_frame,
                        ipc_frame.header.flags & UV_IPC_TCP_SERVER ?
                          sizeof(ipc_frame) : sizeof(ipc_frame.header),
                        NULL,
                        &ipc_header_req->overlapped);
    if (!result && GetLastError() != ERROR_IO_PENDING) {
      err = GetLastError();
      CloseHandle(ipc_header_req->overlapped.hEvent);
      return err;
    }

    if (!result) {
      /* Request not completed immediately. Wait for it.*/
      if (WaitForSingleObject(ipc_header_req->overlapped.hEvent, INFINITE) !=
          WAIT_OBJECT_0) {
        err = GetLastError();
        CloseHandle(ipc_header_req->overlapped.hEvent);
        return err;
      }
    }
    ipc_header_req->queued_bytes = 0;
    CloseHandle(ipc_header_req->overlapped.hEvent);
    ipc_header_req->overlapped.hEvent = NULL;

    REGISTER_HANDLE_REQ(loop, handle, ipc_header_req);
    handle->reqs_pending++;
    handle->write_reqs_pending++;

    /* If we don't have any raw data to write - we're done. */
    if (!(ipc_frame.header.flags & UV_IPC_RAW_DATA)) {
      return 0;
    }
  }

  if ((handle->flags &
      (UV_HANDLE_BLOCKING_WRITES | UV_HANDLE_NON_OVERLAPPED_PIPE)) ==
      (UV_HANDLE_BLOCKING_WRITES | UV_HANDLE_NON_OVERLAPPED_PIPE)) {
    DWORD bytes;
    result = WriteFile(handle->handle,
                       bufs[0].base,
                       bufs[0].len,
                       &bytes,
                       NULL);

    if (!result) {
      err = GetLastError();
      return err;
    } else {
      /* Request completed immediately. */
      req->queued_bytes = 0;
    }

    REGISTER_HANDLE_REQ(loop, handle, req);
    handle->reqs_pending++;
    handle->write_reqs_pending++;
    POST_COMPLETION_FOR_REQ(loop, req);
    return 0;
  } else if (handle->flags & UV_HANDLE_NON_OVERLAPPED_PIPE) {
    req->write_buffer = bufs[0];
    uv_insert_non_overlapped_write_req(handle, req);
    if (handle->write_reqs_pending == 0) {
      uv_queue_non_overlapped_write(handle);
    }

    /* Request queued by the kernel. */
    req->queued_bytes = uv__count_bufs(bufs, nbufs);
    handle->write_queue_size += req->queued_bytes;
  } else if (handle->flags & UV_HANDLE_BLOCKING_WRITES) {
    /* Using overlapped IO, but wait for completion before returning */
    req->overlapped.hEvent = CreateEvent(NULL, 1, 0, NULL);
    if (!req->overlapped.hEvent) {
      uv_fatal_error(GetLastError(), "CreateEvent");
    }

    result = WriteFile(handle->handle,
                       bufs[0].base,
                       bufs[0].len,
                       NULL,
                       &req->overlapped);

    if (!result && GetLastError() != ERROR_IO_PENDING) {
      err = GetLastError();
      CloseHandle(req->overlapped.hEvent);
      return err;
    }

    if (result) {
      /* Request completed immediately. */
      req->queued_bytes = 0;
    } else {
      assert(ipc_header_req != NULL);
      /* Request queued by the kernel. */
      if (WaitForSingleObject(ipc_header_req->overlapped.hEvent, INFINITE) !=
          WAIT_OBJECT_0) {
        err = GetLastError();
        CloseHandle(ipc_header_req->overlapped.hEvent);
        return uv_translate_sys_error(err);
      }
    }
    CloseHandle(req->overlapped.hEvent);

    REGISTER_HANDLE_REQ(loop, handle, req);
    handle->reqs_pending++;
    handle->write_reqs_pending++;
    POST_COMPLETION_FOR_REQ(loop, req);
    return 0;
  } else {
    result = WriteFile(handle->handle,
                       bufs[0].base,
                       bufs[0].len,
                       NULL,
                       &req->overlapped);

    if (!result && GetLastError() != ERROR_IO_PENDING) {
      return GetLastError();
    }

    if (result) {
      /* Request completed immediately. */
      req->queued_bytes = 0;
    } else {
      /* Request queued by the kernel. */
      req->queued_bytes = uv__count_bufs(bufs, nbufs);
      handle->write_queue_size += req->queued_bytes;
    }

    if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
      req->event_handle = CreateEvent(NULL, 0, 0, NULL);
      if (!req->event_handle) {
        uv_fatal_error(GetLastError(), "CreateEvent");
      }
      if (!RegisterWaitForSingleObject(&req->wait_handle,
          req->overlapped.hEvent, post_completion_write_wait, (void*) req,
          INFINITE, WT_EXECUTEINWAITTHREAD)) {
        return GetLastError();
      }
    }
  }

  REGISTER_HANDLE_REQ(loop, handle, req);
  handle->reqs_pending++;
  handle->write_reqs_pending++;

  return 0;
}


int uv_pipe_write(uv_loop_t* loop,
                  uv_write_t* req,
                  uv_pipe_t* handle,
                  const uv_buf_t bufs[],
                  unsigned int nbufs,
                  uv_write_cb cb) {
  return uv_pipe_write_impl(loop, req, handle, bufs, nbufs, NULL, cb);
}


int uv_pipe_write2(uv_loop_t* loop,
                   uv_write_t* req,
                   uv_pipe_t* handle,
                   const uv_buf_t bufs[],
                   unsigned int nbufs,
                   uv_stream_t* send_handle,
                   uv_write_cb cb) {
  if (!handle->ipc) {
    return WSAEINVAL;
  }

  return uv_pipe_write_impl(loop, req, handle, bufs, nbufs, send_handle, cb);
}


static void uv_pipe_read_eof(uv_loop_t* loop, uv_pipe_t* handle,
    uv_buf_t buf) {
  /* If there is an eof timer running, we don't need it any more, */
  /* so discard it. */
  eof_timer_destroy(handle);

  handle->flags &= ~UV_HANDLE_READABLE;
  uv_read_stop((uv_stream_t*) handle);

  handle->read_cb((uv_stream_t*) handle, UV_EOF, &buf);
}


static void uv_pipe_read_error(uv_loop_t* loop, uv_pipe_t* handle, int error,
    uv_buf_t buf) {
  /* If there is an eof timer running, we don't need it any more, */
  /* so discard it. */
  eof_timer_destroy(handle);

  uv_read_stop((uv_stream_t*) handle);

  handle->read_cb((uv_stream_t*)handle, uv_translate_sys_error(error), &buf);
}


static void uv_pipe_read_error_or_eof(uv_loop_t* loop, uv_pipe_t* handle,
    int error, uv_buf_t buf) {
  if (error == ERROR_BROKEN_PIPE) {
    uv_pipe_read_eof(loop, handle, buf);
  } else {
    uv_pipe_read_error(loop, handle, error, buf);
  }
}


void uv__pipe_insert_pending_socket(uv_pipe_t* handle,
                                    uv__ipc_socket_info_ex* info,
                                    int tcp_connection) {
  uv__ipc_queue_item_t* item;

  item = (uv__ipc_queue_item_t*) malloc(sizeof(*item));
  if (item == NULL)
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");

  memcpy(&item->socket_info_ex, info, sizeof(item->socket_info_ex));
  item->tcp_connection = tcp_connection;
  QUEUE_INSERT_TAIL(&handle->pending_ipc_info.queue, &item->member);
  handle->pending_ipc_info.queue_len++;
}


void uv_process_pipe_read_req(uv_loop_t* loop, uv_pipe_t* handle,
    uv_req_t* req) {
  DWORD bytes, avail;
  uv_buf_t buf;
  uv_ipc_frame_uv_stream ipc_frame;

  assert(handle->type == UV_NAMED_PIPE);

  handle->flags &= ~UV_HANDLE_READ_PENDING;
  eof_timer_stop(handle);

  if (!REQ_SUCCESS(req)) {
    /* An error occurred doing the 0-read. */
    if (handle->flags & UV_HANDLE_READING) {
      uv_pipe_read_error_or_eof(loop,
                                handle,
                                GET_REQ_ERROR(req),
                                uv_null_buf_);
    }
  } else {
    /* Do non-blocking reads until the buffer is empty */
    while (handle->flags & UV_HANDLE_READING) {
      if (!PeekNamedPipe(handle->handle,
                          NULL,
                          0,
                          NULL,
                          &avail,
                          NULL)) {
        uv_pipe_read_error_or_eof(loop, handle, GetLastError(), uv_null_buf_);
        break;
      }

      if (avail == 0) {
        /* There is nothing to read after all. */
        break;
      }

      if (handle->ipc) {
        /* Use the IPC framing protocol to read the incoming data. */
        if (handle->remaining_ipc_rawdata_bytes == 0) {
          /* We're reading a new frame.  First, read the header. */
          assert(avail >= sizeof(ipc_frame.header));

          if (!ReadFile(handle->handle,
                        &ipc_frame.header,
                        sizeof(ipc_frame.header),
                        &bytes,
                        NULL)) {
            uv_pipe_read_error_or_eof(loop, handle, GetLastError(),
              uv_null_buf_);
            break;
          }

          assert(bytes == sizeof(ipc_frame.header));
          assert(ipc_frame.header.flags <= (UV_IPC_TCP_SERVER | UV_IPC_RAW_DATA |
            UV_IPC_TCP_CONNECTION));

          if (ipc_frame.header.flags & UV_IPC_TCP_SERVER) {
            assert(avail - sizeof(ipc_frame.header) >=
              sizeof(ipc_frame.socket_info_ex));

            /* Read the TCP socket info. */
            if (!ReadFile(handle->handle,
                          &ipc_frame.socket_info_ex,
                          sizeof(ipc_frame) - sizeof(ipc_frame.header),
                          &bytes,
                          NULL)) {
              uv_pipe_read_error_or_eof(loop, handle, GetLastError(),
                uv_null_buf_);
              break;
            }

            assert(bytes == sizeof(ipc_frame) - sizeof(ipc_frame.header));

            /* Store the pending socket info. */
            uv__pipe_insert_pending_socket(
                handle,
                &ipc_frame.socket_info_ex,
                ipc_frame.header.flags & UV_IPC_TCP_CONNECTION);
          }

          if (ipc_frame.header.flags & UV_IPC_RAW_DATA) {
            handle->remaining_ipc_rawdata_bytes =
              ipc_frame.header.raw_data_length;
            continue;
          }
        } else {
          avail = min(avail, (DWORD)handle->remaining_ipc_rawdata_bytes);
        }
      }

      handle->alloc_cb((uv_handle_t*) handle, avail, &buf);
      if (buf.len == 0) {
        handle->read_cb((uv_stream_t*) handle, UV_ENOBUFS, &buf);
        break;
      }
      assert(buf.base != NULL);

      if (ReadFile(handle->handle,
                   buf.base,
                   buf.len,
                   &bytes,
                   NULL)) {
        /* Successful read */
        if (handle->ipc) {
          assert(handle->remaining_ipc_rawdata_bytes >= bytes);
          handle->remaining_ipc_rawdata_bytes =
            handle->remaining_ipc_rawdata_bytes - bytes;
        }
        handle->read_cb((uv_stream_t*)handle, bytes, &buf);

        /* Read again only if bytes == buf.len */
        if (bytes <= buf.len) {
          break;
        }
      } else {
        uv_pipe_read_error_or_eof(loop, handle, GetLastError(), buf);
        break;
      }
    }

    /* Post another 0-read if still reading and not closing. */
    if ((handle->flags & UV_HANDLE_READING) &&
        !(handle->flags & UV_HANDLE_READ_PENDING)) {
      uv_pipe_queue_read(loop, handle);
    }
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_pipe_write_req(uv_loop_t* loop, uv_pipe_t* handle,
    uv_write_t* req) {
  int err;

  assert(handle->type == UV_NAMED_PIPE);

  assert(handle->write_queue_size >= req->queued_bytes);
  handle->write_queue_size -= req->queued_bytes;

  UNREGISTER_HANDLE_REQ(loop, handle, req);

  if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
    if (req->wait_handle != INVALID_HANDLE_VALUE) {
      UnregisterWait(req->wait_handle);
      req->wait_handle = INVALID_HANDLE_VALUE;
    }
    if (req->event_handle) {
      CloseHandle(req->event_handle);
      req->event_handle = NULL;
    }
  }

  if (req->ipc_header) {
    if (req == &handle->ipc_header_write_req) {
      req->type = UV_UNKNOWN_REQ;
    } else {
      free(req);
    }
  } else {
    if (req->cb) {
      err = GET_REQ_ERROR(req);
      req->cb(req, uv_translate_sys_error(err));
    }
  }

  handle->write_reqs_pending--;

  if (handle->flags & UV_HANDLE_NON_OVERLAPPED_PIPE &&
      handle->non_overlapped_writes_tail) {
    assert(handle->write_reqs_pending > 0);
    uv_queue_non_overlapped_write(handle);
  }

  if (handle->shutdown_req != NULL &&
      handle->write_reqs_pending == 0) {
    uv_want_endgame(loop, (uv_handle_t*)handle);
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_pipe_accept_req(uv_loop_t* loop, uv_pipe_t* handle,
    uv_req_t* raw_req) {
  uv_pipe_accept_t* req = (uv_pipe_accept_t*) raw_req;

  assert(handle->type == UV_NAMED_PIPE);

  if (handle->flags & UV__HANDLE_CLOSING) {
    /* The req->pipeHandle should be freed already in uv_pipe_cleanup(). */
    assert(req->pipeHandle == INVALID_HANDLE_VALUE);
    DECREASE_PENDING_REQ_COUNT(handle);
    return;
  }

  if (REQ_SUCCESS(req)) {
    assert(req->pipeHandle != INVALID_HANDLE_VALUE);
    req->next_pending = handle->pending_accepts;
    handle->pending_accepts = req;

    if (handle->connection_cb) {
      handle->connection_cb((uv_stream_t*)handle, 0);
    }
  } else {
    if (req->pipeHandle != INVALID_HANDLE_VALUE) {
      CloseHandle(req->pipeHandle);
      req->pipeHandle = INVALID_HANDLE_VALUE;
    }
    if (!(handle->flags & UV__HANDLE_CLOSING)) {
      uv_pipe_queue_accept(loop, handle, req, FALSE);
    }
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_pipe_connect_req(uv_loop_t* loop, uv_pipe_t* handle,
    uv_connect_t* req) {
  int err;

  assert(handle->type == UV_NAMED_PIPE);

  UNREGISTER_HANDLE_REQ(loop, handle, req);

  if (req->cb) {
    err = 0;
    if (REQ_SUCCESS(req)) {
      uv_pipe_connection_init(handle);
    } else {
      err = GET_REQ_ERROR(req);
    }
    req->cb(req, uv_translate_sys_error(err));
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_pipe_shutdown_req(uv_loop_t* loop, uv_pipe_t* handle,
    uv_shutdown_t* req) {
  assert(handle->type == UV_NAMED_PIPE);

  UNREGISTER_HANDLE_REQ(loop, handle, req);

  if (handle->flags & UV_HANDLE_READABLE) {
    /* Initialize and optionally start the eof timer. Only do this if the */
    /* pipe is readable and we haven't seen EOF come in ourselves. */
    eof_timer_init(handle);

    /* If reading start the timer right now. */
    /* Otherwise uv_pipe_queue_read will start it. */
    if (handle->flags & UV_HANDLE_READ_PENDING) {
      eof_timer_start(handle);
    }

  } else {
    /* This pipe is not readable. We can just close it to let the other end */
    /* know that we're done writing. */
    CloseHandle(handle->handle);
    handle->handle = INVALID_HANDLE_VALUE;
  }

  if (req->cb) {
    req->cb(req, 0);
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


static void eof_timer_init(uv_pipe_t* pipe) {
  int r;

  assert(pipe->eof_timer == NULL);
  assert(pipe->flags & UV_HANDLE_CONNECTION);

  pipe->eof_timer = (uv_timer_t*) malloc(sizeof *pipe->eof_timer);

  r = uv_timer_init(pipe->loop, pipe->eof_timer);
  assert(r == 0); /* timers can't fail */
  pipe->eof_timer->data = pipe;
  uv_unref((uv_handle_t*) pipe->eof_timer);
}


static void eof_timer_start(uv_pipe_t* pipe) {
  assert(pipe->flags & UV_HANDLE_CONNECTION);

  if (pipe->eof_timer != NULL) {
    uv_timer_start(pipe->eof_timer, eof_timer_cb, eof_timeout, 0);
  }
}


static void eof_timer_stop(uv_pipe_t* pipe) {
  assert(pipe->flags & UV_HANDLE_CONNECTION);

  if (pipe->eof_timer != NULL) {
    uv_timer_stop(pipe->eof_timer);
  }
}


static void eof_timer_cb(uv_timer_t* timer) {
  uv_pipe_t* pipe = (uv_pipe_t*) timer->data;
  uv_loop_t* loop = timer->loop;

  assert(pipe->type == UV_NAMED_PIPE);

  /* This should always be true, since we start the timer only */
  /* in uv_pipe_queue_read after successfully calling ReadFile, */
  /* or in uv_process_pipe_shutdown_req if a read is pending, */
  /* and we always immediately stop the timer in */
  /* uv_process_pipe_read_req. */
  assert(pipe->flags & UV_HANDLE_READ_PENDING);

  /* If there are many packets coming off the iocp then the timer callback */
  /* may be called before the read request is coming off the queue. */
  /* Therefore we check here if the read request has completed but will */
  /* be processed later. */
  if ((pipe->flags & UV_HANDLE_READ_PENDING) &&
      HasOverlappedIoCompleted(&pipe->read_req.overlapped)) {
    return;
  }

  /* Force both ends off the pipe. */
  CloseHandle(pipe->handle);
  pipe->handle = INVALID_HANDLE_VALUE;

  /* Stop reading, so the pending read that is going to fail will */
  /* not be reported to the user. */
  uv_read_stop((uv_stream_t*) pipe);

  /* Report the eof and update flags. This will get reported even if the */
  /* user stopped reading in the meantime. TODO: is that okay? */
  uv_pipe_read_eof(loop, pipe, uv_null_buf_);
}


static void eof_timer_destroy(uv_pipe_t* pipe) {
  assert(pipe->flags & UV_HANDLE_CONNECTION);

  if (pipe->eof_timer) {
    uv_close((uv_handle_t*) pipe->eof_timer, eof_timer_close_cb);
    pipe->eof_timer = NULL;
  }
}


static void eof_timer_close_cb(uv_handle_t* handle) {
  assert(handle->type == UV_TIMER);
  free(handle);
}


int uv_pipe_open(uv_pipe_t* pipe, uv_file file) {
  HANDLE os_handle = uv__get_osfhandle(file);
  NTSTATUS nt_status;
  IO_STATUS_BLOCK io_status;
  FILE_ACCESS_INFORMATION access;
  DWORD duplex_flags = 0;

  /* Determine what kind of permissions we have on this handle.
   * Cygwin opens the pipe in message mode, but we can support it,
   * just query the access flags and set the stream flags accordingly.
   */
  nt_status = pNtQueryInformationFile(os_handle,
                                      &io_status,
                                      &access,
                                      sizeof(access),
                                      FileAccessInformation);
  if (nt_status != STATUS_SUCCESS)
    return UV_EINVAL;

  if (pipe->ipc) {
    if (!(access.AccessFlags & FILE_WRITE_DATA) ||
        !(access.AccessFlags & FILE_READ_DATA)) {
      return UV_EINVAL;
    }
  }

  if (access.AccessFlags & FILE_WRITE_DATA)
    duplex_flags |= UV_HANDLE_WRITABLE;
  if (access.AccessFlags & FILE_READ_DATA)
    duplex_flags |= UV_HANDLE_READABLE;

  if (os_handle == INVALID_HANDLE_VALUE ||
      uv_set_pipe_handle(pipe->loop, pipe, os_handle, duplex_flags) == -1) {
    return UV_EINVAL;
  }

  uv_pipe_connection_init(pipe);

  if (pipe->ipc) {
    assert(!(pipe->flags & UV_HANDLE_NON_OVERLAPPED_PIPE));
    pipe->ipc_pid = uv_parent_pid();
    assert(pipe->ipc_pid != -1);
  }
  return 0;
}


static int uv__pipe_getname(const uv_pipe_t* handle, char* buffer, size_t* size) {
  NTSTATUS nt_status;
  IO_STATUS_BLOCK io_status;
  FILE_NAME_INFORMATION tmp_name_info;
  FILE_NAME_INFORMATION* name_info;
  WCHAR* name_buf;
  unsigned int addrlen;
  unsigned int name_size;
  unsigned int name_len;
  int err;

  name_info = NULL;

  if (handle->handle == INVALID_HANDLE_VALUE) {
    *size = 0;
    return UV_EINVAL;
  }

  uv__pipe_pause_read((uv_pipe_t*)handle); /* cast away const warning */

  nt_status = pNtQueryInformationFile(handle->handle,
                                      &io_status,
                                      &tmp_name_info,
                                      sizeof tmp_name_info,
                                      FileNameInformation);
  if (nt_status == STATUS_BUFFER_OVERFLOW) {
    name_size = sizeof(*name_info) + tmp_name_info.FileNameLength;
    name_info = malloc(name_size);
    if (!name_info) {
      *size = 0;
      err = UV_ENOMEM;
      goto cleanup;
    }

    nt_status = pNtQueryInformationFile(handle->handle,
                                        &io_status,
                                        name_info,
                                        name_size,
                                        FileNameInformation);
  }

  if (nt_status != STATUS_SUCCESS) {
    *size = 0;
    err = uv_translate_sys_error(pRtlNtStatusToDosError(nt_status));
    goto error;
  }

  if (!name_info) {
    /* the struct on stack was used */
    name_buf = tmp_name_info.FileName;
    name_len = tmp_name_info.FileNameLength;
  } else {
    name_buf = name_info->FileName;
    name_len = name_info->FileNameLength;
  }

  if (name_len == 0) {
    *size = 0;
    err = 0;
    goto error;
  }

  name_len /= sizeof(WCHAR);

  /* check how much space we need */
  addrlen = WideCharToMultiByte(CP_UTF8,
                                0,
                                name_buf,
                                name_len,
                                NULL,
                                0,
                                NULL,
                                NULL);
  if (!addrlen) {
    *size = 0;
    err = uv_translate_sys_error(GetLastError());
    goto error;
  } else if (pipe_prefix_len + addrlen > *size) {
    /* "\\\\.\\pipe" + name */
    *size = pipe_prefix_len + addrlen;
    err = UV_ENOBUFS;
    goto error;
  }

  memcpy(buffer, pipe_prefix, pipe_prefix_len);
  addrlen = WideCharToMultiByte(CP_UTF8,
                                0,
                                name_buf,
                                name_len,
                                buffer+pipe_prefix_len,
                                *size-pipe_prefix_len,
                                NULL,
                                NULL);
  if (!addrlen) {
    *size = 0;
    err = uv_translate_sys_error(GetLastError());
    goto error;
  }

  addrlen += pipe_prefix_len;
  *size = addrlen;

  err = 0;
  goto cleanup;

error:
  free(name_info);

cleanup:
  uv__pipe_unpause_read((uv_pipe_t*)handle); /* cast away const warning */
  return err;
}


int uv_pipe_pending_count(uv_pipe_t* handle) {
  if (!handle->ipc)
    return 0;
  return handle->pending_ipc_info.queue_len;
}


int uv_pipe_getsockname(const uv_pipe_t* handle, char* buffer, size_t* size) {
  if (handle->flags & UV_HANDLE_BOUND)
    return uv__pipe_getname(handle, buffer, size);

  if (handle->flags & UV_HANDLE_CONNECTION ||
      handle->handle != INVALID_HANDLE_VALUE) {
    *size = 0;
    return 0;
  }

  return UV_EBADF;
}


int uv_pipe_getpeername(const uv_pipe_t* handle, char* buffer, size_t* size) {
  /* emulate unix behaviour */
  if (handle->flags & UV_HANDLE_BOUND)
    return UV_ENOTCONN;

  if (handle->handle != INVALID_HANDLE_VALUE)
    return uv__pipe_getname(handle, buffer, size);

  return UV_EBADF;
}


uv_handle_type uv_pipe_pending_type(uv_pipe_t* handle) {
  if (!handle->ipc)
    return UV_UNKNOWN_HANDLE;
  if (handle->pending_ipc_info.queue_len == 0)
    return UV_UNKNOWN_HANDLE;
  else
    return UV_TCP;
}
