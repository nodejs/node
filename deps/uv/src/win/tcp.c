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

#include "uv.h"
#include "internal.h"
#include "handle-inl.h"
#include "stream-inl.h"
#include "req-inl.h"


/*
 * Threshold of active tcp streams for which to preallocate tcp read buffers.
 * (Due to node slab allocator performing poorly under this pattern,
 *  the optimization is temporarily disabled (threshold=0).  This will be
 *  revisited once node allocator is improved.)
 */
const unsigned int uv_active_tcp_streams_threshold = 0;

/*
 * Number of simultaneous pending AcceptEx calls.
 */
const unsigned int uv_simultaneous_server_accepts = 32;

/* A zero-size buffer for use by uv_tcp_read */
static char uv_zero_[] = "";

static int uv__tcp_nodelay(uv_tcp_t* handle, SOCKET socket, int enable) {
  if (setsockopt(socket,
                 IPPROTO_TCP,
                 TCP_NODELAY,
                 (const char*)&enable,
                 sizeof enable) == -1) {
    uv__set_sys_error(handle->loop, WSAGetLastError());
    return -1;
  }
  return 0;
}


static int uv__tcp_keepalive(uv_tcp_t* handle, SOCKET socket, int enable, unsigned int delay) {
  if (setsockopt(socket,
                 SOL_SOCKET,
                 SO_KEEPALIVE,
                 (const char*)&enable,
                 sizeof enable) == -1) {
    uv__set_sys_error(handle->loop, WSAGetLastError());
    return -1;
  }

  if (enable && setsockopt(socket,
                           IPPROTO_TCP,
                           TCP_KEEPALIVE,
                           (const char*)&delay,
                           sizeof delay) == -1) {
    uv__set_sys_error(handle->loop, WSAGetLastError());
    return -1;
  }

  return 0;
}


static int uv_tcp_set_socket(uv_loop_t* loop, uv_tcp_t* handle,
    SOCKET socket, int family, int imported) {
  DWORD yes = 1;
  int non_ifs_lsp;

  assert(handle->socket == INVALID_SOCKET);

  /* Set the socket to nonblocking mode */
  if (ioctlsocket(socket, FIONBIO, &yes) == SOCKET_ERROR) {
    uv__set_sys_error(loop, WSAGetLastError());
    return -1;
  }

  /* Associate it with the I/O completion port. */
  /* Use uv_handle_t pointer as completion key. */
  if (CreateIoCompletionPort((HANDLE)socket,
                             loop->iocp,
                             (ULONG_PTR)socket,
                             0) == NULL) {
    if (imported) {
      handle->flags |= UV_HANDLE_EMULATE_IOCP;
    } else {
      uv__set_sys_error(loop, GetLastError());
      return -1;
    }
  }

  if (family == AF_INET6) {
    non_ifs_lsp = uv_tcp_non_ifs_lsp_ipv6;
  } else {
    non_ifs_lsp = uv_tcp_non_ifs_lsp_ipv4;
  }

  if (pSetFileCompletionNotificationModes &&
      !(handle->flags & UV_HANDLE_EMULATE_IOCP) && !non_ifs_lsp) {
    if (pSetFileCompletionNotificationModes((HANDLE) socket,
        FILE_SKIP_SET_EVENT_ON_HANDLE |
        FILE_SKIP_COMPLETION_PORT_ON_SUCCESS)) {
      handle->flags |= UV_HANDLE_SYNC_BYPASS_IOCP;
    } else if (GetLastError() != ERROR_INVALID_FUNCTION) {
      uv__set_sys_error(loop, GetLastError());
      return -1;
    }
  }

  if ((handle->flags & UV_HANDLE_TCP_NODELAY) &&
      uv__tcp_nodelay(handle, socket, 1)) {
    return -1;
  }

  /* TODO: Use stored delay. */
  if ((handle->flags & UV_HANDLE_TCP_KEEPALIVE) &&
      uv__tcp_keepalive(handle, socket, 1, 60)) {
    return -1;
  }

  handle->socket = socket;

  if (family == AF_INET6) {
    handle->flags |= UV_HANDLE_IPV6;
  } else {
    assert(!(handle->flags & UV_HANDLE_IPV6));
  }

  return 0;
}


int uv_tcp_init(uv_loop_t* loop, uv_tcp_t* handle) {
  uv_stream_init(loop, (uv_stream_t*) handle, UV_TCP);

  handle->accept_reqs = NULL;
  handle->pending_accepts = NULL;
  handle->socket = INVALID_SOCKET;
  handle->reqs_pending = 0;
  handle->func_acceptex = NULL;
  handle->func_connectex = NULL;
  handle->processed_accepts = 0;

  return 0;
}


