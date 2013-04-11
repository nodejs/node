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
 * Threshold of active udp streams for which to preallocate udp read buffers.
 */
const unsigned int uv_active_udp_streams_threshold = 0;

/* A zero-size buffer for use by uv_udp_read */
static char uv_zero_[] = "";

int uv_udp_getsockname(uv_udp_t* handle, struct sockaddr* name,
    int* namelen) {
  uv_loop_t* loop = handle->loop;
  int result;

  if (!(handle->flags & UV_HANDLE_BOUND)) {
    uv__set_sys_error(loop, WSAEINVAL);
    return -1;
  }

  result = getsockname(handle->socket, name, namelen);
  if (result != 0) {
    uv__set_sys_error(loop, WSAGetLastError());
    return -1;
  }

  return 0;
}


static int uv_udp_set_socket(uv_loop_t* loop, uv_udp_t* handle, SOCKET socket,
    int family) {
  DWORD yes = 1;
  WSAPROTOCOL_INFOW info;
  int opt_len;

  assert(handle->socket == INVALID_SOCKET);

  /* Set SO_REUSEADDR on the socket. */
  if (setsockopt(socket,
                 SOL_SOCKET,
                 SO_REUSEADDR,
                 (char*) &yes,
                 sizeof yes) == SOCKET_ERROR) {
    uv__set_sys_error(loop, WSAGetLastError());
    return -1;
  }

  /* Set the socket to nonblocking mode */
  if (ioctlsocket(socket, FIONBIO, &yes) == SOCKET_ERROR) {
    uv__set_sys_error(loop, WSAGetLastError());
    return -1;
  }

  /* Make the socket non-inheritable */
  if (!SetHandleInformation((HANDLE)socket, HANDLE_FLAG_INHERIT, 0)) {
    uv__set_sys_error(loop, GetLastError());
    return -1;
  }

  /* Associate it with the I/O completion port. */
  /* Use uv_handle_t pointer as completion key. */
  if (CreateIoCompletionPort((HANDLE)socket,
                             loop->iocp,
                             (ULONG_PTR)socket,
                             0) == NULL) {
    uv__set_sys_error(loop, GetLastError());
    return -1;
  }

  if (pSetFileCompletionNotificationModes) {
    /* All know windowses that support SetFileCompletionNotificationModes */
    /* have a bug that makes it impossible to use this function in */
    /* conjunction with datagram sockets. We can work around that but only */
    /* if the user is using the default UDP driver (AFD) and has no other */
    /* LSPs stacked on top. Here we check whether that is the case. */
    opt_len = (int) sizeof info;
    if (getsockopt(socket,
                   SOL_SOCKET,
                   SO_PROTOCOL_INFOW,
                   (char*) &info,
                   &opt_len) == SOCKET_ERROR) {
      uv__set_sys_error(loop, GetLastError());
      return -1;
    }

    if (info.ProtocolChain.ChainLen == 1) {
      if (pSetFileCompletionNotificationModes((HANDLE)socket,
          FILE_SKIP_SET_EVENT_ON_HANDLE |
          FILE_SKIP_COMPLETION_PORT_ON_SUCCESS)) {
        handle->flags |= UV_HANDLE_SYNC_BYPASS_IOCP;
        handle->func_wsarecv = uv_wsarecv_workaround;
        handle->func_wsarecvfrom = uv_wsarecvfrom_workaround;
      } else if (GetLastError() != ERROR_INVALID_FUNCTION) {
        uv__set_sys_error(loop, GetLastError());
        return -1;
      }
    }
  }

  handle->socket = socket;

  if (family == AF_INET6) {
    handle->flags |= UV_HANDLE_IPV6;
  } else {
    assert(!(handle->flags & UV_HANDLE_IPV6));
  }

  return 0;
}


int uv_udp_init(uv_loop_t* loop, uv_udp_t* handle) {
  uv__handle_init(loop, (uv_handle_t*) handle, UV_UDP);

  handle->socket = INVALID_SOCKET;
  handle->reqs_pending = 0;
  handle->activecnt = 0;
  handle->func_wsarecv = WSARecv;
  handle->func_wsarecvfrom = WSARecvFrom;

  uv_req_init(loop, (uv_req_t*) &(handle->recv_req));
  handle->recv_req.type = UV_UDP_RECV;
  handle->recv_req.data = handle;

  return 0;
}


void uv_udp_close(uv_loop_t* loop, uv_udp_t* handle) {
  uv_udp_recv_stop(handle);
  closesocket(handle->socket);

  uv__handle_closing(handle);

  if (handle->reqs_pending == 0) {
    uv_want_endgame(loop, (uv_handle_t*) handle);
  }
}


