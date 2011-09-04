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
#include "../uv-common.h"
#include "internal.h"
#include <stdio.h>

#if 0
/*
 * Threshold of active udp streams for which to preallocate udp read buffers.
 */
const unsigned int uv_active_udp_streams_threshold = 0;

/* A zero-size buffer for use by uv_udp_read */
static char uv_zero_[] = "";
#endif

/* Counter to keep track of active udp streams */
static unsigned int active_udp_streams = 0;


int uv_udp_getsockname(uv_udp_t* handle, struct sockaddr* name,
    int* namelen) {
  uv_loop_t* loop = handle->loop;
  int result;

  if (!(handle->flags & UV_HANDLE_BOUND)) {
    uv_set_sys_error(loop, WSAEINVAL);
    return -1;
  }

  result = getsockname(handle->socket, name, namelen);
  if (result != 0) {
    uv_set_sys_error(loop, WSAGetLastError());
    return -1;
  }

  return 0;
}


static int uv_udp_set_socket(uv_loop_t* loop, uv_udp_t* handle,
    SOCKET socket) {
  DWORD yes = 1;

  assert(handle->socket == INVALID_SOCKET);

  /* Set the socket to nonblocking mode */
  if (ioctlsocket(socket, FIONBIO, &yes) == SOCKET_ERROR) {
    uv_set_sys_error(loop, WSAGetLastError());
    return -1;
  }

  /* Make the socket non-inheritable */
  if (!SetHandleInformation((HANDLE)socket, HANDLE_FLAG_INHERIT, 0)) {
    uv_set_sys_error(loop, GetLastError());
    return -1;
  }

  /* Associate it with the I/O completion port. */
  /* Use uv_handle_t pointer as completion key. */
  if (CreateIoCompletionPort((HANDLE)socket,
                             loop->iocp,
                             (ULONG_PTR)socket,
                             0) == NULL) {
    uv_set_sys_error(loop, GetLastError());
    return -1;
  }

  if (pSetFileCompletionNotificationModes) {
    if (!pSetFileCompletionNotificationModes((HANDLE)socket,
       FILE_SKIP_SET_EVENT_ON_HANDLE | FILE_SKIP_COMPLETION_PORT_ON_SUCCESS)) {
      uv_set_sys_error(loop, GetLastError());
      return -1;
    }

    handle->flags |= UV_HANDLE_SYNC_BYPASS_IOCP;
  }

  handle->socket = socket;

  return 0;
}


int uv_udp_init(uv_loop_t* loop, uv_udp_t* handle) {
  handle->type = UV_UDP;
  handle->socket = INVALID_SOCKET;
  handle->reqs_pending = 0;
  handle->loop = loop;
  handle->flags = 0;

  uv_req_init(loop, (uv_req_t*) &(handle->recv_req));
  handle->recv_req.type = UV_UDP_RECV;
  handle->recv_req.data = handle;

  uv_ref(loop);

  loop->counters.handle_init++;
  loop->counters.udp_init++;

  return 0;
}


void uv_udp_endgame(uv_loop_t* loop, uv_udp_t* handle) {
  if (handle->flags & UV_HANDLE_CLOSING &&
      handle->reqs_pending == 0) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    handle->flags |= UV_HANDLE_CLOSED;

    if (handle->close_cb) {
      handle->close_cb((uv_handle_t*)handle);
    }

    uv_unref(loop);
  }
}


static int uv__bind(uv_udp_t* handle, int domain, struct sockaddr* addr,
    int addrsize, unsigned int flags) {
  uv_loop_t* loop = handle->loop;
  DWORD err;
  int r;
  SOCKET sock;

  if ((flags & UV_UDP_IPV6ONLY) && domain != AF_INET6) {
    /* UV_UDP_IPV6ONLY is supported only for IPV6 sockets */
    uv_set_sys_error(loop, UV_EINVAL);
  }

  if (handle->socket == INVALID_SOCKET) {
    sock = socket(domain, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
      uv_set_sys_error(loop, WSAGetLastError());
      return -1;
    }

    if (uv_udp_set_socket(loop, handle, sock) == -1) {
      closesocket(sock);
      return -1;
    }
  }

  if (domain == AF_INET6 && !(flags & UV_UDP_IPV6ONLY)) {
    DWORD off = 0;
    /* On windows IPV6ONLY is on by default. */
    /* If the user doesn't specify it libuv turns it off. */

    /* TODO: how to handle errors? This may fail if there is no ipv4 stack */
    /* available, or when run on XP/2003 which have no support for dualstack */
    /* sockets. For now we're silently ignoring the error. */
    setsockopt(sock,
               IPPROTO_IPV6,
               IPV6_V6ONLY,
               (const char*) &off,
               sizeof off);
  }

  r = bind(handle->socket, addr, addrsize);

  if (r == SOCKET_ERROR) {
    err = WSAGetLastError();
    uv_set_sys_error(loop, WSAGetLastError());
    return -1;
  }

  handle->flags |= UV_HANDLE_BOUND;

  return 0;
}