void uv_tcp_endgame(uv_loop_t* loop, uv_tcp_t* handle) {
  int status;
  unsigned int i;
  uv_tcp_accept_t* req;

  if (handle->flags & UV_HANDLE_CONNECTION &&
      handle->shutdown_req != NULL &&
      handle->write_reqs_pending == 0) {

    UNREGISTER_HANDLE_REQ(loop, handle, handle->shutdown_req);

    if (handle->flags & UV__HANDLE_CLOSING) {
      status = -1;
      uv__set_artificial_error(loop, UV_ECANCELED);
    } else if (shutdown(handle->socket, SD_SEND) != SOCKET_ERROR) {
      status = 0;
    } else {
      status = -1;
      uv__set_sys_error(loop, WSAGetLastError());
    }

    if (handle->shutdown_req->cb) {
      handle->shutdown_req->cb(handle->shutdown_req, status);
    }

    handle->shutdown_req = NULL;
    DECREASE_PENDING_REQ_COUNT(handle);
    return;
  }

  if (handle->flags & UV__HANDLE_CLOSING &&
      handle->reqs_pending == 0) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));

    if (!(handle->flags & UV_HANDLE_TCP_SOCKET_CLOSED)) {
      closesocket(handle->socket);
      handle->flags |= UV_HANDLE_TCP_SOCKET_CLOSED;
    }

    if (!(handle->flags & UV_HANDLE_CONNECTION) && handle->accept_reqs) {
      if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
        for (i = 0; i < uv_simultaneous_server_accepts; i++) {
          req = &handle->accept_reqs[i];
          if (req->wait_handle != INVALID_HANDLE_VALUE) {
            UnregisterWait(req->wait_handle);
            req->wait_handle = INVALID_HANDLE_VALUE;
          }
          if (req->event_handle) {
            CloseHandle(req->event_handle);
            req->event_handle = NULL;
          }
        }
      }

      free(handle->accept_reqs);
      handle->accept_reqs = NULL;
    }

    if (handle->flags & UV_HANDLE_CONNECTION &&
        handle->flags & UV_HANDLE_EMULATE_IOCP) {
      if (handle->read_req.wait_handle != INVALID_HANDLE_VALUE) {
        UnregisterWait(handle->read_req.wait_handle);
        handle->read_req.wait_handle = INVALID_HANDLE_VALUE;
      }
      if (handle->read_req.event_handle) {
        CloseHandle(handle->read_req.event_handle);
        handle->read_req.event_handle = NULL;
      }
    }

    uv__handle_close(handle);
    loop->active_tcp_streams--;
  }
}


static int uv__bind(uv_tcp_t* handle,
                    int family,
                    struct sockaddr* addr,
                    int addrsize) {
  DWORD err;
  int r;

  if (handle->socket == INVALID_SOCKET) {
    SOCKET sock = socket(family, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
      uv__set_sys_error(handle->loop, WSAGetLastError());
      return -1;
    }

    /* Make the socket non-inheritable */
    if (!SetHandleInformation((HANDLE) sock, HANDLE_FLAG_INHERIT, 0)) {
      uv__set_sys_error(handle->loop, GetLastError());
      closesocket(sock);
      return -1;
    }

    if (uv_tcp_set_socket(handle->loop, handle, sock, family, 0) < 0) {
      closesocket(sock);
      return -1;
    }
  }

  r = bind(handle->socket, addr, addrsize);

  if (r == SOCKET_ERROR) {
    err = WSAGetLastError();
    if (err == WSAEADDRINUSE) {
      /* Some errors are not to be reported until connect() or listen() */
      handle->bind_error = err;
      handle->flags |= UV_HANDLE_BIND_ERROR;
    } else {
      uv__set_sys_error(handle->loop, err);
      return -1;
    }
  }

  handle->flags |= UV_HANDLE_BOUND;

  return 0;
}


int uv__tcp_bind(uv_tcp_t* handle, struct sockaddr_in addr) {
  return uv__bind(handle,
                  AF_INET,
                  (struct sockaddr*)&addr,
                  sizeof(struct sockaddr_in));
}


int uv__tcp_bind6(uv_tcp_t* handle, struct sockaddr_in6 addr) {
  return uv__bind(handle,
                  AF_INET6,
                  (struct sockaddr*)&addr,
                  sizeof(struct sockaddr_in6));
}


static void CALLBACK post_completion(void* context, BOOLEAN timed_out) {
  uv_req_t* req;
  uv_tcp_t* handle;

  req = (uv_req_t*) context;
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


static void CALLBACK post_write_completion(void* context, BOOLEAN timed_out) {
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


static void uv_tcp_queue_accept(uv_tcp_t* handle, uv_tcp_accept_t* req) {
  uv_loop_t* loop = handle->loop;
  BOOL success;
  DWORD bytes;
  SOCKET accept_socket;
  short family;

  assert(handle->flags & UV_HANDLE_LISTENING);
  assert(req->accept_socket == INVALID_SOCKET);

  /* choose family and extension function */
  if (handle->flags & UV_HANDLE_IPV6) {
    family = AF_INET6;
  } else {
    family = AF_INET;
  }

  /* Open a socket for the accepted connection. */
  accept_socket = socket(family, SOCK_STREAM, 0);
  if (accept_socket == INVALID_SOCKET) {
    SET_REQ_ERROR(req, WSAGetLastError());
    uv_insert_pending_req(loop, (uv_req_t*)req);
    handle->reqs_pending++;
    return;
  }

  /* Make the socket non-inheritable */
  if (!SetHandleInformation((HANDLE) accept_socket, HANDLE_FLAG_INHERIT, 0)) {
    SET_REQ_ERROR(req, GetLastError());
    uv_insert_pending_req(loop, (uv_req_t*)req);
    handle->reqs_pending++;
    closesocket(accept_socket);
    return;
  }

  /* Prepare the overlapped structure. */
  memset(&(req->overlapped), 0, sizeof(req->overlapped));
  if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
    req->overlapped.hEvent = (HANDLE) ((ULONG_PTR) req->event_handle | 1);
  }

  success = handle->func_acceptex(handle->socket,
                                  accept_socket,
                                  (void*)req->accept_buffer,
                                  0,
                                  sizeof(struct sockaddr_storage),
                                  sizeof(struct sockaddr_storage),
                                  &bytes,
                                  &req->overlapped);

  if (UV_SUCCEEDED_WITHOUT_IOCP(success)) {
    /* Process the req without IOCP. */
    req->accept_socket = accept_socket;
    handle->reqs_pending++;
    uv_insert_pending_req(loop, (uv_req_t*)req);
  } else if (UV_SUCCEEDED_WITH_IOCP(success)) {
    /* The req will be processed with IOCP. */
    req->accept_socket = accept_socket;
    handle->reqs_pending++;
    if (handle->flags & UV_HANDLE_EMULATE_IOCP &&
        req->wait_handle == INVALID_HANDLE_VALUE &&
        !RegisterWaitForSingleObject(&req->wait_handle,
          req->event_handle, post_completion, (void*) req,
          INFINITE, WT_EXECUTEINWAITTHREAD)) {
      SET_REQ_ERROR(req, GetLastError());
      uv_insert_pending_req(loop, (uv_req_t*)req);
      handle->reqs_pending++;
      return;
    }
  } else {
    /* Make this req pending reporting an error. */
    SET_REQ_ERROR(req, WSAGetLastError());
    uv_insert_pending_req(loop, (uv_req_t*)req);
    handle->reqs_pending++;
    /* Destroy the preallocated client socket. */
    closesocket(accept_socket);
    /* Destroy the event handle */
    if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
      CloseHandle(req->overlapped.hEvent);
      req->event_handle = NULL;
    }
  }
}


