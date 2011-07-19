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

/*
 * Guids and typedefs for winsock extension functions
 * Mingw32 doesn't have these :-(
 */
#ifndef WSAID_ACCEPTEX
# define WSAID_ACCEPTEX                                        \
         {0xb5367df1, 0xcbac, 0x11cf,                          \
         {0x95, 0xca, 0x00, 0x80, 0x5f, 0x48, 0xa1, 0x92}}

# define WSAID_CONNECTEX                                       \
         {0x25a207b9, 0xddf3, 0x4660,                          \
         {0x8e, 0xe9, 0x76, 0xe5, 0x8c, 0x74, 0x06, 0x3e}}

# define WSAID_GETACCEPTEXSOCKADDRS                            \
         {0xb5367df2, 0xcbac, 0x11cf,                          \
         {0x95, 0xca, 0x00, 0x80, 0x5f, 0x48, 0xa1, 0x92}}

# define WSAID_DISCONNECTEX                                    \
         {0x7fda2e11, 0x8630, 0x436f,                          \
         {0xa0, 0x31, 0xf5, 0x36, 0xa6, 0xee, 0xc1, 0x57}}

# define WSAID_TRANSMITFILE                                    \
         {0xb5367df0, 0xcbac, 0x11cf,                          \
         {0x95, 0xca, 0x00, 0x80, 0x5f, 0x48, 0xa1, 0x92}}

  typedef BOOL PASCAL (*LPFN_ACCEPTEX)
                      (SOCKET sListenSocket,
                       SOCKET sAcceptSocket,
                       PVOID lpOutputBuffer,
                       DWORD dwReceiveDataLength,
                       DWORD dwLocalAddressLength,
                       DWORD dwRemoteAddressLength,
                       LPDWORD lpdwBytesReceived,
                       LPOVERLAPPED lpOverlapped);

  typedef BOOL PASCAL (*LPFN_CONNECTEX)
                      (SOCKET s,
                       const struct sockaddr* name,
                       int namelen,
                       PVOID lpSendBuffer,
                       DWORD dwSendDataLength,
                       LPDWORD lpdwBytesSent,
                       LPOVERLAPPED lpOverlapped);

  typedef void PASCAL (*LPFN_GETACCEPTEXSOCKADDRS)
                      (PVOID lpOutputBuffer,
                       DWORD dwReceiveDataLength,
                       DWORD dwLocalAddressLength,
                       DWORD dwRemoteAddressLength,
                       LPSOCKADDR* LocalSockaddr,
                       LPINT LocalSockaddrLength,
                       LPSOCKADDR* RemoteSockaddr,
                       LPINT RemoteSockaddrLength);

  typedef BOOL PASCAL (*LPFN_DISCONNECTEX)
                      (SOCKET hSocket,
                       LPOVERLAPPED lpOverlapped,
                       DWORD dwFlags,
                       DWORD reserved);

  typedef BOOL PASCAL (*LPFN_TRANSMITFILE)
                      (SOCKET hSocket,
                       HANDLE hFile,
                       DWORD nNumberOfBytesToWrite,
                       DWORD nNumberOfBytesPerSend,
                       LPOVERLAPPED lpOverlapped,
                       LPTRANSMIT_FILE_BUFFERS lpTransmitBuffers,
                       DWORD dwFlags);
#endif

/*
 * MinGW is missing this too
 */
#ifndef SO_UPDATE_CONNECT_CONTEXT
# define SO_UPDATE_CONNECT_CONTEXT   0x7010
#endif


/* Pointers to winsock extension functions to be retrieved dynamically */
static LPFN_CONNECTEX               pConnectEx;
static LPFN_ACCEPTEX                pAcceptEx;
static LPFN_GETACCEPTEXSOCKADDRS    pGetAcceptExSockAddrs;
static LPFN_DISCONNECTEX            pDisconnectEx;
static LPFN_TRANSMITFILE            pTransmitFile;

/* IPv6 version of these extension functions */
static LPFN_CONNECTEX               pConnectEx6;
static LPFN_ACCEPTEX                pAcceptEx6;
static LPFN_GETACCEPTEXSOCKADDRS    pGetAcceptExSockAddrs6;
static LPFN_DISCONNECTEX            pDisconnectEx6;
static LPFN_TRANSMITFILE            pTransmitFile6;

/* Ip address used to bind to any port at any interface */
static struct sockaddr_in uv_addr_ip4_any_;
static struct sockaddr_in6 uv_addr_ip6_any_;