int uv_udp_bind(uv_udp_t* handle, struct sockaddr_in addr,
    unsigned int flags) {
  uv_loop_t* loop = handle->loop;

  if (addr.sin_family != AF_INET) {
    uv_set_sys_error(loop, WSAEFAULT);
    return -1;
  }

  return uv__bind(handle,
                  AF_INET,
                  (struct sockaddr*) &addr,
                  sizeof(struct sockaddr_in),
                  flags);
}


int uv_udp_bind6(uv_udp_t* handle, struct sockaddr_in6 addr,
    unsigned int flags) {
  uv_loop_t* loop = handle->loop;

  if (addr.sin6_family != AF_INET6) {
    uv_set_sys_error(loop, WSAEFAULT);
    return -1;
  }

  if (uv_allow_ipv6) {
    handle->flags |= UV_HANDLE_IPV6;
    return uv__bind(handle,
                    AF_INET6,
                    (struct sockaddr*) &addr,
                    sizeof(struct sockaddr_in6),
                    flags);
  } else {
    uv_new_sys_error(WSAEAFNOSUPPORT);
    return -1;
  }
}


static void uv_udp_queue_recv(uv_loop_t* loop, uv_udp_t* handle) {
  uv_req_t* req;
  uv_buf_t buf;
  DWORD bytes, flags;
  int result;

  assert(handle->flags & UV_HANDLE_READING);
  assert(!(handle->flags & UV_HANDLE_READ_PENDING));

  req = &handle->recv_req;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

  /*
   * Preallocate a read buffer if the number of active streams is below
   * the threshold.
  */
#if 0
  if (active_udp_streams < uv_active_udp_streams_threshold) {
    handle->flags &= ~UV_HANDLE_ZERO_READ;
#endif
    handle->recv_buffer = handle->alloc_cb((uv_handle_t*) handle, 65536);
    assert(handle->recv_buffer.len > 0);

    buf = handle->recv_buffer;
    memset(&handle->recv_from, 0, sizeof handle->recv_from);
    handle->recv_from_len = sizeof handle->recv_from;
    flags = 0;

    result = WSARecvFrom(handle->socket,
                      (WSABUF*) &buf,
                      1,
                      &bytes,
                      &flags,
                      (struct sockaddr*) &handle->recv_from,
                      &handle->recv_from_len,
                      &req->overlapped,
                      NULL);

    if (UV_SUCCEEDED_WITHOUT_IOCP(result == 0)) {
      /* Process the req without IOCP. */
      handle->flags |= UV_HANDLE_READ_PENDING;
      req->overlapped.InternalHigh = bytes;
      handle->reqs_pending++;
      uv_insert_pending_req(loop, req);
    } else if (UV_SUCCEEDED_WITH_IOCP(result == 0)) {
      /* The req will be processed with IOCP. */
      handle->flags |= UV_HANDLE_READ_PENDING;
      handle->reqs_pending++;
    } else {
      /* Make this req pending reporting an error. */
      SET_REQ_ERROR(req, WSAGetLastError());
      uv_insert_pending_req(loop, req);
      handle->reqs_pending++;
    }
#if 0
  } else {
    handle->flags |= UV_HANDLE_ZERO_READ;

    buf.base = (char*) uv_zero_;
    buf.len = 0;
    flags = MSG_PARTIAL;

    result = WSARecv(handle->socket,
                     (WSABUF*) &buf,
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
      uv_insert_pending_req(loop, req);
    } else if (UV_SUCCEEDED_WITH_IOCP(result == 0)) {
      /* The req will be processed with IOCP. */
      handle->flags |= UV_HANDLE_READ_PENDING;
      handle->reqs_pending++;
    } else {
      /* Make this req pending reporting an error. */
      SET_REQ_ERROR(req, WSAGetLastError());
      uv_insert_pending_req(loop, req);
      handle->reqs_pending++;
    }
  }
#endif
}