static void uv_tcp_queue_read(uv_loop_t* loop, uv_tcp_t* handle) {
  uv_read_t* req;
  uv_buf_t buf;
  int result;
  DWORD bytes, flags;

  assert(handle->flags & UV_HANDLE_READING);
  assert(!(handle->flags & UV_HANDLE_READ_PENDING));

  req = &handle->read_req;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

  /*
   * Preallocate a read buffer if the number of active streams is below
   * the threshold.
  */
  if (loop->active_tcp_streams < uv_active_tcp_streams_threshold) {
    handle->flags &= ~UV_HANDLE_ZERO_READ;
    handle->read_buffer = handle->alloc_cb((uv_handle_t*) handle, 65536);
    assert(handle->read_buffer.len > 0);
    buf = handle->read_buffer;
  } else {
    handle->flags |= UV_HANDLE_ZERO_READ;
    buf.base = (char*) &uv_zero_;
    buf.len = 0;
  }

  /* Prepare the overlapped structure. */
  memset(&(req->overlapped), 0, sizeof(req->overlapped));
  if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
    assert(req->event_handle);
    req->overlapped.hEvent = (HANDLE) ((ULONG_PTR) req->event_handle | 1);
  }

  flags = 0;
  result = WSARecv(handle->socket,
                   (WSABUF*)&buf,
                   1,
                   &bytes,
                   &flags,
                   &req->overlapped,
                   NULL);

  if (UV_SUCCEEDED_WITHOUT_IOCP(result == 0)) {
    /* Process the req without IOCP. */
    handle->flags |= UV_HANDLE_READ_PENDING;
    req->overlapped.InternalHigh = bytes;
    handle->reqs_pending++;
    uv_insert_pending_req(loop, (uv_req_t*)req);
  } else if (UV_SUCCEEDED_WITH_IOCP(result == 0)) {
    /* The req will be processed with IOCP. */
    handle->flags |= UV_HANDLE_READ_PENDING;
    handle->reqs_pending++;
    if (handle->flags & UV_HANDLE_EMULATE_IOCP &&
        req->wait_handle == INVALID_HANDLE_VALUE &&
        !RegisterWaitForSingleObject(&req->wait_handle,
          req->event_handle, post_completion, (void*) req,
          INFINITE, WT_EXECUTEINWAITTHREAD)) {
      SET_REQ_ERROR(req, GetLastError());
      uv_insert_pending_req(loop, (uv_req_t*)req);
    }
  } else {
    /* Make this req pending reporting an error. */
    SET_REQ_ERROR(req, WSAGetLastError());
    uv_insert_pending_req(loop, (uv_req_t*)req);
    handle->reqs_pending++;
  }
}


int uv_tcp_listen(uv_tcp_t* handle, int backlog, uv_connection_cb cb) {
  uv_loop_t* loop = handle->loop;
  unsigned int i, simultaneous_accepts;
  uv_tcp_accept_t* req;

  assert(backlog > 0);

  if (handle->flags & UV_HANDLE_LISTENING) {
    handle->connection_cb = cb;
  }

  if (handle->flags & UV_HANDLE_READING) {
    uv__set_artificial_error(loop, UV_EISCONN);
    return -1;
  }

  if (handle->flags & UV_HANDLE_BIND_ERROR) {
    uv__set_sys_error(loop, handle->bind_error);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_tcp_bind(handle, uv_addr_ip4_any_) < 0)
    return -1;

  if (!handle->func_acceptex) {
    if(!uv_get_acceptex_function(handle->socket, &handle->func_acceptex)) {
      uv__set_sys_error(loop, WSAEAFNOSUPPORT);
      return -1;
    }
  }

  if (!(handle->flags & UV_HANDLE_SHARED_TCP_SOCKET) &&
      listen(handle->socket, backlog) == SOCKET_ERROR) {
    uv__set_sys_error(loop, WSAGetLastError());
    return -1;
  }

  handle->flags |= UV_HANDLE_LISTENING;
  handle->connection_cb = cb;
  INCREASE_ACTIVE_COUNT(loop, handle);

  simultaneous_accepts = handle->flags & UV_HANDLE_TCP_SINGLE_ACCEPT ? 1
    : uv_simultaneous_server_accepts;

  if(!handle->accept_reqs) {
    handle->accept_reqs = (uv_tcp_accept_t*)
      malloc(uv_simultaneous_server_accepts * sizeof(uv_tcp_accept_t));
    if (!handle->accept_reqs) {
      uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
    }

    for (i = 0; i < simultaneous_accepts; i++) {
      req = &handle->accept_reqs[i];
      uv_req_init(loop, (uv_req_t*)req);
      req->type = UV_ACCEPT;
      req->accept_socket = INVALID_SOCKET;
      req->data = handle;

      req->wait_handle = INVALID_HANDLE_VALUE;
      if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
        req->event_handle = CreateEvent(NULL, 0, 0, NULL);
        if (!req->event_handle) {
          uv_fatal_error(GetLastError(), "CreateEvent");
        }
      } else {
        req->event_handle = NULL;
      }

      uv_tcp_queue_accept(handle, req);
    }

    /* Initialize other unused requests too, because uv_tcp_endgame */
    /* doesn't know how how many requests were intialized, so it will */
    /* try to clean up {uv_simultaneous_server_accepts} requests. */
    for (i = simultaneous_accepts; i < uv_simultaneous_server_accepts; i++) {
      req = &handle->accept_reqs[i];
      uv_req_init(loop, (uv_req_t*) req);
      req->type = UV_ACCEPT;
      req->accept_socket = INVALID_SOCKET;
      req->data = handle;
      req->wait_handle = INVALID_HANDLE_VALUE;
    }
  }

  return 0;
}