void uv_udp_endgame(uv_loop_t* loop, uv_udp_t* handle) {
  if (handle->flags & UV__HANDLE_CLOSING &&
      handle->reqs_pending == 0) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    uv__handle_close(handle);
  }
}


static int uv__bind(uv_udp_t* handle,
                    int family,
                    struct sockaddr* addr,
                    int addrsize,
                    unsigned int flags) {
  int r;
  DWORD no = 0;

  if ((flags & UV_UDP_IPV6ONLY) && family != AF_INET6) {
    /* UV_UDP_IPV6ONLY is supported only for IPV6 sockets */
    uv__set_artificial_error(handle->loop, UV_EINVAL);
    return -1;
  }

  if (handle->socket == INVALID_SOCKET) {
    SOCKET sock = socket(family, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
      uv__set_sys_error(handle->loop, WSAGetLastError());
      return -1;
    }

    if (uv_udp_set_socket(handle->loop, handle, sock, family) < 0) {
      closesocket(sock);
      return -1;
    }

    if (family == AF_INET6)
      handle->flags |= UV_HANDLE_IPV6;
  }

  if (family == AF_INET6 && !(flags & UV_UDP_IPV6ONLY)) {
    /* On windows IPV6ONLY is on by default. */
    /* If the user doesn't specify it libuv turns it off. */

    /* TODO: how to handle errors? This may fail if there is no ipv4 stack */
    /* available, or when run on XP/2003 which have no support for dualstack */
    /* sockets. For now we're silently ignoring the error. */
    setsockopt(handle->socket,
               IPPROTO_IPV6,
               IPV6_V6ONLY,
               (char*) &no,
               sizeof no);
  }

  r = bind(handle->socket, addr, addrsize);
  if (r == SOCKET_ERROR) {
    uv__set_sys_error(handle->loop, WSAGetLastError());
    return -1;
  }

  handle->flags |= UV_HANDLE_BOUND;

  return 0;
}


int uv__udp_bind(uv_udp_t* handle, struct sockaddr_in addr,
    unsigned int flags) {
  return uv__bind(handle,
                  AF_INET,
                  (struct sockaddr*) &addr,
                  sizeof(struct sockaddr_in),
                  flags);
}