/* A zero-size buffer for use by uv_tcp_read */
static char uv_zero_[] = "";

/* mark if IPv6 sockets are supported */
static BOOL uv_allow_ipv6 = FALSE;


/*
 * Retrieves the pointer to a winsock extension function.
 */
static BOOL uv_get_extension_function(SOCKET socket, GUID guid,
    void **target) {
  DWORD result, bytes;

  result = WSAIoctl(socket,
                    SIO_GET_EXTENSION_FUNCTION_POINTER,
                    &guid,
                    sizeof(guid),
                    (void*)target,
                    sizeof(*target),
                    &bytes,
                    NULL,
                    NULL);

  if (result == SOCKET_ERROR) {
    *target = NULL;
    return FALSE;
  } else {
    return TRUE;
  }
}


/*
 * Setup tcp subsystem
 */
void uv_winsock_startup() {
  const GUID wsaid_connectex            = WSAID_CONNECTEX;
  const GUID wsaid_acceptex             = WSAID_ACCEPTEX;
  const GUID wsaid_getacceptexsockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
  const GUID wsaid_disconnectex         = WSAID_DISCONNECTEX;
  const GUID wsaid_transmitfile         = WSAID_TRANSMITFILE;

  WSADATA wsa_data;
  int errorno;
  SOCKET dummy;
  SOCKET dummy6;

  /* Initialize winsock */
  errorno = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (errorno != 0) {
    uv_fatal_error(errorno, "WSAStartup");
  }

  /* Set implicit binding address used by connectEx */
  uv_addr_ip4_any_ = uv_ip4_addr("0.0.0.0", 0);
  uv_addr_ip6_any_ = uv_ip6_addr("::1", 0);

  /* Retrieve the needed winsock extension function pointers. */
  dummy = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (dummy == INVALID_SOCKET) {
    uv_fatal_error(WSAGetLastError(), "socket");
  }

  if (!uv_get_extension_function(dummy,
                            wsaid_connectex,
                            (void**)&pConnectEx) ||
      !uv_get_extension_function(dummy,
                            wsaid_acceptex,
                            (void**)&pAcceptEx) ||
      !uv_get_extension_function(dummy,
                            wsaid_getacceptexsockaddrs,
                            (void**)&pGetAcceptExSockAddrs) ||
      !uv_get_extension_function(dummy,
                            wsaid_disconnectex,
                            (void**)&pDisconnectEx) ||
      !uv_get_extension_function(dummy,
                            wsaid_transmitfile,
                            (void**)&pTransmitFile)) {
    uv_fatal_error(WSAGetLastError(),
                   "WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER)");
  }

  if (closesocket(dummy) == SOCKET_ERROR) {
    uv_fatal_error(WSAGetLastError(), "closesocket");
  }

  /* optional IPv6 versions of winsock extension functions */
  dummy6 = socket(AF_INET6, SOCK_STREAM, IPPROTO_IP);
  if (dummy6 != INVALID_SOCKET) {
    uv_allow_ipv6 = TRUE;

    if (!uv_get_extension_function(dummy6,
                              wsaid_connectex,
                              (void**)&pConnectEx6) ||
        !uv_get_extension_function(dummy6,
                              wsaid_acceptex,
                              (void**)&pAcceptEx6) ||
        !uv_get_extension_function(dummy6,
                              wsaid_getacceptexsockaddrs,
                              (void**)&pGetAcceptExSockAddrs6) ||
        !uv_get_extension_function(dummy6,
                              wsaid_disconnectex,
                              (void**)&pDisconnectEx6) ||
        !uv_get_extension_function(dummy6,
                              wsaid_transmitfile,
                              (void**)&pTransmitFile6)) {
      uv_allow_ipv6 = FALSE;
    }

    if (closesocket(dummy6) == SOCKET_ERROR) {
      uv_fatal_error(WSAGetLastError(), "closesocket");
    }
  }
}


static int uv_tcp_set_socket(uv_tcp_t* handle, SOCKET socket) {
  DWORD yes = 1;

  assert(handle->socket == INVALID_SOCKET);

  /* Set the socket to nonblocking mode */
  if (ioctlsocket(socket, FIONBIO, &yes) == SOCKET_ERROR) {
    uv_set_sys_error(WSAGetLastError());
    return -1;
  }

  /* Make the socket non-inheritable */
  if (!SetHandleInformation((HANDLE)socket, HANDLE_FLAG_INHERIT, 0)) {
    uv_set_sys_error(GetLastError());
    return -1;
  }

  /* Associate it with the I/O completion port. */
  /* Use uv_handle_t pointer as completion key. */
  if (CreateIoCompletionPort((HANDLE)socket,
                             LOOP->iocp,
                             (ULONG_PTR)socket,
                             0) == NULL) {
    uv_set_sys_error(GetLastError());
    return -1;
  }

  handle->socket = socket;

  return 0;
}