int uv_tcp_accept(uv_tcp_t* server, uv_tcp_t* client) {
  uv_loop_t* loop = server->loop;
  int rv = 0;
  int family;

  uv_tcp_accept_t* req = server->pending_accepts;

  if (!req) {
    /* No valid connections found, so we error out. */
    uv__set_sys_error(loop, WSAEWOULDBLOCK);
    return -1;
  }

  if (req->accept_socket == INVALID_SOCKET) {
    uv__set_sys_error(loop, WSAENOTCONN);
    return -1;
  }

  if (server->flags & UV_HANDLE_IPV6) {
    family = AF_INET6;
  } else {
    family = AF_INET;
  }

  if (uv_tcp_set_socket(client->loop,
                        client,
                        req->accept_socket,
                        family,
                        0) < 0) {
    closesocket(req->accept_socket);
    rv = -1;
  } else {
    uv_connection_init((uv_stream_t*) client);
    /* AcceptEx() implicitly binds the accepted socket. */
    client->flags |= UV_HANDLE_BOUND | UV_HANDLE_READABLE | UV_HANDLE_WRITABLE;
  }

  /* Prepare the req to pick up a new connection */
  server->pending_accepts = req->next_pending;
  req->next_pending = NULL;
  req->accept_socket = INVALID_SOCKET;

  if (!(server->flags & UV__HANDLE_CLOSING)) {
    /* Check if we're in a middle of changing the number of pending accepts. */
    if (!(server->flags & UV_HANDLE_TCP_ACCEPT_STATE_CHANGING)) {
      uv_tcp_queue_accept(server, req);
    } else {
      /* We better be switching to a single pending accept. */
      assert(server->flags & UV_HANDLE_TCP_SINGLE_ACCEPT);

      server->processed_accepts++;

      if (server->processed_accepts >= uv_simultaneous_server_accepts) {
        server->processed_accepts = 0;
        /*
         * All previously queued accept requests are now processed.
         * We now switch to queueing just a single accept.
         */
        uv_tcp_queue_accept(server, &server->accept_reqs[0]);
        server->flags &= ~UV_HANDLE_TCP_ACCEPT_STATE_CHANGING;
        server->flags |= UV_HANDLE_TCP_SINGLE_ACCEPT;
      }
    }
  }

  loop->active_tcp_streams++;

  return rv;
}


int uv_tcp_read_start(uv_tcp_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb) {
  uv_loop_t* loop = handle->loop;

  handle->flags |= UV_HANDLE_READING;
  handle->read_cb = read_cb;
  handle->alloc_cb = alloc_cb;
  INCREASE_ACTIVE_COUNT(loop, handle);

  /* If reading was stopped and then started again, there could still be a */
  /* read request pending. */
  if (!(handle->flags & UV_HANDLE_READ_PENDING)) {
    if (handle->flags & UV_HANDLE_EMULATE_IOCP &&
        !handle->read_req.event_handle) {
      handle->read_req.event_handle = CreateEvent(NULL, 0, 0, NULL);
      if (!handle->read_req.event_handle) {
        uv_fatal_error(GetLastError(), "CreateEvent");
      }
    }
    uv_tcp_queue_read(loop, handle);
  }

  return 0;
}


int uv__tcp_connect(uv_connect_t* req,
                    uv_tcp_t* handle,
                    struct sockaddr_in address,
                    uv_connect_cb cb) {
  uv_loop_t* loop = handle->loop;
  int addrsize = sizeof(struct sockaddr_in);
  BOOL success;
  DWORD bytes;

  if (handle->flags & UV_HANDLE_BIND_ERROR) {
    uv__set_sys_error(loop, handle->bind_error);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_tcp_bind(handle, uv_addr_ip4_any_) < 0)
    return -1;

  if (!handle->func_connectex) {
    if(!uv_get_connectex_function(handle->socket, &handle->func_connectex)) {
      uv__set_sys_error(loop, WSAEAFNOSUPPORT);
      return -1;
    }
  }

  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_CONNECT;
  req->handle = (uv_stream_t*) handle;
  req->cb = cb;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

  success = handle->func_connectex(handle->socket,
                                   (struct sockaddr*) &address,
                                   addrsize,
                                   NULL,
                                   0,
                                   &bytes,
                                   &req->overlapped);

  if (UV_SUCCEEDED_WITHOUT_IOCP(success)) {
    /* Process the req without IOCP. */
    handle->reqs_pending++;
    REGISTER_HANDLE_REQ(loop, handle, req);
    uv_insert_pending_req(loop, (uv_req_t*)req);
  } else if (UV_SUCCEEDED_WITH_IOCP(success)) {
    /* The req will be processed with IOCP. */
    handle->reqs_pending++;
    REGISTER_HANDLE_REQ(loop, handle, req);
  } else {
    uv__set_sys_error(loop, WSAGetLastError());
    return -1;
  }

  return 0;
}


