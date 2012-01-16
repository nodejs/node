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

#include "uv.h"
#include "../uv-common.h"
#include "internal.h"


/* A zero-size buffer for use by uv_pipe_read */
static char uv_zero_[] = "";

/* Null uv_buf_t */
static const uv_buf_t uv_null_buf_ = { 0, NULL };

/* The timeout that the pipe will wait for the remote end to write data */
/* when the local ends wants to shut it down. */
static const int64_t eof_timeout = 50; /* ms */

static const int default_pending_pipe_instances = 4;

/* IPC protocol flags. */
#define UV_IPC_RAW_DATA   0x0001
#define UV_IPC_UV_STREAM  0x0002

/* IPC frame header. */
typedef struct {
  int flags;
  uint64_t raw_data_length;
} uv_ipc_frame_header_t;

/* IPC frame, which contains an imported TCP socket stream. */
typedef struct {
  uv_ipc_frame_header_t header;
  WSAPROTOCOL_INFOW socket_info;
} uv_ipc_frame_uv_stream;

static void eof_timer_init(uv_pipe_t* pipe);
static void eof_timer_start(uv_pipe_t* pipe);
static void eof_timer_stop(uv_pipe_t* pipe);
static void eof_timer_cb(uv_timer_t* timer, int status);
static void eof_timer_destroy(uv_pipe_t* pipe);
static void eof_timer_close_cb(uv_handle_t* handle);


static void uv_unique_pipe_name(char* ptr, char* name, size_t size) {
  _snprintf(name, size, "\\\\.\\pipe\\uv\\%p-%d", ptr, GetCurrentProcessId());
}


int uv_pipe_init(uv_loop_t* loop, uv_pipe_t* handle, int ipc) {
  uv_stream_init(loop, (uv_stream_t*)handle);

  handle->type = UV_NAMED_PIPE;
  handle->reqs_pending = 0;
  handle->handle = INVALID_HANDLE_VALUE;
  handle->name = NULL;
  handle->ipc_pid = 0;
  handle->remaining_ipc_rawdata_bytes = 0;
  handle->pending_socket_info = NULL;
  handle->ipc = ipc;
  handle->non_overlapped_writes_tail = NULL;

  uv_req_init(loop, (uv_req_t*) &handle->ipc_header_write_req);

  loop->counters.pipe_init++;

  return 0;
}


static void uv_pipe_connection_init(uv_pipe_t* handle) {
  uv_connection_init((uv_stream_t*) handle);
  handle->read_req.data = handle;
  handle->eof_timer = NULL;
}


int uv_stdio_pipe_server(uv_loop_t* loop, uv_pipe_t* handle, DWORD access,
    char* name, size_t nameSize) {
  HANDLE pipeHandle;
  int errno;
  int err;
  char* ptr = (char*)handle;

  while (TRUE) {
    uv_unique_pipe_name(ptr, name, nameSize);

    pipeHandle = CreateNamedPipeA(name,
      access | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 65536, 65536, 0,
      NULL);

    if (pipeHandle != INVALID_HANDLE_VALUE) {
      /* No name collisions.  We're done. */
      break;
    }

    errno = GetLastError();
    if (errno != ERROR_PIPE_BUSY && errno != ERROR_ACCESS_DENIED) {
      uv__set_sys_error(loop, errno);
      err = -1;
      goto done;
    }

    /* Pipe name collision.  Increment the pointer and try again. */
    ptr++;
  }

  if (CreateIoCompletionPort(pipeHandle,
                             loop->iocp,
                             (ULONG_PTR)handle,
                             0) == NULL) {
    uv__set_sys_error(loop, GetLastError());
    err = -1;
    goto done;
  }

  uv_pipe_connection_init(handle);
  handle->handle = pipeHandle;
  err = 0;

done:
  if (err && pipeHandle != INVALID_HANDLE_VALUE) {
    CloseHandle(pipeHandle);
  }

  return err;
}