int uv_tcp_init(uv_tcp_t* handle) {
  uv_stream_init((uv_stream_t*)handle);

  handle->socket = INVALID_SOCKET;
  handle->type = UV_TCP;
  handle->reqs_pending = 0;
  handle->accept_socket = INVALID_SOCKET;

  uv_counters()->tcp_init++;

  return 0;
}


void uv_tcp_endgame(uv_tcp_t* handle) {
  uv_err_t err;
  int status;

  if (handle->flags & UV_HANDLE_SHUTTING &&
      !(handle->flags & UV_HANDLE_SHUT) &&
      handle->write_reqs_pending == 0) {

    if (shutdown(handle->socket, SD_SEND) != SOCKET_ERROR) {
      status = 0;
      handle->flags |= UV_HANDLE_SHUT;
    } else {
      status = -1;
      err = uv_new_sys_error(WSAGetLastError());
    }
    if (handle->shutdown_req->cb) {
      if (status == -1) {
        LOOP->last_error = err;
      }
      handle->shutdown_req->cb(handle->shutdown_req, status);
    }
    handle->reqs_pending--;
  }

  if (handle->flags & UV_HANDLE_CLOSING &&
      handle->reqs_pending == 0) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    handle->flags |= UV_HANDLE_CLOSED;

    if (handle->close_cb) {
      handle->close_cb((uv_handle_t*)handle);
    }

    uv_unref();
  }
}


static int uv__bind(uv_tcp_t* handle, int domain, struct sockaddr* addr, int addrsize) {
  DWORD err;
  int r;
  SOCKET sock;

  if (handle->socket == INVALID_SOCKET) {
    sock = socket(domain, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
      uv_set_sys_error(WSAGetLastError());
      return -1;
    }

    if (uv_tcp_set_socket(handle, sock) == -1) {
      closesocket(sock);
      return -1;
    }
  }

  r = bind(handle->socket, addr, addrsize);

  if (r == SOCKET_ERROR) {
    err = WSAGetLastError();
    if (err == WSAEADDRINUSE) {
      /* Some errors are not to be reported until connect() or listen() */
      handle->error = uv_new_sys_error(err);
      handle->flags |= UV_HANDLE_BIND_ERROR;
    } else {
      uv_set_sys_error(err);
      return -1;
    }
  }

  handle->flags |= UV_HANDLE_BOUND;

  return 0;
}


int uv_tcp_bind(uv_tcp_t* handle, struct sockaddr_in addr) {
  if (addr.sin_family != AF_INET) {
    uv_set_sys_error(WSAEFAULT);
    return -1;
  }

  return uv__bind(handle, AF_INET, (struct sockaddr*)&addr, sizeof(struct sockaddr_in));
}


int uv_tcp_bind6(uv_tcp_t* handle, struct sockaddr_in6 addr) {
  if (addr.sin6_family != AF_INET6) {
    uv_set_sys_error(WSAEFAULT);
    return -1;
  }
  if (uv_allow_ipv6) {
    handle->flags |= UV_HANDLE_IPV6;
    return uv__bind(handle, AF_INET6, (struct sockaddr*)&addr, sizeof(struct sockaddr_in6));
  } else {
    uv_new_sys_error(UV_EAFNOSUPPORT);
    return -1;
  }
}