int uv__udp_bind6(uv_udp_t* handle, struct sockaddr_in6 addr,
    unsigned int flags) {
  return uv__bind(handle,
                  AF_INET6,
                  (struct sockaddr*) &addr,
                  sizeof(struct sockaddr_in6),
                  flags);
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
  if (loop->active_udp_streams < uv_active_udp_streams_threshold) {
    handle->flags &= ~UV_HANDLE_ZERO_READ;

    handle->recv_buffer = handle->alloc_cb((uv_handle_t*) handle, 65536);
    assert(handle->recv_buffer.len > 0);

    buf = handle->recv_buffer;
    memset(&handle->recv_from, 0, sizeof handle->recv_from);
    handle->recv_from_len = sizeof handle->recv_from;
    flags = 0;

    result = handle->func_wsarecvfrom(handle->socket,
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

  } else {
    handle->flags |= UV_HANDLE_ZERO_READ;

    buf.base = (char*) uv_zero_;
    buf.len = 0;
    flags = MSG_PEEK;

    result = handle->func_wsarecv(handle->socket,
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
}


int uv__udp_recv_start(uv_udp_t* handle, uv_alloc_cb alloc_cb,
    uv_udp_recv_cb recv_cb) {
  uv_loop_t* loop = handle->loop;

  if (handle->flags & UV_HANDLE_READING) {
    uv__set_sys_error(loop, WSAEALREADY);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_udp_bind(handle, uv_addr_ip4_any_, 0) < 0) {
    return -1;
  }

  handle->flags |= UV_HANDLE_READING;
  INCREASE_ACTIVE_COUNT(loop, handle);
  loop->active_udp_streams++;

  handle->recv_cb = recv_cb;
  handle->alloc_cb = alloc_cb;

  /* If reading was stopped and then started again, there could still be a */
  /* recv request pending. */
  if (!(handle->flags & UV_HANDLE_READ_PENDING))
    uv_udp_queue_recv(loop, handle);

  return 0;
}


int uv__udp_recv_stop(uv_udp_t* handle) {
  if (handle->flags & UV_HANDLE_READING) {
    handle->flags &= ~UV_HANDLE_READING;
    handle->loop->active_udp_streams--;
    DECREASE_ACTIVE_COUNT(loop, handle);
  }

  return 0;
}


static int uv__send(uv_udp_send_t* req, uv_udp_t* handle, uv_buf_t bufs[],
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
    REGISTER_HANDLE_REQ(loop, handle, req);
    uv_insert_pending_req(loop, (uv_req_t*)req);
  } else if (UV_SUCCEEDED_WITH_IOCP(result == 0)) {
    /* Request queued by the kernel. */
    req->queued_bytes = uv_count_bufs(bufs, bufcnt);
    handle->reqs_pending++;
    REGISTER_HANDLE_REQ(loop, handle, req);
  } else {
    /* Send failed due to an error. */
    uv__set_sys_error(loop, WSAGetLastError());
    return -1;
  }

  return 0;
}


int uv__udp_send(uv_udp_send_t* req, uv_udp_t* handle, uv_buf_t bufs[],
    int bufcnt, struct sockaddr_in addr, uv_udp_send_cb cb) {

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_udp_bind(handle, uv_addr_ip4_any_, 0) < 0) {
    return -1;
  }

  return uv__send(req,
                  handle,
                  bufs,
                  bufcnt,
                  (struct sockaddr*) &addr,
                  sizeof addr,
                  cb);
}


int uv__udp_send6(uv_udp_send_t* req, uv_udp_t* handle, uv_buf_t bufs[],
    int bufcnt, struct sockaddr_in6 addr, uv_udp_send_cb cb) {

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_udp_bind6(handle, uv_addr_ip6_any_, 0) < 0) {
    return -1;
  }

  return uv__send(req,
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

  if (!REQ_SUCCESS(req)) {
    DWORD err = GET_REQ_SOCK_ERROR(req);
    if (err == WSAEMSGSIZE) {
      /* Not a real error, it just indicates that the received packet */
      /* was bigger than the receive buffer. */
    } else if (err == WSAECONNRESET || err == WSAENETRESET) {
      /* A previous sendto operation failed; ignore this error. If */
      /* zero-reading we need to call WSARecv/WSARecvFrom _without_ the */
      /* MSG_PEEK flag to clear out the error queue. For nonzero reads, */
      /* immediately queue a new receive. */
      if (!(handle->flags & UV_HANDLE_ZERO_READ)) {
        goto done;
      }
    } else {
      /* A real error occurred. Report the error to the user only if we're */
      /* currently reading. */
      if (handle->flags & UV_HANDLE_READING) {
        uv__set_sys_error(loop, err);
        uv_udp_recv_stop(handle);
        buf = (handle->flags & UV_HANDLE_ZERO_READ) ?
              uv_buf_init(NULL, 0) : handle->recv_buffer;
        handle->recv_cb(handle, -1, buf, NULL, 0);
      }
      goto done;
    }
  }

  if (!(handle->flags & UV_HANDLE_ZERO_READ)) {
    /* Successful read */
    partial = !REQ_SUCCESS(req);
    handle->recv_cb(handle,
                    req->overlapped.InternalHigh,
                    handle->recv_buffer,
                    (struct sockaddr*) &handle->recv_from,
                    partial ? UV_UDP_PARTIAL : 0);
  } else if (handle->flags & UV_HANDLE_READING) {
    DWORD bytes, err, flags;
    struct sockaddr_storage from;
    int from_len;

    /* Do a nonblocking receive */
    /* TODO: try to read multiple datagrams at once. FIONREAD maybe? */
    buf = handle->alloc_cb((uv_handle_t*) handle, 65536);
    assert(buf.len > 0);

    memset(&from, 0, sizeof from);
    from_len = sizeof from;

    flags = 0;

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
      handle->recv_cb(handle, bytes, buf, (struct sockaddr*) &from, 0);
    } else {
      err = WSAGetLastError();
      if (err == WSAEMSGSIZE) {
        /* Message truncated */
        handle->recv_cb(handle,
                        bytes,
                        buf,
                        (struct sockaddr*) &from,
                        UV_UDP_PARTIAL);
      } if (err == WSAEWOULDBLOCK) {
        /* Kernel buffer empty */
        uv__set_sys_error(loop, WSAEWOULDBLOCK);
        handle->recv_cb(handle, 0, buf, NULL, 0);
      } else if (err != WSAECONNRESET && err != WSAENETRESET) {
        /* Serious error. WSAECONNRESET/WSANETRESET is ignored because this */
        /* just indicates that a previous sendto operation failed. */
        uv_udp_recv_stop(handle);
        uv__set_sys_error(loop, err);
        handle->recv_cb(handle, -1, buf, NULL, 0);
      }
    }
  }

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

  UNREGISTER_HANDLE_REQ(loop, handle, req);

  if (req->cb) {
    if (REQ_SUCCESS(req)) {
      req->cb(req, 0);
    } else {
      uv__set_sys_error(loop, GET_REQ_SOCK_ERROR(req));
      req->cb(req, -1);
    }
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


int uv_udp_set_membership(uv_udp_t* handle, const char* multicast_addr,
    const char* interface_addr, uv_membership membership) {
  int optname;
  struct ip_mreq mreq;

  /* If the socket is unbound, bind to inaddr_any. */
  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_udp_bind(handle, uv_addr_ip4_any_, 0) < 0) {
    return -1;
  }

  if (handle->flags & UV_HANDLE_IPV6) {
    uv__set_artificial_error(handle->loop, UV_ENOSYS);
    return -1;
  }

  memset(&mreq, 0, sizeof mreq);

  if (interface_addr) {
    mreq.imr_interface.s_addr = inet_addr(interface_addr);
  } else {
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  }

  mreq.imr_multiaddr.s_addr = inet_addr(multicast_addr);

  switch (membership) {
    case UV_JOIN_GROUP:
      optname = IP_ADD_MEMBERSHIP;
      break;
    case UV_LEAVE_GROUP:
      optname = IP_DROP_MEMBERSHIP;
      break;
    default:
      return uv__set_artificial_error(handle->loop, UV_EINVAL);
  }

  if (setsockopt(handle->socket,
                 IPPROTO_IP,
                 optname,
                 (char*) &mreq,
                 sizeof mreq) == SOCKET_ERROR) {
      uv__set_sys_error(handle->loop, WSAGetLastError());
    return -1;
  }

  return 0;
}


int uv_udp_set_broadcast(uv_udp_t* handle, int value) {
  BOOL optval = (BOOL) value;

  /* If the socket is unbound, bind to inaddr_any. */
  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_udp_bind(handle, uv_addr_ip4_any_, 0) < 0) {
    return -1;
  }

  if (setsockopt(handle->socket,
                 SOL_SOCKET,
                 SO_BROADCAST,
                 (char*) &optval,
                 sizeof optval)) {
    uv__set_sys_error(handle->loop, WSAGetLastError());
    return -1;
  }
  return 0;
}