int uv_udp_recv_start(uv_udp_t* handle, uv_alloc_cb alloc_cb,
    uv_udp_recv_cb recv_cb) {
  uv_loop_t* loop = handle->loop;

  if (handle->flags & UV_HANDLE_READING) {
    uv_set_sys_error(loop, WSAEALREADY);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_udp_bind(handle, uv_addr_ip4_any_, 0) < 0) {
    return -1;
  }

  handle->flags |= UV_HANDLE_READING;
  active_udp_streams++;

  handle->recv_cb = recv_cb;
  handle->alloc_cb = alloc_cb;

  /* If reading was stopped and then started again, there could stell be a */
  /* recv request pending. */
  if (!(handle->flags & UV_HANDLE_READ_PENDING))
    uv_udp_queue_recv(loop, handle);

  return 0;
}


int uv_udp_recv_stop(uv_udp_t* handle) {
  if (handle->flags & UV_HANDLE_READING) {
    handle->flags &= ~UV_HANDLE_READING;
    active_udp_streams--;
  }

  return 0;
}


int uv_udp_connect6(uv_connect_t* req, uv_udp_t* handle,
    struct sockaddr_in6 address, uv_connect_cb cb) {
  uv_loop_t* loop = handle->loop;
  int addrsize = sizeof(struct sockaddr_in6);
  BOOL success;
  DWORD bytes;

  if (!uv_allow_ipv6) {
    uv_new_sys_error(WSAEAFNOSUPPORT);
    return -1;
  }

  if (address.sin6_family != AF_INET6) {
    uv_set_sys_error(loop, WSAEFAULT);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_udp_bind6(handle, uv_addr_ip6_any_, 0) < 0)
    return -1;

  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_CONNECT;
  req->handle = (uv_stream_t*) handle;
  req->cb = cb;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

  success = pConnectEx6(handle->socket,
                       (struct sockaddr*) &address,
                       addrsize,
                       NULL,
                       0,
                       &bytes,
                       &req->overlapped);

  if (UV_SUCCEEDED_WITHOUT_IOCP(success)) {
    handle->reqs_pending++;
    uv_insert_pending_req(loop, (uv_req_t*)req);
  } else if (UV_SUCCEEDED_WITH_IOCP(success)) {
    handle->reqs_pending++;
  } else {
    uv_set_sys_error(loop, WSAGetLastError());
    return -1;
  }

  return 0;
}


static int uv__udp_send(uv_udp_send_t* req, uv_udp_t* handle, uv_buf_t bufs[],
    int bufcnt, struct sockaddr* addr, int addr_len, uv_udp_send_cb cb) {
  uv_loop_t* loop = handle->loop;
  DWORD result, bytes;

  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_UDP_SEND;
  req->handle = handle;
  req->cb = cb;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

  result = WSASendTo(handle->socket,
                     (WSABUF*)bufs,
                     bufcnt,
                     &bytes,
                     0,
                     addr,
                     addr_len,
                     &req->overlapped,
                     NULL);

  if (UV_SUCCEEDED_WITHOUT_IOCP(result == 0)) {
    /* Request completed immediately. */
    req->queued_bytes = 0;
    handle->reqs_pending++;
    uv_insert_pending_req(loop, (uv_req_t*)req);
  } else if (UV_SUCCEEDED_WITH_IOCP(result == 0)) {
    /* Request queued by the kernel. */
    req->queued_bytes = uv_count_bufs(bufs, bufcnt);
    handle->reqs_pending++;
  } else {
    /* Send failed due to an error. */
    uv_set_sys_error(loop, WSAGetLastError());
    return -1;
  }

  return 0;
}