static void uv_tcp_queue_accept(uv_tcp_t* handle) {
  uv_req_t* req;
  BOOL success;
  DWORD bytes;
  SOCKET accept_socket;
  short family;
  LPFN_ACCEPTEX pAcceptExFamily;

  assert(handle->flags & UV_HANDLE_LISTENING);
  assert(handle->accept_socket == INVALID_SOCKET);

  /* Prepare the uv_req structure. */
  req = &handle->accept_req;

  /* choose family and extension function */
  if ((handle->flags & UV_HANDLE_IPV6) != 0) {
    family = AF_INET6;
    pAcceptExFamily = pAcceptEx6;
  } else {
    family = AF_INET;
    pAcceptExFamily = pAcceptEx;
  }

  /* Open a socket for the accepted connection. */
  accept_socket = socket(family, SOCK_STREAM, 0);
  if (accept_socket == INVALID_SOCKET) {
    req->error = uv_new_sys_error(WSAGetLastError());
    uv_insert_pending_req(req);
    handle->reqs_pending++;
    return;
  }

  /* Prepare the overlapped structure. */
  memset(&(req->overlapped), 0, sizeof(req->overlapped));

  success = pAcceptExFamily(handle->socket,
                          accept_socket,
                          (void*)&handle->accept_buffer,
                          0,
                          sizeof(struct sockaddr_storage),
                          sizeof(struct sockaddr_storage),
                          &bytes,
                          &req->overlapped);

  if (!success && WSAGetLastError() != ERROR_IO_PENDING) {
    /* Make this req pending reporting an error. */
    req->error = uv_new_sys_error(WSAGetLastError());
    uv_insert_pending_req(req);
    handle->reqs_pending++;
    /* Destroy the preallocated client socket. */
    closesocket(accept_socket);
    return;
  }

  handle->accept_socket = accept_socket;

  handle->reqs_pending++;
}


static void uv_tcp_queue_read(uv_tcp_t* handle) {
  uv_req_t* req;
  uv_buf_t buf;
  int result;
  DWORD bytes, flags;

  assert(handle->flags & UV_HANDLE_READING);
  assert(!(handle->flags & UV_HANDLE_READ_PENDING));

  req = &handle->read_req;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

  buf.base = (char*) &uv_zero_;
  buf.len = 0;

  flags = 0;
  result = WSARecv(handle->socket,
                   (WSABUF*)&buf,
                   1,
                   &bytes,
                   &flags,
                   &req->overlapped,
                   NULL);
  if (result != 0 && WSAGetLastError() != ERROR_IO_PENDING) {
    /* Make this req pending reporting an error. */
    req->error = uv_new_sys_error(WSAGetLastError());
    uv_insert_pending_req(req);
    handle->reqs_pending++;
    return;
  }

  handle->flags |= UV_HANDLE_READ_PENDING;
  handle->reqs_pending++;
}


int uv_tcp_listen(uv_tcp_t* handle, int backlog, uv_connection_cb cb) {
  assert(backlog > 0);

  if (handle->flags & UV_HANDLE_BIND_ERROR) {
    LOOP->last_error = handle->error;
    return -1;
  }

  if (handle->flags & UV_HANDLE_LISTENING ||
      handle->flags & UV_HANDLE_READING) {
    /* Already listening. */
    uv_set_sys_error(WSAEALREADY);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_tcp_bind(handle, uv_addr_ip4_any_) < 0)
    return -1;

  if (listen(handle->socket, backlog) == SOCKET_ERROR) {
    uv_set_sys_error(WSAGetLastError());
    return -1;
  }

  handle->flags |= UV_HANDLE_LISTENING;
  handle->connection_cb = cb;

  uv_req_init(&(handle->accept_req));
  handle->accept_req.type = UV_ACCEPT;
  handle->accept_req.data = handle;
  uv_tcp_queue_accept(handle);

  return 0;
}


int uv_tcp_accept(uv_tcp_t* server, uv_tcp_t* client) {
  int rv = 0;

  if (server->accept_socket == INVALID_SOCKET) {
    uv_set_sys_error(WSAENOTCONN);
    return -1;
  }

  if (uv_tcp_set_socket(client, server->accept_socket) == -1) {
    closesocket(server->accept_socket);
    rv = -1;
  } else {
    uv_connection_init((uv_stream_t*)client);
  }

  server->accept_socket = INVALID_SOCKET;

  if (!(server->flags & UV_HANDLE_CLOSING)) {
    uv_tcp_queue_accept(server);
  }

  return rv;
}


int uv_tcp_read_start(uv_tcp_t* handle, uv_alloc_cb alloc_cb, uv_read_cb read_cb) {
  if (!(handle->flags & UV_HANDLE_CONNECTION)) {
    uv_set_sys_error(WSAEINVAL);
    return -1;
  }

  if (handle->flags & UV_HANDLE_READING) {
    uv_set_sys_error(WSAEALREADY);
    return -1;
  }

  if (handle->flags & UV_HANDLE_EOF) {
    uv_set_sys_error(WSAESHUTDOWN);
    return -1;
  }

  handle->flags |= UV_HANDLE_READING;
  handle->read_cb = read_cb;
  handle->alloc_cb = alloc_cb;

  /* If reading was stopped and then started again, there could stell be a */
  /* read request pending. */
  if (!(handle->flags & UV_HANDLE_READ_PENDING))
    uv_tcp_queue_read(handle);

  return 0;
}