int uv__tcp_connect6(uv_connect_t* req,
                     uv_tcp_t* handle,
                     struct sockaddr_in6 address,
                     uv_connect_cb cb) {
  uv_loop_t* loop = handle->loop;
  int addrsize = sizeof(struct sockaddr_in6);
  BOOL success;
  DWORD bytes;

  if (handle->flags & UV_HANDLE_BIND_ERROR) {
    uv__set_sys_error(loop, handle->bind_error);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_tcp_bind6(handle, uv_addr_ip6_any_) < 0)
    return -1;

  if (!handle->func_connectex) {
    if(!uv_get_connectex_function(handle->socket, &handle->func_connectex)) {
      uv__set_sys_error(loop, WSAEAFNOSUPPORT);
      return -1;
    }
  }

  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_CONNECT;
  req->handle = (uv_stream_t*) handle;
  req->cb = cb;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

  success = handle->func_connectex(handle->socket,
                                   (struct sockaddr*) &address,
                                   addrsize,
                                   NULL,
                                   0,
                                   &bytes,
                                   &req->overlapped);

  if (UV_SUCCEEDED_WITHOUT_IOCP(success)) {
    handle->reqs_pending++;
    REGISTER_HANDLE_REQ(loop, handle, req);
    uv_insert_pending_req(loop, (uv_req_t*)req);
  } else if (UV_SUCCEEDED_WITH_IOCP(success)) {
    handle->reqs_pending++;
    REGISTER_HANDLE_REQ(loop, handle, req);
  } else {
    uv__set_sys_error(loop, WSAGetLastError());
    return -1;
  }

  return 0;
}


int uv_tcp_getsockname(uv_tcp_t* handle, struct sockaddr* name,
    int* namelen) {
  uv_loop_t* loop = handle->loop;
  int result;

  if (!(handle->flags & UV_HANDLE_BOUND)) {
    uv__set_sys_error(loop, WSAEINVAL);
    return -1;
  }

  if (handle->flags & UV_HANDLE_BIND_ERROR) {
    uv__set_sys_error(loop, handle->bind_error);
    return -1;
  }

  result = getsockname(handle->socket, name, namelen);
  if (result != 0) {
    uv__set_sys_error(loop, WSAGetLastError());
    return -1;
  }

  return 0;
}


int uv_tcp_getpeername(uv_tcp_t* handle, struct sockaddr* name,
    int* namelen) {
  uv_loop_t* loop = handle->loop;
  int result;

  if (!(handle->flags & UV_HANDLE_BOUND)) {
    uv__set_sys_error(loop, WSAEINVAL);
    return -1;
  }

  if (handle->flags & UV_HANDLE_BIND_ERROR) {
    uv__set_sys_error(loop, handle->bind_error);
    return -1;
  }

  result = getpeername(handle->socket, name, namelen);
  if (result != 0) {
    uv__set_sys_error(loop, WSAGetLastError());
    return -1;
  }

  return 0;
}


int uv_tcp_write(uv_loop_t* loop, uv_write_t* req, uv_tcp_t* handle,
    uv_buf_t bufs[], int bufcnt, uv_write_cb cb) {
  int result;
  DWORD bytes;

  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_WRITE;
  req->handle = (uv_stream_t*) handle;
  req->cb = cb;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

  /* Prepare the overlapped structure. */
  memset(&(req->overlapped), 0, sizeof(req->overlapped));
  if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
    req->event_handle = CreateEvent(NULL, 0, 0, NULL);
    if (!req->event_handle) {
      uv_fatal_error(GetLastError(), "CreateEvent");
    }
    req->overlapped.hEvent = (HANDLE) ((ULONG_PTR) req->event_handle | 1);
    req->wait_handle = INVALID_HANDLE_VALUE;
  }

  result = WSASend(handle->socket,
                   (WSABUF*)bufs,
                   bufcnt,
                   &bytes,
                   0,
                   &req->overlapped,
                   NULL);

  if (UV_SUCCEEDED_WITHOUT_IOCP(result == 0)) {
    /* Request completed immediately. */
    req->queued_bytes = 0;
    handle->reqs_pending++;
    handle->write_reqs_pending++;
    REGISTER_HANDLE_REQ(loop, handle, req);
    uv_insert_pending_req(loop, (uv_req_t*) req);
  } else if (UV_SUCCEEDED_WITH_IOCP(result == 0)) {
    /* Request queued by the kernel. */
    req->queued_bytes = uv_count_bufs(bufs, bufcnt);
    handle->reqs_pending++;
    handle->write_reqs_pending++;
    REGISTER_HANDLE_REQ(loop, handle, req);
    handle->write_queue_size += req->queued_bytes;
    if (handle->flags & UV_HANDLE_EMULATE_IOCP &&
        !RegisterWaitForSingleObject(&req->wait_handle,
          req->event_handle, post_write_completion, (void*) req,
          INFINITE, WT_EXECUTEINWAITTHREAD | WT_EXECUTEONLYONCE)) {
      SET_REQ_ERROR(req, GetLastError());
      uv_insert_pending_req(loop, (uv_req_t*)req);
    }
  } else {
    /* Send failed due to an error. */
    uv__set_sys_error(loop, WSAGetLastError());
    return -1;
  }

  return 0;
}