int uv_udp_send(uv_udp_send_t* req, uv_udp_t* handle, uv_buf_t bufs[],
    int bufcnt, struct sockaddr_in addr, uv_udp_send_cb cb) {

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_udp_bind(handle, uv_addr_ip4_any_, 0) < 0) {
    return -1;
  }

  return uv__udp_send(req,
                      handle,
                      bufs,
                      bufcnt,
                      (struct sockaddr*) &addr,
                      sizeof addr,
                      cb);
}


int uv_udp_send6(uv_udp_send_t* req, uv_udp_t* handle, uv_buf_t bufs[],
    int bufcnt, struct sockaddr_in6 addr, uv_udp_send_cb cb) {

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_udp_bind6(handle, uv_addr_ip6_any_, 0) < 0) {
    return -1;
  }

  return uv__udp_send(req,
                      handle,
                      bufs,
                      bufcnt,
                      (struct sockaddr*) &addr,
                      sizeof addr,
                      cb);
}


void uv_process_udp_recv_req(uv_loop_t* loop, uv_udp_t* handle,
    uv_req_t* req) {
  uv_buf_t buf;
  int partial;

  assert(handle->type == UV_UDP);

  handle->flags &= ~UV_HANDLE_READ_PENDING;

  if (!REQ_SUCCESS(req) &&
      GET_REQ_STATUS(req) != STATUS_RECEIVE_EXPEDITED) {
    /* An error occurred doing the read. */
    if ((handle->flags & UV_HANDLE_READING)) {
      loop->last_error = GET_REQ_UV_SOCK_ERROR(req);
      uv_udp_recv_stop(handle);
#if 0
      buf = (handle->flags & UV_HANDLE_ZERO_READ) ?
            uv_buf_init(NULL, 0) : handle->recv_buffer;
#else
      buf = handle->recv_buffer;
#endif
      handle->recv_cb(handle, -1, buf, NULL, 0);
    }
    goto done;
  }

#if 0
  if (!(handle->flags & UV_HANDLE_ZERO_READ)) {
#endif
    /* Successful read */
    partial = (GET_REQ_STATUS(req) == STATUS_RECEIVE_EXPEDITED);
    handle->recv_cb(handle,
                    req->overlapped.InternalHigh,
                    handle->recv_buffer,
                    (struct sockaddr*) &handle->recv_from,
                    partial ? UV_UDP_PARTIAL : 0);
#if 0
  } else {
    DWORD bytes, err, flags;
    struct sockaddr_storage from;
    int from_len;

    /* Do a nonblocking receive */
    /* TODO: try to read multiple datagrams at once. FIONREAD maybe? */
    buf = handle->alloc_cb((uv_handle_t*) handle, 65536);
    assert(buf.len > 0);

    memset(&from, 0, sizeof from);
    from_len = sizeof from;
    flags = MSG_PARTIAL;

    if (WSARecvFrom(handle->socket,
                    (WSABUF*)&buf,
                    1,
                    &bytes,
                    &flags,
                    (struct sockaddr*) &from,
                    &from_len,
                    NULL,
                    NULL) != SOCKET_ERROR) {

      /* Message received */
      handle->recv_cb(handle,
                      bytes,
                      buf,
                      (struct sockaddr*) &from,
                        (flags & MSG_PARTIAL) ? UV_UDP_PARTIAL : 0);
    } else {
      err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK) {
        uv_set_sys_error(loop, WSAEWOULDBLOCK);
        handle->recv_cb(handle, 0, buf, NULL, 0);
      } else {
        /* Ouch! serious error. */
        uv_set_sys_error(loop, err);
        handle->recv_cb(handle, -1, buf, NULL, 0);
      }
    }
  }
#endif

done:
  /* Post another read if still reading and not closing. */
  if ((handle->flags & UV_HANDLE_READING) &&
      !(handle->flags & UV_HANDLE_READ_PENDING)) {
    uv_udp_queue_recv(loop, handle);
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_udp_send_req(uv_loop_t* loop, uv_udp_t* handle,
    uv_udp_send_t* req) {
  assert(handle->type == UV_UDP);

  if (req->cb) {
    if (REQ_SUCCESS(req)) {
      req->cb(req, 0);
    } else {
      loop->last_error = GET_REQ_UV_SOCK_ERROR(req);
      req->cb(req, -1);
    }
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}