int uv_tcp_connect(uv_connect_t* req, uv_tcp_t* handle,
    struct sockaddr_in address, uv_connect_cb cb) {
  int addrsize = sizeof(struct sockaddr_in);
  BOOL success;
  DWORD bytes;

  if (handle->flags & UV_HANDLE_BIND_ERROR) {
    LOOP->last_error = handle->error;
    return -1;
  }

  if (address.sin_family != AF_INET) {
    uv_set_sys_error(WSAEFAULT);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_tcp_bind(handle, uv_addr_ip4_any_) < 0)
    return -1;

  uv_req_init((uv_req_t*) req);
  req->type = UV_CONNECT;
  req->handle = (uv_stream_t*) handle;
  req->cb = cb;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

  success = pConnectEx(handle->socket,
                       (struct sockaddr*) &address,
                       addrsize,
                       NULL,
                       0,
                       &bytes,
                       &req->overlapped);

  if (!success && WSAGetLastError() != ERROR_IO_PENDING) {
    uv_set_sys_error(WSAGetLastError());
    return -1;
  }

  handle->reqs_pending++;

  return 0;
}


int uv_tcp_connect6(uv_connect_t* req, uv_tcp_t* handle,
    struct sockaddr_in6 address, uv_connect_cb cb) {
  int addrsize = sizeof(struct sockaddr_in6);
  BOOL success;
  DWORD bytes;

  if (!uv_allow_ipv6) {
    uv_new_sys_error(UV_EAFNOSUPPORT);
    return -1;
  }

  if (handle->flags & UV_HANDLE_BIND_ERROR) {
    LOOP->last_error = handle->error;
    return -1;
  }

  if (address.sin6_family != AF_INET6) {
    uv_set_sys_error(WSAEFAULT);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_tcp_bind6(handle, uv_addr_ip6_any_) < 0)
    return -1;

  uv_req_init((uv_req_t*) req);
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

  if (!success && WSAGetLastError() != ERROR_IO_PENDING) {
    uv_set_sys_error(WSAGetLastError());
    return -1;
  }

  handle->reqs_pending++;

  return 0;
}


int uv_getsockname(uv_tcp_t* handle, struct sockaddr* name, int* namelen) {
  int result;

  if (handle->flags & UV_HANDLE_SHUTTING) {
    uv_set_sys_error(WSAESHUTDOWN);
    return -1;
  }

  result = getsockname(handle->socket, name, namelen);
  if (result != 0) {
    uv_set_sys_error(WSAGetLastError());
    return -1;
  }

  return 0;
}


int uv_tcp_write(uv_write_t* req, uv_tcp_t* handle, uv_buf_t bufs[], int bufcnt,
    uv_write_cb cb) {
  int result;
  DWORD bytes, err;

  if (!(handle->flags & UV_HANDLE_CONNECTION)) {
    uv_set_sys_error(WSAEINVAL);
    return -1;
  }

  if (handle->flags & UV_HANDLE_SHUTTING) {
    uv_set_sys_error(WSAESHUTDOWN);
    return -1;
  }

  uv_req_init((uv_req_t*) req);
  req->type = UV_WRITE;
  req->handle = (uv_stream_t*) handle;
  req->cb = cb;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

  result = WSASend(handle->socket,
                   (WSABUF*)bufs,
                   bufcnt,
                   &bytes,
                   0,
                   &req->overlapped,
                   NULL);
  if (result != 0) {
    err = WSAGetLastError();
    if (err != WSA_IO_PENDING) {
      /* Send failed due to an error. */
      uv_set_sys_error(WSAGetLastError());
      return -1;
    }
  }

  if (result == 0) {
    /* Request completed immediately. */
    req->queued_bytes = 0;
  } else {
    /* Request queued by the kernel. */
    req->queued_bytes = uv_count_bufs(bufs, bufcnt);
    handle->write_queue_size += req->queued_bytes;
  }

  handle->reqs_pending++;
  handle->write_reqs_pending++;

  return 0;
}