static int uv_set_pipe_handle(uv_loop_t* loop, uv_pipe_t* handle,
    HANDLE pipeHandle) {
  NTSTATUS nt_status;
  IO_STATUS_BLOCK io_status;
  FILE_MODE_INFORMATION mode_info;
  DWORD mode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;

  if (!SetNamedPipeHandleState(pipeHandle, &mode, NULL, NULL)) {
    return -1;
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

  return 0;
}


static DWORD WINAPI pipe_shutdown_thread_proc(void* parameter) {
  int errno;
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
  unsigned int uv_alloced;
  DWORD result;
  uv_shutdown_t* req;
  NTSTATUS nt_status;
  IO_STATUS_BLOCK io_status;
  FILE_PIPE_LOCAL_INFORMATION pipe_info;

  if (handle->flags & UV_HANDLE_SHUTTING &&
      !(handle->flags & UV_HANDLE_SHUT) &&
      handle->write_reqs_pending == 0) {
    req = handle->shutdown_req;

    /* Try to avoid flushing the pipe buffer in the thread pool. */
    nt_status = pNtQueryInformationFile(handle->handle,
                                        &io_status,
                                        &pipe_info,
                                        sizeof pipe_info,
                                        FilePipeLocalInformation);

    if (nt_status != STATUS_SUCCESS) {
      /* Failure */
      handle->flags &= ~UV_HANDLE_SHUTTING;
      if (req->cb) {
        uv__set_sys_error(loop, pRtlNtStatusToDosError(nt_status));
        req->cb(req, -1);
      }
      DECREASE_PENDING_REQ_COUNT(handle);
      return;
    }

    if (pipe_info.OutboundQuota == pipe_info.WriteQuotaAvailable) {
      handle->flags |= UV_HANDLE_SHUT;

      /* Short-circuit, no need to call FlushFileBuffers. */
      uv_insert_pending_req(loop, (uv_req_t*) req);
      return;
    }

    /* Run FlushFileBuffers in the thread pool. */
    result = QueueUserWorkItem(pipe_shutdown_thread_proc,
                               req,
                               WT_EXECUTELONGFUNCTION);
    if (result) {
      /* Mark the handle as shut now to avoid going through this again. */
      handle->flags |= UV_HANDLE_SHUT;
      return;

    } else {
      /* Failure. */
      handle->flags &= ~UV_HANDLE_SHUTTING;
      if (req->cb) {
        uv__set_sys_error(loop, GetLastError());
        req->cb(req, -1);
      }
      DECREASE_PENDING_REQ_COUNT(handle);
      return;
    }
  }

  if (handle->flags & UV_HANDLE_CLOSING &&
      handle->reqs_pending == 0) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    handle->flags |= UV_HANDLE_CLOSED;

    if (handle->flags & UV_HANDLE_CONNECTION) {
      if (handle->pending_socket_info) {
        free(handle->pending_socket_info);
        handle->pending_socket_info = NULL;
      }
      
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

    /* Remember the state of this flag because the close callback is */
    /* allowed to clobber or free the handle's memory */
    uv_alloced = handle->flags & UV_HANDLE_UV_ALLOCED;

    if (handle->close_cb) {
      handle->close_cb((uv_handle_t*)handle);
    }

    if (uv_alloced) {
      free(handle);
    }

    uv_unref(loop);
  }
}


void uv_pipe_pending_instances(uv_pipe_t* handle, int count) {
  handle->pending_instances = count;
  handle->flags |= UV_HANDLE_PIPESERVER;
}


/* Creates a pipe server. */
int uv_pipe_bind(uv_pipe_t* handle, const char* name) {
  uv_loop_t* loop = handle->loop;
  int i, errno, nameSize;
  uv_pipe_accept_t* req;

  if (handle->flags & UV_HANDLE_BOUND) {
    uv__set_sys_error(loop, WSAEINVAL);
    return -1;
  }

  if (!name) {
    uv__set_sys_error(loop, WSAEINVAL);
    return -1;
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
  nameSize = uv_utf8_to_utf16(name, NULL, 0) * sizeof(wchar_t);
  handle->name = (wchar_t*)malloc(nameSize);
  if (!handle->name) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  if (!uv_utf8_to_utf16(name, handle->name, nameSize / sizeof(wchar_t))) {
    uv__set_sys_error(loop, GetLastError());
    return -1;
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
    errno = GetLastError();
    if (errno == ERROR_ACCESS_DENIED) {
      uv__set_error(loop, UV_EADDRINUSE, errno);
    } else if (errno == ERROR_PATH_NOT_FOUND || errno == ERROR_INVALID_NAME) {
      uv__set_error(loop, UV_EACCES, errno);
    } else {
      uv__set_sys_error(loop, errno);
    }
    goto error;
  }

  if (uv_set_pipe_handle(loop, handle, handle->accept_reqs[0].pipeHandle)) {
    uv__set_sys_error(loop, GetLastError());
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

  return -1;
}


static DWORD WINAPI pipe_connect_thread_proc(void* parameter) {
  HANDLE pipeHandle = INVALID_HANDLE_VALUE;
  int errno;
  uv_loop_t* loop;
  uv_pipe_t* handle;
  uv_connect_t* req;

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
    pipeHandle = CreateFileW(handle->name,
                            GENERIC_READ | GENERIC_WRITE,
                            0,
                            NULL,
                            OPEN_EXISTING,
                            FILE_FLAG_OVERLAPPED,
                            NULL);

    if (pipeHandle != INVALID_HANDLE_VALUE) {
      break;
    }

    SwitchToThread();
  }

  if (pipeHandle != INVALID_HANDLE_VALUE &&
      !uv_set_pipe_handle(loop, handle, pipeHandle)) {
    handle->handle = pipeHandle;
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
  int errno, nameSize;
  HANDLE pipeHandle;

  handle->handle = INVALID_HANDLE_VALUE;

  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_CONNECT;
  req->handle = (uv_stream_t*) handle;
  req->cb = cb;

  /* Convert name to UTF16. */
  nameSize = uv_utf8_to_utf16(name, NULL, 0) * sizeof(wchar_t);
  handle->name = (wchar_t*)malloc(nameSize);
  if (!handle->name) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }

  if (!uv_utf8_to_utf16(name, handle->name, nameSize / sizeof(wchar_t))) {
    errno = GetLastError();
    goto error;
  }

  pipeHandle = CreateFileW(handle->name,
                          GENERIC_READ | GENERIC_WRITE,
                          0,
                          NULL,
                          OPEN_EXISTING,
                          FILE_FLAG_OVERLAPPED,
                          NULL);

  if (pipeHandle == INVALID_HANDLE_VALUE) {
    if (GetLastError() == ERROR_PIPE_BUSY) {
      /* Wait for the server to make a pipe instance available. */
      if (!QueueUserWorkItem(&pipe_connect_thread_proc,
                             req,
                             WT_EXECUTELONGFUNCTION)) {
        errno = GetLastError();
        goto error;
      }

      handle->reqs_pending++;

      return;
    }

    errno = GetLastError();
    goto error;
  }

  if (uv_set_pipe_handle(loop, (uv_pipe_t*)req->handle, pipeHandle)) {
    errno = GetLastError();
    goto error;
  }

  handle->handle = pipeHandle;

  SET_REQ_SUCCESS(req);
  uv_insert_pending_req(loop, (uv_req_t*) req);
  handle->reqs_pending++;
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
  SET_REQ_ERROR(req, errno);
  uv_insert_pending_req(loop, (uv_req_t*) req);
  handle->reqs_pending++;
  return;
}


/* Cleans up uv_pipe_t (server or connection) and all resources associated */
/* with it. */
void close_pipe(uv_pipe_t* handle, int* status, uv_err_t* err) {
  int i;
  HANDLE pipeHandle;

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
    eof_timer_destroy(handle);
  }

  if ((handle->flags & UV_HANDLE_CONNECTION)
      && handle->handle != INVALID_HANDLE_VALUE) {
    CloseHandle(handle->handle);
    handle->handle = INVALID_HANDLE_VALUE;
  }

  handle->flags |= UV_HANDLE_SHUT;
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

    if (uv_set_pipe_handle(loop, handle, req->pipeHandle)) {
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

  if (server->ipc) {
    if (!server->pending_socket_info) {
      /* No valid pending sockets. */
      uv__set_sys_error(loop, WSAEWOULDBLOCK);
      return -1;
    }

    return uv_tcp_import((uv_tcp_t*)client, server->pending_socket_info);
  } else {
    pipe_client = (uv_pipe_t*)client;

    /* Find a connection instance that has been connected, but not yet */
    /* accepted. */
    req = server->pending_accepts;

    if (!req) {
      /* No valid connections found, so we error out. */
      uv__set_sys_error(loop, WSAEWOULDBLOCK);
      return -1;
    }

    /* Initialize the client handle and copy the pipeHandle to the client */
    uv_pipe_connection_init(pipe_client);
    pipe_client->handle = req->pipeHandle;

    /* Prepare the req to pick up a new connection */
    server->pending_accepts = req->next_pending;
    req->next_pending = NULL;
    req->pipeHandle = INVALID_HANDLE_VALUE;

    if (!(server->flags & UV_HANDLE_CLOSING)) {
      uv_pipe_queue_accept(loop, server, req, FALSE);
    }
  }

  return 0;
}


/* Starts listening for connections for the given pipe. */
int uv_pipe_listen(uv_pipe_t* handle, int backlog, uv_connection_cb cb) {
  uv_loop_t* loop = handle->loop;

  int i, errno;

  if (!(handle->flags & UV_HANDLE_BOUND)) {
    uv__set_artificial_error(loop, UV_EINVAL);
    return -1;
  }

  if (handle->flags & UV_HANDLE_LISTENING ||
      handle->flags & UV_HANDLE_READING) {
    uv__set_artificial_error(loop, UV_EALREADY);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_PIPESERVER)) {
    uv__set_artificial_error(loop, UV_ENOTSUP);
    return -1;
  }

  handle->flags |= UV_HANDLE_LISTENING;
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

  assert(req != NULL);
  assert(req->type == UV_READ);
  assert(handle->type == UV_NAMED_PIPE);

  result = ReadFile(handle->handle,
                    &uv_zero_,
                    0,
                    &bytes,
                    NULL);

  if (!result) {
    SET_REQ_ERROR(req, GetLastError());
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
      req->overlapped.hEvent = (HANDLE) ((DWORD) req->event_handle | 1);
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


static int uv_pipe_read_start_impl(uv_pipe_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb, uv_read2_cb read2_cb) {
  uv_loop_t* loop = handle->loop;

  if (!(handle->flags & UV_HANDLE_CONNECTION)) {
    uv__set_artificial_error(loop, UV_EINVAL);
    return -1;
  }

  if (handle->flags & UV_HANDLE_READING) {
    uv__set_artificial_error(loop, UV_EALREADY);
    return -1;
  }

  if (handle->flags & UV_HANDLE_EOF) {
    uv__set_artificial_error(loop, UV_EOF);
    return -1;
  }

  handle->flags |= UV_HANDLE_READING;
  handle->read_cb = read_cb;
  handle->read2_cb = read2_cb;
  handle->alloc_cb = alloc_cb;

  /* If reading was stopped and then started again, there could still be a */
  /* read request pending. */
  if (!(handle->flags & UV_HANDLE_READ_PENDING))
    uv_pipe_queue_read(loop, handle);

  return 0;
}


int uv_pipe_read_start(uv_pipe_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb) {
  return uv_pipe_read_start_impl(handle, alloc_cb, read_cb, NULL);
}


int uv_pipe_read2_start(uv_pipe_t* handle, uv_alloc_cb alloc_cb,
    uv_read2_cb read_cb) {
  return uv_pipe_read_start_impl(handle, alloc_cb, NULL, read_cb);
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


static int uv_pipe_write_impl(uv_loop_t* loop, uv_write_t* req,
    uv_pipe_t* handle, uv_buf_t bufs[], int bufcnt,
    uv_stream_t* send_handle, uv_write_cb cb) {
  int result;
  uv_tcp_t* tcp_send_handle;
  uv_write_t* ipc_header_req;
  uv_ipc_frame_uv_stream ipc_frame;

  if (bufcnt != 1 && (bufcnt != 0 || !send_handle)) {
    uv__set_artificial_error(loop, UV_ENOTSUP);
    return -1;
  }

  /* Only TCP server handles are supported for sharing. */
  if (send_handle && (send_handle->type != UV_TCP ||
      send_handle->flags & UV_HANDLE_CONNECTION)) {
    uv__set_artificial_error(loop, UV_ENOTSUP);
    return -1;
  }

  assert(handle->handle != INVALID_HANDLE_VALUE);

  if (!(handle->flags & UV_HANDLE_CONNECTION)) {
    uv__set_artificial_error(loop, UV_EINVAL);
    return -1;
  }

  if (handle->flags & UV_HANDLE_SHUTTING) {
    uv__set_artificial_error(loop, UV_EOF);
    return -1;
  }

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

      if (uv_tcp_duplicate_socket(tcp_send_handle, handle->ipc_pid,
          &ipc_frame.socket_info)) {
        return -1;
      }
      ipc_frame.header.flags |= UV_IPC_UV_STREAM;
    }

    if (bufcnt == 1) {
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

    result = WriteFile(handle->handle,
                        &ipc_frame,
                        ipc_frame.header.flags & UV_IPC_UV_STREAM ?
                          sizeof(ipc_frame) : sizeof(ipc_frame.header),
                        NULL,
                        &ipc_header_req->overlapped);
    if (!result && GetLastError() != ERROR_IO_PENDING) {
      uv__set_sys_error(loop, GetLastError());
      return -1;
    }

    if (result) {
      /* Request completed immediately. */
      ipc_header_req->queued_bytes = 0;
    } else {
      /* Request queued by the kernel. */
      ipc_header_req->queued_bytes = ipc_frame.header.flags & UV_IPC_UV_STREAM ?
        sizeof(ipc_frame) : sizeof(ipc_frame.header);
      handle->write_queue_size += req->queued_bytes;
    }

    if (handle->write_reqs_pending == 0) {
      uv_ref(loop);
    }

    handle->reqs_pending++;
    handle->write_reqs_pending++;

    /* If we don't have any raw data to write - we're done. */
    if (!(ipc_frame.header.flags & UV_IPC_RAW_DATA)) {
      return 0;
    }
  }

  if (handle->flags & UV_HANDLE_NON_OVERLAPPED_PIPE) {
    req->write_buffer = bufs[0];
    uv_insert_non_overlapped_write_req(handle, req);
    if (handle->write_reqs_pending == 0) {
      uv_queue_non_overlapped_write(handle);
    }

    /* Request queued by the kernel. */
    req->queued_bytes = uv_count_bufs(bufs, bufcnt);
    handle->write_queue_size += req->queued_bytes;
  } else {
    result = WriteFile(handle->handle,
                       bufs[0].base,
                       bufs[0].len,
                       NULL,
                       &req->overlapped);

    if (!result && GetLastError() != ERROR_IO_PENDING) {
      uv__set_sys_error(loop, GetLastError());
      return -1;
    }

    if (result) {
      /* Request completed immediately. */
      req->queued_bytes = 0;
    } else {
      /* Request queued by the kernel. */
      req->queued_bytes = uv_count_bufs(bufs, bufcnt);
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
        uv__set_sys_error(loop, GetLastError());
        return -1;
      }
    }
  }

  if (handle->write_reqs_pending == 0) {
    uv_ref(loop);
  }

  handle->reqs_pending++;
  handle->write_reqs_pending++;

  return 0;
}


int uv_pipe_write(uv_loop_t* loop, uv_write_t* req, uv_pipe_t* handle,
    uv_buf_t bufs[], int bufcnt, uv_write_cb cb) {
  return uv_pipe_write_impl(loop, req, handle, bufs, bufcnt, NULL, cb);
}


int uv_pipe_write2(uv_loop_t* loop, uv_write_t* req, uv_pipe_t* handle,
    uv_buf_t bufs[], int bufcnt, uv_stream_t* send_handle, uv_write_cb cb) {
  if (!handle->ipc) {
    uv__set_artificial_error(loop, UV_EINVAL);
    return -1;
  }

  return uv_pipe_write_impl(loop, req, handle, bufs, bufcnt, send_handle, cb);
}


static void uv_pipe_read_eof(uv_loop_t* loop, uv_pipe_t* handle,
    uv_buf_t buf) {
  /* If there is an eof timer running, we don't need it any more, */
  /* so discard it. */
  eof_timer_destroy(handle);

  handle->flags |= UV_HANDLE_EOF;
  uv_read_stop((uv_stream_t*) handle);

  uv__set_artificial_error(loop, UV_EOF);
  if (handle->read2_cb) {
    handle->read2_cb(handle, -1, uv_null_buf_, UV_UNKNOWN_HANDLE);
  } else {
    handle->read_cb((uv_stream_t*) handle, -1, uv_null_buf_);
  }
}


static void uv_pipe_read_error(uv_loop_t* loop, uv_pipe_t* handle, int error,
    uv_buf_t buf) {
  /* If there is an eof timer running, we don't need it any more, */
  /* so discard it. */
  eof_timer_destroy(handle);

  uv_read_stop((uv_stream_t*) handle);

  uv__set_sys_error(loop, error);
  if (handle->read2_cb) {
    handle->read2_cb(handle, -1, buf, UV_UNKNOWN_HANDLE);
  } else {
    handle->read_cb((uv_stream_t*)handle, -1, buf);
  }
}


static void uv_pipe_read_error_or_eof(uv_loop_t* loop, uv_pipe_t* handle,
    int error, uv_buf_t buf) {
  if (error == ERROR_BROKEN_PIPE) {
    uv_pipe_read_eof(loop, handle, buf);
  } else {
    uv_pipe_read_error(loop, handle, error, buf);
  }
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
          assert(ipc_frame.header.flags <= (UV_IPC_UV_STREAM | UV_IPC_RAW_DATA));

          if (ipc_frame.header.flags & UV_IPC_UV_STREAM) {
            assert(avail - sizeof(ipc_frame.header) >=
              sizeof(ipc_frame.socket_info));

            /* Read the TCP socket info. */
            if (!ReadFile(handle->handle,
                          &ipc_frame.socket_info,
                          sizeof(ipc_frame) - sizeof(ipc_frame.header),
                          &bytes,
                          NULL)) {
              uv_pipe_read_error_or_eof(loop, handle, GetLastError(),
                uv_null_buf_);
              break;
            }

            assert(bytes == sizeof(ipc_frame) - sizeof(ipc_frame.header));

            /* Store the pending socket info. */
            assert(!handle->pending_socket_info);
            handle->pending_socket_info =
              (WSAPROTOCOL_INFOW*)malloc(sizeof(*(handle->pending_socket_info)));
            if (!handle->pending_socket_info) {
              uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
            }

            *(handle->pending_socket_info) = ipc_frame.socket_info;
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

      buf = handle->alloc_cb((uv_handle_t*) handle, avail);
      assert(buf.len > 0);

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
          if (handle->read2_cb) {
            handle->read2_cb(handle, bytes, buf,
              handle->pending_socket_info ? UV_TCP : UV_UNKNOWN_HANDLE);
          } else if (handle->read_cb) {
            handle->read_cb((uv_stream_t*)handle, bytes, buf);
          }

          if (handle->pending_socket_info) {
            free(handle->pending_socket_info);
            handle->pending_socket_info = NULL;
          }
        } else {
          handle->read_cb((uv_stream_t*)handle, bytes, buf);
        }

        /* Read again only if bytes == buf.len */
        if (bytes <= buf.len) {
          break;
        }
      } else {
        uv_pipe_read_error_or_eof(loop, handle, GetLastError(), uv_null_buf_);
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
  assert(handle->type == UV_NAMED_PIPE);

  assert(handle->write_queue_size >= req->queued_bytes);
  handle->write_queue_size -= req->queued_bytes;

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
      if (!REQ_SUCCESS(req)) {
        uv__set_sys_error(loop, GET_REQ_ERROR(req));
        ((uv_write_cb)req->cb)(req, -1);
      } else {
        ((uv_write_cb)req->cb)(req, 0);
      }
    }
  }

  handle->write_reqs_pending--;

  if (handle->flags & UV_HANDLE_NON_OVERLAPPED_PIPE &&
      handle->non_overlapped_writes_tail) {
    assert(handle->write_reqs_pending > 0);
    uv_queue_non_overlapped_write(handle);
  }

  if (handle->write_reqs_pending == 0) {
    uv_unref(loop);
  }

  if (handle->write_reqs_pending == 0 &&
      handle->flags & UV_HANDLE_SHUTTING) {
    uv_want_endgame(loop, (uv_handle_t*)handle);
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_pipe_accept_req(uv_loop_t* loop, uv_pipe_t* handle,
    uv_req_t* raw_req) {
  uv_pipe_accept_t* req = (uv_pipe_accept_t*) raw_req;

  assert(handle->type == UV_NAMED_PIPE);

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
    if (!(handle->flags & UV_HANDLE_CLOSING)) {
      uv_pipe_queue_accept(loop, handle, req, FALSE);
    }
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_pipe_connect_req(uv_loop_t* loop, uv_pipe_t* handle,
    uv_connect_t* req) {
  assert(handle->type == UV_NAMED_PIPE);

  if (req->cb) {
    if (REQ_SUCCESS(req)) {
      uv_pipe_connection_init(handle);
      ((uv_connect_cb)req->cb)(req, 0);
    } else {
      uv__set_sys_error(loop, GET_REQ_ERROR(req));
      ((uv_connect_cb)req->cb)(req, -1);
    }
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_pipe_shutdown_req(uv_loop_t* loop, uv_pipe_t* handle,
    uv_shutdown_t* req) {
  assert(handle->type == UV_NAMED_PIPE);

  /* Initialize and optionally start the eof timer. */
  /* This makes no sense if we've already seen EOF. */
  if (!(handle->flags & UV_HANDLE_EOF)) {
    eof_timer_init(handle);

    /* If reading start the timer right now. */
    /* Otherwise uv_pipe_queue_read will start it. */
    if (handle->flags & UV_HANDLE_READ_PENDING) {
      eof_timer_start(handle);
    }
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


static void eof_timer_cb(uv_timer_t* timer, int status) {
  uv_pipe_t* pipe = (uv_pipe_t*) timer->data;
  uv_loop_t* loop = timer->loop;

  assert(status == 0); /* timers can't fail */
  assert(pipe->type == UV_NAMED_PIPE);

  /* This should always be true, since we start the timer only */
  /* in uv_pipe_queue_read after successfully calling ReadFile, */
  /* or in uv_process_pipe_shutdown_req if a read is pending, */
  /* and we always immediately stop the timer in */
  /* uv_process_pipe_read_req. */
  assert(pipe->flags & UV_HANDLE_READ_PENDING) ;

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
  assert(pipe->flags && UV_HANDLE_CONNECTION);

  if (pipe->eof_timer) {
    uv_close((uv_handle_t*) pipe->eof_timer, eof_timer_close_cb);
    pipe->eof_timer = NULL;
  }
}


static void eof_timer_close_cb(uv_handle_t* handle) {
  assert(handle->type == UV_TIMER);
  free(handle);
}


void uv_pipe_open(uv_pipe_t* pipe, uv_file file) {
  HANDLE os_handle = (HANDLE)_get_osfhandle(file);

  if (os_handle == INVALID_HANDLE_VALUE ||
      uv_set_pipe_handle(pipe->loop, pipe, os_handle) == -1) {
    return;
  }

  uv_pipe_connection_init(pipe);
  pipe->handle = os_handle;

  if (pipe->ipc) {
    assert(!(pipe->flags & UV_HANDLE_NON_OVERLAPPED_PIPE));
    pipe->ipc_pid = uv_parent_pid();
    assert(pipe->ipc_pid != -1);
  }
}