int uv_udp_open(uv_udp_t* handle, uv_os_sock_t sock) {
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

  if (uv_udp_set_socket(handle->loop,
                        handle,
                        sock,
                        protocol_info.iAddressFamily) < 0) {
    return -1;
  }

  return 0;
}


#define SOCKOPT_SETTER(name, option4, option6, validate)                      \
  int uv_udp_set_##name(uv_udp_t* handle, int value) {                        \
    DWORD optval = (DWORD) value;                                             \
                                                                              \
    if (!(validate(value))) {                                                 \
      uv__set_artificial_error(handle->loop, UV_EINVAL);                      \
      return -1;                                                              \
    }                                                                         \
                                                                              \
    /* If the socket is unbound, bind to inaddr_any. */                       \
    if (!(handle->flags & UV_HANDLE_BOUND) &&                                 \
        uv_udp_bind(handle, uv_addr_ip4_any_, 0) < 0) {                       \
      return -1;                                                              \
    }                                                                         \
                                                                              \
    if (!(handle->flags & UV_HANDLE_IPV6)) {                                  \
      /* Set IPv4 socket option */                                            \
      if (setsockopt(handle->socket,                                          \
                     IPPROTO_IP,                                              \
                     option4,                                                 \
                     (char*) &optval,                                         \
                     sizeof optval)) {                                        \
        uv__set_sys_error(handle->loop, WSAGetLastError());                   \
        return -1;                                                            \
      }                                                                       \
    } else {                                                                  \
      /* Set IPv6 socket option */                                            \
      if (setsockopt(handle->socket,                                          \
                     IPPROTO_IPV6,                                            \
                     option6,                                                 \
                     (char*) &optval,                                         \
                     sizeof optval)) {                                        \
        uv__set_sys_error(handle->loop, WSAGetLastError());                   \
        return -1;                                                            \
      }                                                                       \
    }                                                                         \
    return 0;                                                                 \
  }

#define VALIDATE_TTL(value) ((value) >= 1 && (value) <= 255)
#define VALIDATE_MULTICAST_TTL(value) ((value) >= -1 && (value) <= 255)
#define VALIDATE_MULTICAST_LOOP(value) (1)

SOCKOPT_SETTER(ttl,
               IP_TTL,
               IPV6_HOPLIMIT,
               VALIDATE_TTL)
SOCKOPT_SETTER(multicast_ttl,
               IP_MULTICAST_TTL,
               IPV6_MULTICAST_HOPS,
               VALIDATE_MULTICAST_TTL)
SOCKOPT_SETTER(multicast_loop,
               IP_MULTICAST_LOOP,
               IPV6_MULTICAST_LOOP,
               VALIDATE_MULTICAST_LOOP)

#undef SOCKOPT_SETTER
#undef VALIDATE_TTL
#undef VALIDATE_MULTICAST_TTL
#undef VALIDATE_MULTICAST_LOOP