void uv_process_tcp_read_req(uv_tcp_t* handle, uv_req_t* req) {
  DWORD bytes, flags, err;
  uv_buf_t buf;

  assert(handle->type == UV_TCP);

  handle->flags &= ~UV_HANDLE_READ_PENDING;

  if (req->error.code != UV_OK) {
    /* An error occurred doing the 0-read. */
    if ((handle->flags & UV_HANDLE_READING)) {
      handle->flags &= ~UV_HANDLE_READING;
      LOOP->last_error = req->error;
      buf.base = 0;
      buf.len = 0;
      handle->read_cb((uv_stream_t*)handle, -1, buf);
    }
  } else {
    /* Do nonblocking reads until the buffer is empty */
    while (handle->flags & UV_HANDLE_READING) {
      buf = handle->alloc_cb((uv_stream_t*)handle, 65536);
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
          handle->flags &= ~UV_HANDLE_READING;
          handle->flags |= UV_HANDLE_EOF;
          LOOP->last_error.code = UV_EOF;
          LOOP->last_error.sys_errno_ = ERROR_SUCCESS;
          handle->read_cb((uv_stream_t*)handle, -1, buf);
          break;
        }
      } else {
        err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK) {
          /* Read buffer was completely empty, report a 0-byte read. */
          uv_set_sys_error(WSAEWOULDBLOCK);
          handle->read_cb((uv_stream_t*)handle, 0, buf);
        } else {
          /* Ouch! serious error. */
          uv_set_sys_error(err);
          handle->read_cb((uv_stream_t*)handle, -1, buf);
        }
        break;
      }
    }

    /* Post another 0-read if still reading and not closing. */
    if ((handle->flags & UV_HANDLE_READING) &&
        !(handle->flags & UV_HANDLE_READ_PENDING)) {
      uv_tcp_queue_read(handle);
    }
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_tcp_write_req(uv_tcp_t* handle, uv_write_t* req) {
  assert(handle->type == UV_TCP);

  handle->write_queue_size -= req->queued_bytes;

  if (req->cb) {
    LOOP->last_error = req->error;
    ((uv_write_cb)req->cb)(req, LOOP->last_error.code == UV_OK ? 0 : -1);
  }

  handle->write_reqs_pending--;
  if (handle->flags & UV_HANDLE_SHUTTING &&
      handle->write_reqs_pending == 0) {
    uv_want_endgame((uv_handle_t*)handle);
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_tcp_accept_req(uv_tcp_t* handle, uv_req_t* req) {
  assert(handle->type == UV_TCP);

  /* If handle->accepted_socket is not a valid socket, then */
  /* uv_queue_accept must have failed. This is a serious error. We stop */
  /* accepting connections and report this error to the connection */
  /* callback. */
  if (handle->accept_socket == INVALID_SOCKET) {
    if (handle->flags & UV_HANDLE_LISTENING) {
      handle->flags &= ~UV_HANDLE_LISTENING;
      if (handle->connection_cb) {
        LOOP->last_error = req->error;
        handle->connection_cb((uv_handle_t*)handle, -1);
      }
    }
  } else if (req->error.code == UV_OK &&
      setsockopt(handle->accept_socket,
                  SOL_SOCKET,
                  SO_UPDATE_ACCEPT_CONTEXT,
                  (char*)&handle->socket,
                  sizeof(handle->socket)) == 0) {
    /* Accept and SO_UPDATE_ACCEPT_CONTEXT were successful. */
    if (handle->connection_cb) {
      handle->connection_cb((uv_handle_t*)handle, 0);
    }
  } else {
    /* Error related to accepted socket is ignored because the server */
    /* socket may still be healthy. If the server socket is broken
    /* uv_queue_accept will detect it. */
    closesocket(handle->accept_socket);
    if (handle->flags & UV_HANDLE_LISTENING) {
      uv_tcp_queue_accept(handle);
    }
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


void uv_process_tcp_connect_req(uv_tcp_t* handle, uv_connect_t* req) {
  assert(handle->type == UV_TCP);

  if (req->cb) {
    if (req->error.code == UV_OK) {
      if (setsockopt(handle->socket,
                      SOL_SOCKET,
                      SO_UPDATE_CONNECT_CONTEXT,
                      NULL,
                      0) == 0) {
        uv_connection_init((uv_stream_t*)handle);
        ((uv_connect_cb)req->cb)(req, 0);
      } else {
        uv_set_sys_error(WSAGetLastError());
        ((uv_connect_cb)req->cb)(req, -1);
      }
    } else {
      LOOP->last_error = req->error;
      ((uv_connect_cb)req->cb)(req, -1);
    }
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}