void uv_process_tcp_read_req(uv_loop_t* loop, uv_tcp_t* handle,
    uv_req_t* req) {
  DWORD bytes, flags, err;
  uv_buf_t buf;

  assert(handle->type == UV_TCP);

  handle->flags &= ~UV_HANDLE_READ_PENDING;

  if (!REQ_SUCCESS(req)) {
    /* An error occurred doing the read. */
    if ((handle->flags & UV_HANDLE_READING) ||
        !(handle->flags & UV_HANDLE_ZERO_READ)) {
      handle->flags &= ~UV_HANDLE_READING;
      DECREASE_ACTIVE_COUNT(loop, handle);
      buf = (handle->flags & UV_HANDLE_ZERO_READ) ?
            uv_buf_init(NULL, 0) : handle->read_buffer;

      err = GET_REQ_SOCK_ERROR(req);

      if (err == WSAECONNABORTED) {
        /*
         * Turn WSAECONNABORTED into UV_ECONNRESET to be consistent with Unix.
         */
        uv__set_error(loop, UV_ECONNRESET, err);
      } else {
        uv__set_sys_error(loop, err);
      }

      handle->read_cb((uv_stream_t*)handle, -1, buf);
    }
  } else {
    if (!(handle->flags & UV_HANDLE_ZERO_READ)) {
      /* The read was done with a non-zero buffer length. */
      if (req->overlapped.InternalHigh > 0) {
        /* Successful read */
        handle->read_cb((uv_stream_t*)handle,
                        req->overlapped.InternalHigh,
                        handle->read_buffer);
        /* Read again only if bytes == buf.len */
        if (req->overlapped.InternalHigh < handle->read_buffer.len) {
          goto done;
        }
      } else {
        /* Connection closed */
        if (handle->flags & UV_HANDLE_READING) {
          handle->flags &= ~UV_HANDLE_READING;
          DECREASE_ACTIVE_COUNT(loop, handle);
        }
        handle->flags &= ~UV_HANDLE_READABLE;

        uv__set_error(loop, UV_EOF, ERROR_SUCCESS);
        buf.base = 0;
        buf.len = 0;
        handle->read_cb((uv_stream_t*)handle, -1, handle->read_buffer);
        goto done;
      }
    }

    /* Do nonblocking reads until the buffer is empty */
    while (handle->flags & UV_HANDLE_READING) {
      buf = handle->alloc_cb((uv_handle_t*) handle, 65536);
      assert(buf.len > 0);
      flags = 0;
      if (WSARecv(handle->socket,
                  (WSABUF*)&buf,
                  1,
                  &bytes,
                  &flags,
                  NULL,
                  NULL) != SOCKET_ERROR) {
        if (bytes > 0) {
          /* Successful read */
          handle->read_cb((uv_stream_t*)handle, bytes, buf);
          /* Read again only if bytes == buf.len */
          if (bytes < buf.len) {
            break;
          }
        } else {
          /* Connection closed */
          handle->flags &= ~(UV_HANDLE_READING | UV_HANDLE_READABLE);
          DECREASE_ACTIVE_COUNT(loop, handle);

          uv__set_error(loop, UV_EOF, ERROR_SUCCESS);
          handle->read_cb((uv_stream_t*)handle, -1, buf);
          break;
        }
      } else {
        err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
          /* Read buffer was completely empty, report a 0-byte read. */
          uv__set_sys_error(loop, WSAEWOULDBLOCK);
          handle->read_cb((uv_stream_t*)handle, 0, buf);
        } else {
          /* Ouch! serious error. */
          handle->flags &= ~UV_HANDLE_READING;
          DECREASE_ACTIVE_COUNT(loop, handle);

          if (err == WSAECONNABORTED) {
            /* Turn WSAECONNABORTED into UV_ECONNRESET to be consistent with */
            /* Unix. */
            uv__set_error(loop, UV_ECONNRESET, err);
          } else {
            uv__set_sys_error(loop, err);
          }

          handle->read_cb((uv_stream_t*)handle, -1, buf);
        }
        break;
      }
    }

done:
    /* Post another read if still reading and not closing. */
    if ((handle->flags & UV_HANDLE_READING) &&
        !(handle->flags & UV_HANDLE_READ_PENDING)) {
      uv_tcp_queue_read(loop, handle);
    }
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_tcp_write_req(uv_loop_t* loop, uv_tcp_t* handle,
    uv_write_t* req) {
  assert(handle->type == UV_TCP);

  assert(handle->write_queue_size >= req->queued_bytes);
  handle->write_queue_size -= req->queued_bytes;

  UNREGISTER_HANDLE_REQ(loop, handle, req);

  if (handle->flags & UV_HANDLE_EMULATE_IOCP) {
    if (req->wait_handle != INVALID_HANDLE_VALUE) {
      UnregisterWait(req->wait_handle);
    }
    if (req->event_handle) {
      CloseHandle(req->event_handle);
    }
  }

  if (req->cb) {
    uv__set_sys_error(loop, GET_REQ_SOCK_ERROR(req));
    ((uv_write_cb)req->cb)(req, loop->last_err.code == UV_OK ? 0 : -1);
  }

  handle->write_reqs_pending--;
  if (handle->shutdown_req != NULL &&
      handle->write_reqs_pending == 0) {
    uv_want_endgame(loop, (uv_handle_t*)handle);
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_tcp_accept_req(uv_loop_t* loop, uv_tcp_t* handle,
    uv_req_t* raw_req) {
  uv_tcp_accept_t* req = (uv_tcp_accept_t*) raw_req;

  assert(handle->type == UV_TCP);

  /* If handle->accepted_socket is not a valid socket, then */
  /* uv_queue_accept must have failed. This is a serious error. We stop */
  /* accepting connections and report this error to the connection */
  /* callback. */
  if (req->accept_socket == INVALID_SOCKET) {
    if (handle->flags & UV_HANDLE_LISTENING) {
      handle->flags &= ~UV_HANDLE_LISTENING;
      DECREASE_ACTIVE_COUNT(loop, handle);
      if (handle->connection_cb) {
        uv__set_sys_error(loop, GET_REQ_SOCK_ERROR(req));
        handle->connection_cb((uv_stream_t*)handle, -1);
      }
    }
  } else if (REQ_SUCCESS(req) &&
      setsockopt(req->accept_socket,
                  SOL_SOCKET,
                  SO_UPDATE_ACCEPT_CONTEXT,
                  (char*)&handle->socket,
                  sizeof(handle->socket)) == 0) {
    req->next_pending = handle->pending_accepts;
    handle->pending_accepts = req;

    /* Accept and SO_UPDATE_ACCEPT_CONTEXT were successful. */
    if (handle->connection_cb) {
      handle->connection_cb((uv_stream_t*)handle, 0);
    }
  } else {
    /* Error related to accepted socket is ignored because the server */
    /* socket may still be healthy. If the server socket is broken
    /* uv_queue_accept will detect it. */
    closesocket(req->accept_socket);
    req->accept_socket = INVALID_SOCKET;
    if (handle->flags & UV_HANDLE_LISTENING) {
      uv_tcp_queue_accept(handle, req);
    }
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_tcp_connect_req(uv_loop_t* loop, uv_tcp_t* handle,
    uv_connect_t* req) {
  assert(handle->type == UV_TCP);

  UNREGISTER_HANDLE_REQ(loop, handle, req);

  if (REQ_SUCCESS(req)) {
    if (setsockopt(handle->socket,
                    SOL_SOCKET,
                    SO_UPDATE_CONNECT_CONTEXT,
                    NULL,
                    0) == 0) {
      uv_connection_init((uv_stream_t*)handle);
      handle->flags |= UV_HANDLE_READABLE | UV_HANDLE_WRITABLE;
      loop->active_tcp_streams++;
      ((uv_connect_cb)req->cb)(req, 0);
    } else {
      uv__set_sys_error(loop, WSAGetLastError());
      ((uv_connect_cb)req->cb)(req, -1);
    }
  } else {
    uv__set_sys_error(loop, GET_REQ_SOCK_ERROR(req));
    ((uv_connect_cb)req->cb)(req, -1);
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


int uv_tcp_import(uv_tcp_t* tcp, WSAPROTOCOL_INFOW* socket_protocol_info,
    int tcp_connection) {
  SOCKET socket = WSASocketW(AF_INET,
                             SOCK_STREAM,
                             IPPROTO_IP,
                             socket_protocol_info,
                             0,
                             WSA_FLAG_OVERLAPPED);

  if (socket == INVALID_SOCKET) {
    uv__set_sys_error(tcp->loop, WSAGetLastError());
    return -1;
  }

  if (!SetHandleInformation((HANDLE) socket, HANDLE_FLAG_INHERIT, 0)) {
    uv__set_sys_error(tcp->loop, GetLastError());
    closesocket(socket);
    return -1;
  }

  if (uv_tcp_set_socket(tcp->loop,
                        tcp,
                        socket,
                        socket_protocol_info->iAddressFamily,
                        1) < 0) {
    closesocket(socket);
    return -1;
  }

  if (tcp_connection) {
    uv_connection_init((uv_stream_t*)tcp);
    tcp->flags |= UV_HANDLE_READABLE | UV_HANDLE_WRITABLE;
  }

  tcp->flags |= UV_HANDLE_BOUND;
  tcp->flags |= UV_HANDLE_SHARED_TCP_SOCKET;

  tcp->loop->active_tcp_streams++;
  return 0;
}


int uv_tcp_nodelay(uv_tcp_t* handle, int enable) {
  if (handle->socket != INVALID_SOCKET &&
      uv__tcp_nodelay(handle, handle->socket, enable)) {
    return -1;
  }

  if (enable) {
    handle->flags |= UV_HANDLE_TCP_NODELAY;
  } else {
    handle->flags &= ~UV_HANDLE_TCP_NODELAY;
  }

  return 0;
}


int uv_tcp_keepalive(uv_tcp_t* handle, int enable, unsigned int delay) {
  if (handle->socket != INVALID_SOCKET &&
      uv__tcp_keepalive(handle, handle->socket, enable, delay)) {
    return -1;
  }

  if (enable) {
    handle->flags |= UV_HANDLE_TCP_KEEPALIVE;
  } else {
    handle->flags &= ~UV_HANDLE_TCP_KEEPALIVE;
  }

  /* TODO: Store delay if handle->socket isn't created yet. */

  return 0;
}


int uv_tcp_duplicate_socket(uv_tcp_t* handle, int pid,
    LPWSAPROTOCOL_INFOW protocol_info) {
  if (!(handle->flags & UV_HANDLE_CONNECTION)) {
    /*
     * We're about to share the socket with another process.  Because
     * this is a listening socket, we assume that the other process will
     * be accepting connections on it.  So, before sharing the socket
     * with another process, we call listen here in the parent process.
     */

    if (!(handle->flags & UV_HANDLE_LISTENING)) {
      if (!(handle->flags & UV_HANDLE_BOUND)) {
        uv__set_artificial_error(handle->loop, UV_EINVAL);
        return -1;
      }
      if (listen(handle->socket, SOMAXCONN) == SOCKET_ERROR) {
        uv__set_sys_error(handle->loop, WSAGetLastError());
        return -1;
      }
    }
  }

  if (WSADuplicateSocketW(handle->socket, pid, protocol_info)) {
    uv__set_sys_error(handle->loop, WSAGetLastError());
    return -1;
  }

  handle->flags |= UV_HANDLE_SHARED_TCP_SOCKET;

  return 0;
}


int uv_tcp_simultaneous_accepts(uv_tcp_t* handle, int enable) {
  if (handle->flags & UV_HANDLE_CONNECTION) {
    uv__set_artificial_error(handle->loop, UV_EINVAL);
    return -1;
  }

  /* Check if we're already in the desired mode. */
  if ((enable && !(handle->flags & UV_HANDLE_TCP_SINGLE_ACCEPT)) ||
      (!enable && handle->flags & UV_HANDLE_TCP_SINGLE_ACCEPT)) {
    return 0;
  }

  /* Don't allow switching from single pending accept to many. */
  if (enable) {
    uv__set_artificial_error(handle->loop, UV_ENOTSUP);
    return -1;
  }

  /* Check if we're in a middle of changing the number of pending accepts. */
  if (handle->flags & UV_HANDLE_TCP_ACCEPT_STATE_CHANGING) {
    return 0;
  }

  handle->flags |= UV_HANDLE_TCP_SINGLE_ACCEPT;

  /* Flip the changing flag if we have already queued multiple accepts. */
  if (handle->flags & UV_HANDLE_LISTENING) {
    handle->flags |= UV_HANDLE_TCP_ACCEPT_STATE_CHANGING;
  }

  return 0;
}


static int uv_tcp_try_cancel_io(uv_tcp_t* tcp) {
  SOCKET socket = tcp->socket;
  int non_ifs_lsp;

  /* Check if we have any non-IFS LSPs stacked on top of TCP */
  non_ifs_lsp = (tcp->flags & UV_HANDLE_IPV6) ? uv_tcp_non_ifs_lsp_ipv6 :
                                                uv_tcp_non_ifs_lsp_ipv4;

  /* If there are non-ifs LSPs then try to obtain a base handle for the */
  /* socket. This will always fail on Windows XP/3k. */
  if (non_ifs_lsp) {
    DWORD bytes;
    if (WSAIoctl(socket,
                 SIO_BASE_HANDLE,
                 NULL,
                 0,
                 &socket,
                 sizeof socket,
                 &bytes,
                 NULL,
                 NULL) != 0) {
      /* Failed. We can't do CancelIo. */
      return -1;
    }
  }

  assert(socket != 0 && socket != INVALID_SOCKET);

  if (!CancelIo((HANDLE) socket)) {
    return -1;
  }

  /* It worked. */
  return 0;
}


void uv_tcp_close(uv_loop_t* loop, uv_tcp_t* tcp) {
  int close_socket = 1;

  if (tcp->flags & UV_HANDLE_READ_PENDING) {
    /* In order for winsock to do a graceful close there must not be any */
    /* any pending reads, or the socket must be shut down for writing */
    if (!(tcp->flags & UV_HANDLE_SHARED_TCP_SOCKET)) {
      /* Just do shutdown on non-shared sockets, which ensures graceful close. */
      shutdown(tcp->socket, SD_SEND);

    } else if (uv_tcp_try_cancel_io(tcp) == 0) {
      /* In case of a shared socket, we try to cancel all outstanding I/O, */
      /* If that works, don't close the socket yet - wait for the read req to */
      /* return and close the socket in uv_tcp_endgame. */
      close_socket = 0;

    } else {
      /* When cancelling isn't possible - which could happen when an LSP is */
      /* present on an old Windows version, we will have to close the socket */
      /* with a read pending. That is not nice because trailing sent bytes */
      /* may not make it to the other side. */
    }

  } else if ((tcp->flags & UV_HANDLE_SHARED_TCP_SOCKET) &&
             tcp->accept_reqs != NULL) {
    /* Under normal circumstances closesocket() will ensure that all pending */
    /* accept reqs are canceled. However, when the socket is shared the */
    /* presence of another reference to the socket in another process will */
    /* keep the accept reqs going, so we have to ensure that these are */
    /* canceled. */
    if (uv_tcp_try_cancel_io(tcp) != 0) {
      /* When cancellation is not possible, there is another option: we can */
      /* close the incoming sockets, which will also cancel the accept */
      /* operations. However this is not cool because we might inadvertedly */
      /* close a socket that just accepted a new connection, which will */
      /* cause the connection to be aborted. */
      unsigned int i;
      for (i = 0; i < uv_simultaneous_server_accepts; i++) {
        uv_tcp_accept_t* req = &tcp->accept_reqs[i];
        if (req->accept_socket != INVALID_SOCKET &&
            !HasOverlappedIoCompleted(&req->overlapped)) {
          closesocket(req->accept_socket);
          req->accept_socket = INVALID_SOCKET;
        }
      }
    }
  }

  if (tcp->flags & UV_HANDLE_READING) {
    tcp->flags &= ~UV_HANDLE_READING;
    DECREASE_ACTIVE_COUNT(loop, tcp);
  }

  if (tcp->flags & UV_HANDLE_LISTENING) {
    tcp->flags &= ~UV_HANDLE_LISTENING;
    DECREASE_ACTIVE_COUNT(loop, tcp);
  }

  if (close_socket) {
    closesocket(tcp->socket);
    tcp->flags |= UV_HANDLE_TCP_SOCKET_CLOSED;
  }

  tcp->flags &= ~(UV_HANDLE_READABLE | UV_HANDLE_WRITABLE);
  uv__handle_closing(tcp);

  if (tcp->reqs_pending == 0) {
    uv_want_endgame(tcp->loop, (uv_handle_t*)tcp);
  }
}


int uv_tcp_open(uv_tcp_t* handle, uv_os_sock_t sock) {
  WSAPROTOCOL_INFOW protocol_info;
  int opt_len;

  /* Detect the address family of the socket. */
  opt_len = (int) sizeof protocol_info;
  if (getsockopt(sock,
                 SOL_SOCKET,
                 SO_PROTOCOL_INFOW,
                 (char*) &protocol_info,
                 &opt_len) == SOCKET_ERROR) {
    uv__set_sys_error(handle->loop, GetLastError());
    return -1;
  }

  /* Make the socket non-inheritable */
  if (!SetHandleInformation((HANDLE) sock, HANDLE_FLAG_INHERIT, 0)) {
    uv__set_sys_error(handle->loop, GetLastError());
    return -1;
  }

  if (uv_tcp_set_socket(handle->loop,
                        handle,
                        sock,
                        protocol_info.iAddressFamily,
                        1) < 0) {
    return -1;
  }

  return 0;
}
