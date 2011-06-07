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
#include <errno.h>
#include <limits.h>
#include <malloc.h>
#include <stdio.h>

#include "uv.h"
#include "tree.h"

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

  typedef BOOL(*LPFN_ACCEPTEX)
              (SOCKET sListenSocket,
               SOCKET sAcceptSocket,
               PVOID lpOutputBuffer,
               DWORD dwReceiveDataLength,
               DWORD dwLocalAddressLength,
               DWORD dwRemoteAddressLength,
               LPDWORD lpdwBytesReceived,
               LPOVERLAPPED lpOverlapped);

  typedef BOOL(*LPFN_CONNECTEX)
              (SOCKET s,
               const struct sockaddr* name,
               int namelen,
               PVOID lpSendBuffer,
               DWORD dwSendDataLength,
               LPDWORD lpdwBytesSent,
               LPOVERLAPPED lpOverlapped);

  typedef void(*LPFN_GETACCEPTEXSOCKADDRS)
              (PVOID lpOutputBuffer,
               DWORD dwReceiveDataLength,
               DWORD dwLocalAddressLength,
               DWORD dwRemoteAddressLength,
               LPSOCKADDR* LocalSockaddr,
               LPINT LocalSockaddrLength,
               LPSOCKADDR* RemoteSockaddr,
               LPINT RemoteSockaddrLength);

  typedef BOOL(*LPFN_DISCONNECTEX)
              (SOCKET hSocket,
               LPOVERLAPPED lpOverlapped,
               DWORD dwFlags,
               DWORD reserved);

  typedef BOOL(*LPFN_TRANSMITFILE)
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


/*
 * Pointers to winsock extension functions to be retrieved dynamically
 */
static LPFN_CONNECTEX               pConnectEx;
static LPFN_ACCEPTEX                pAcceptEx;
static LPFN_GETACCEPTEXSOCKADDRS    pGetAcceptExSockAddrs;
static LPFN_DISCONNECTEX            pDisconnectEx;
static LPFN_TRANSMITFILE            pTransmitFile;


/*
 * Private uv_handle flags
 */
#define UV_HANDLE_CLOSING          0x0001
#define UV_HANDLE_CLOSED           0x0002
#define UV_HANDLE_BOUND            0x0004
#define UV_HANDLE_LISTENING        0x0008
#define UV_HANDLE_CONNECTION       0x0010
#define UV_HANDLE_CONNECTED        0x0020
#define UV_HANDLE_READING          0x0040
#define UV_HANDLE_ACTIVE           0x0040
#define UV_HANDLE_EOF              0x0080
#define UV_HANDLE_SHUTTING         0x0100
#define UV_HANDLE_SHUT             0x0200
#define UV_HANDLE_ENDGAME_QUEUED   0x0400
#define UV_HANDLE_BIND_ERROR       0x1000

/*
 * Private uv_req flags.
 */
/* The request is currently queued. */
#define UV_REQ_PENDING             0x01


/* Binary tree used to keep the list of timers sorted. */
static int uv_timer_compare(uv_timer_t* handle1, uv_timer_t* handle2);
RB_HEAD(uv_timer_tree_s, uv_timer_s);
RB_PROTOTYPE_STATIC(uv_timer_tree_s, uv_timer_s, tree_entry, uv_timer_compare);

/* The head of the timers tree */
static struct uv_timer_tree_s uv_timers_ = RB_INITIALIZER(uv_timers_);


/* Lists of active uv_prepare / uv_check / uv_idle watchers */
static uv_handle_t* uv_prepare_handles_ = NULL;
static uv_handle_t* uv_check_handles_ = NULL;
static uv_handle_t* uv_idle_handles_ = NULL;

/* This pointer will refer to the prepare/check/idle handle whose callback */
/* is scheduled to be called next. This is needed to allow safe removal */
/* from one of the lists above while that list being iterated. */
static uv_handle_t* uv_next_loop_handle_ = NULL;


/* Head of a single-linked list of closed handles */
static uv_handle_t* uv_endgame_handles_ = NULL;


/* Tail of a single-linked circular queue of pending reqs. If the queue is */
/* empty, tail_ is NULL. If there is only one item, tail_->next_req == tail_ */
static uv_req_t* uv_pending_reqs_tail_ = NULL;


/* The current time according to the event loop. in msecs. */
static int64_t uv_now_ = 0;
static int64_t uv_ticks_per_msec_ = 0;


/*
 * Global I/O completion port
 */
static HANDLE uv_iocp_;


/* Global error code */
static const uv_err_t uv_ok_ = { UV_OK, ERROR_SUCCESS };
static uv_err_t uv_last_error_ = { UV_OK, ERROR_SUCCESS };

/* Error message string */
static char* uv_err_str_ = NULL;


/* Reference count that keeps the event loop alive */
static int uv_refs_ = 0;


/* Ip address used to bind to any port at any interface */
static struct sockaddr_in uv_addr_ip4_any_;


/* A zero-size buffer for use by uv_read */
static char uv_zero_[] = "";


/* Atomic set operation on char */
#ifdef _MSC_VER /* MSVC */

/* _InterlockedOr8 is supported by MSVC on x32 and x64. It is  slightly less */
/* efficient than InterlockedExchange, but InterlockedExchange8 does not */
/* exist, and interlocked operations on larger targets might require the */
/* target to be aligned. */
#pragma intrinsic(_InterlockedOr8)

static char __declspec(inline) uv_atomic_exchange_set(char volatile* target) {
  return _InterlockedOr8(target, 1);
}

#else /* GCC */

/* Mingw-32 version, hopefully this works for 64-bit gcc as well. */
static inline char uv_atomic_exchange_set(char volatile* target) {
  const char one = 1;
  char old_value;
  __asm__ __volatile__ ("lock xchgb %0, %1\n\t"
                        : "=r"(old_value), "=m"(*target)
                        : "0"(one), "m"(*target)
                        : "memory");
  return old_value;
}

#endif


/*
 * Display an error message and abort the event loop.
 */
static void uv_fatal_error(const int errorno, const char* syscall) {
  char* buf = NULL;
  const char* errmsg;

  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorno,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buf, 0, NULL);

  if (buf) {
    errmsg = buf;
  } else {
    errmsg = "Unknown error";
  }

  /* FormatMessage messages include a newline character already, */
  /* so don't add another. */
  if (syscall) {
    fprintf(stderr, "%s: (%d) %s", syscall, errorno, errmsg);
  } else {
    fprintf(stderr, "(%d) %s", errorno, errmsg);
  }

  if (buf) {
    LocalFree(buf);
  }

  *((char*)NULL) = 0xff; /* Force debug break */
  abort();
}


uv_err_t uv_last_error() {
  return uv_last_error_;
}


char* uv_strerror(uv_err_t err) {
  if (uv_err_str_ != NULL) {
    LocalFree((void*) uv_err_str_);
  }

  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS, NULL, err.sys_errno_,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&uv_err_str_, 0, NULL);

  if (uv_err_str_) {
    return uv_err_str_;
  } else {
    return "Unknown error";
  }
}


static uv_err_code uv_translate_sys_error(int sys_errno) {
  switch (sys_errno) {
    case ERROR_SUCCESS:                     return UV_OK;
    case ERROR_NOACCESS:                    return UV_EACCESS;
    case WSAEACCES:                         return UV_EACCESS;
    case ERROR_ADDRESS_ALREADY_ASSOCIATED:  return UV_EADDRINUSE;
    case WSAEADDRINUSE:                     return UV_EADDRINUSE;
    case WSAEADDRNOTAVAIL:                  return UV_EADDRNOTAVAIL;
    case WSAEWOULDBLOCK:                    return UV_EAGAIN;
    case WSAEALREADY:                       return UV_EALREADY;
    case ERROR_CONNECTION_REFUSED:          return UV_ECONNREFUSED;
    case WSAECONNREFUSED:                   return UV_ECONNREFUSED;
    case WSAEFAULT:                         return UV_EFAULT;
    case ERROR_INVALID_DATA:                return UV_EINVAL;
    case WSAEINVAL:                         return UV_EINVAL;
    case ERROR_TOO_MANY_OPEN_FILES:         return UV_EMFILE;
    case WSAEMFILE:                         return UV_EMFILE;
    case ERROR_OUTOFMEMORY:                 return UV_ENOMEM;
    default:                                return UV_UNKNOWN;
  }
}


static uv_err_t uv_new_sys_error(int sys_errno) {
  uv_err_t e;
  e.code = uv_translate_sys_error(sys_errno);
  e.sys_errno_ = sys_errno;
  return e;
}


static void uv_set_sys_error(int sys_errno) {
  uv_last_error_.code = uv_translate_sys_error(sys_errno);
  uv_last_error_.sys_errno_ = sys_errno;
}


/*
 * Retrieves the pointer to a winsock extension function.
 */
static void uv_get_extension_function(SOCKET socket, GUID guid,
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
    uv_fatal_error(WSAGetLastError(),
                   "WSAIoctl(SIO_GET_EXTENSION_FUNCTION_POINTER)");
  }
}


void uv_init() {
  const GUID wsaid_connectex            = WSAID_CONNECTEX;
  const GUID wsaid_acceptex             = WSAID_ACCEPTEX;
  const GUID wsaid_getacceptexsockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
  const GUID wsaid_disconnectex         = WSAID_DISCONNECTEX;
  const GUID wsaid_transmitfile         = WSAID_TRANSMITFILE;

  WSADATA wsa_data;
  int errorno;
  LARGE_INTEGER timer_frequency;
  SOCKET dummy;

  /* Initialize winsock */
  errorno = WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (errorno != 0) {
    uv_fatal_error(errorno, "WSAStartup");
  }

  /* Set implicit binding address used by connectEx */
  uv_addr_ip4_any_ = uv_ip4_addr("0.0.0.0", 0);

  /* Retrieve the needed winsock extension function pointers. */
  dummy = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
  if (dummy == INVALID_SOCKET) {
    uv_fatal_error(WSAGetLastError(), "socket");
  }

  uv_get_extension_function(dummy,
                            wsaid_connectex,
                            (void**)&pConnectEx);
  uv_get_extension_function(dummy,
                            wsaid_acceptex,
                            (void**)&pAcceptEx);
  uv_get_extension_function(dummy,
                            wsaid_getacceptexsockaddrs,
                            (void**)&pGetAcceptExSockAddrs);
  uv_get_extension_function(dummy,
                            wsaid_disconnectex,
                            (void**)&pDisconnectEx);
  uv_get_extension_function(dummy,
                            wsaid_transmitfile,
                            (void**)&pTransmitFile);

  if (closesocket(dummy) == SOCKET_ERROR) {
    uv_fatal_error(WSAGetLastError(), "closesocket");
  }

  /* Create an I/O completion port */
  uv_iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
  if (uv_iocp_ == NULL) {
    uv_fatal_error(GetLastError(), "CreateIoCompletionPort");
  }

  /* Initialize the event loop time */
  if (!QueryPerformanceFrequency(&timer_frequency))
    uv_fatal_error(GetLastError(), "QueryPerformanceFrequency");
  uv_ticks_per_msec_ = timer_frequency.QuadPart / 1000;

  uv_update_time();
}


void uv_req_init(uv_req_t* req, uv_handle_t* handle, void* cb) {
  req->type = UV_UNKNOWN_REQ;
  req->flags = 0;
  req->handle = handle;
  req->cb = cb;
}


static uv_req_t* uv_overlapped_to_req(OVERLAPPED* overlapped) {
  return CONTAINING_RECORD(overlapped, uv_req_t, overlapped);
}


static void uv_insert_pending_req(uv_req_t* req) {
  req->next_req = NULL;
  if (uv_pending_reqs_tail_) {
    req->next_req = uv_pending_reqs_tail_->next_req;
    uv_pending_reqs_tail_ = req;
  } else {
    req->next_req = req;
    uv_pending_reqs_tail_ = req;
  }
}


static uv_req_t* uv_remove_pending_req() {
  uv_req_t* req;

  if (uv_pending_reqs_tail_) {
    req = uv_pending_reqs_tail_->next_req;

    if (req == uv_pending_reqs_tail_) {
      uv_pending_reqs_tail_ = NULL;
    } else {
      uv_pending_reqs_tail_->next_req = req->next_req;
    }

    return req;

  } else {
    /* queue empty */
    return NULL;
  }
}


static int uv_tcp_init_socket(uv_tcp_t* handle, uv_close_cb close_cb,
    void* data, SOCKET socket) {
  DWORD yes = 1;

  handle->socket = socket;
  handle->close_cb = close_cb;
  handle->data = data;
  handle->write_queue_size = 0;
  handle->type = UV_TCP;
  handle->flags = 0;
  handle->reqs_pending = 0;
  handle->error = uv_ok_;
  handle->accept_socket = INVALID_SOCKET;

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
                             uv_iocp_,
                             (ULONG_PTR)socket,
                             0) == NULL) {
    uv_set_sys_error(GetLastError());
    return -1;
  }

  uv_refs_++;

  return 0;
}


static void uv_tcp_init_connection(uv_tcp_t* handle) {
  handle->flags |= UV_HANDLE_CONNECTION;
  handle->write_reqs_pending = 0;
  uv_req_init(&(handle->read_req), (uv_handle_t*)handle, NULL);
}


int uv_tcp_init(uv_tcp_t* handle, uv_close_cb close_cb, void* data) {
  SOCKET sock;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (handle->socket == INVALID_SOCKET) {
    uv_set_sys_error(WSAGetLastError());
    return -1;
  }

  if (uv_tcp_init_socket(handle, close_cb, data, sock) == -1) {
    closesocket(sock);
    return -1;
  }

  return 0;
}


static void uv_tcp_endgame(uv_tcp_t* handle) {
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
      handle->shutdown_req->flags &= ~UV_REQ_PENDING;
      if (status == -1) {
        uv_last_error_ = err;
      }
      ((uv_shutdown_cb)handle->shutdown_req->cb)(handle->shutdown_req, status);
    }
    handle->reqs_pending--;
  }

  if (handle->flags & UV_HANDLE_CLOSING &&
      handle->reqs_pending == 0) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    handle->flags |= UV_HANDLE_CLOSED;

    if (handle->close_cb) {
      uv_last_error_ = handle->error;
      handle->close_cb((uv_handle_t*)handle, handle->error.code == UV_OK ? 0 : 1);
    }

    uv_refs_--;
  }
}


static void uv_timer_endgame(uv_timer_t* handle) {
  if (handle->flags & UV_HANDLE_CLOSING) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    handle->flags |= UV_HANDLE_CLOSED;

    if (handle->close_cb) {
      handle->close_cb((uv_handle_t*)handle, 0);
    }

    uv_refs_--;
  }
}


static void uv_loop_endgame(uv_handle_t* handle) {
  if (handle->flags & UV_HANDLE_CLOSING) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    handle->flags |= UV_HANDLE_CLOSED;

    if (handle->close_cb) {
      handle->close_cb(handle, 0);
    }

    uv_refs_--;
  }
}


static void uv_async_endgame(uv_async_t* handle) {
  if (handle->flags & UV_HANDLE_CLOSING &&
      !handle->async_sent) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    handle->flags |= UV_HANDLE_CLOSED;

    if (handle->close_cb) {
      handle->close_cb((uv_handle_t*)handle, 0);
    }

    uv_refs_--;
  }
}


static void uv_process_endgames() {
  uv_handle_t* handle;

  while (uv_endgame_handles_) {
    handle = uv_endgame_handles_;
    uv_endgame_handles_ = handle->endgame_next;

    handle->flags &= ~UV_HANDLE_ENDGAME_QUEUED;

    switch (handle->type) {
      case UV_TCP:
        uv_tcp_endgame((uv_tcp_t*)handle);
        break;

      case UV_TIMER:
        uv_timer_endgame((uv_timer_t*)handle);
        break;

      case UV_PREPARE:
      case UV_CHECK:
      case UV_IDLE:
        uv_loop_endgame(handle);
        break;

      case UV_ASYNC:
        uv_async_endgame((uv_async_t*)handle);
        break;

      default:
        assert(0);
        break;
    }
  }
}


static void uv_want_endgame(uv_handle_t* handle) {
  if (!(handle->flags & UV_HANDLE_ENDGAME_QUEUED)) {
    handle->flags |= UV_HANDLE_ENDGAME_QUEUED;

    handle->endgame_next = uv_endgame_handles_;
    uv_endgame_handles_ = handle;
  }
}


static int uv_close_error(uv_handle_t* handle, uv_err_t e) {
	uv_tcp_t* tcp;

  if (handle->flags & UV_HANDLE_CLOSING) {
    return 0;
  }

  handle->error = e;
  handle->flags |= UV_HANDLE_CLOSING;

  /* Handle-specific close actions */
  switch (handle->type) {
    case UV_TCP:
			tcp = (uv_tcp_t*)handle;
      closesocket(tcp->socket);
      if (tcp->reqs_pending == 0) {
        uv_want_endgame(handle);
      }
      return 0;

    case UV_TIMER:
      uv_timer_stop((uv_timer_t*)handle);
      uv_want_endgame(handle);
      return 0;

    case UV_PREPARE:
      uv_prepare_stop((uv_prepare_t*)handle);
      uv_want_endgame(handle);
      return 0;

    case UV_CHECK:
      uv_check_stop((uv_check_t*)handle);
      uv_want_endgame(handle);
      return 0;

    case UV_IDLE:
      uv_idle_stop((uv_idle_t*)handle);
      uv_want_endgame(handle);
      return 0;

    case UV_ASYNC:
      if (!((uv_async_t*)handle)->async_sent) {
        uv_want_endgame(handle);
      }
      return 0;

    default:
      /* Not supported */
      assert(0);
      return -1;
  }
}


int uv_close(uv_handle_t* handle) {
  return uv_close_error(handle, uv_ok_);
}


struct sockaddr_in uv_ip4_addr(char* ip, int port) {
  struct sockaddr_in addr;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(ip);

  return addr;
}


int uv_bind(uv_tcp_t* handle, struct sockaddr_in addr) {
  DWORD err;
	int r;
  int addrsize = sizeof(struct sockaddr_in);

  if (addr.sin_family != AF_INET) {
    uv_set_sys_error(WSAEFAULT);
    return -1;
  }

	r = bind(handle->socket, (struct sockaddr*) &addr, addrsize);

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


static void uv_queue_accept(uv_tcp_t* handle) {
  uv_req_t* req;
  BOOL success;
  DWORD bytes;
  SOCKET accept_socket;

  assert(handle->flags & UV_HANDLE_LISTENING);
  assert(handle->accept_socket == INVALID_SOCKET);

  /* Prepare the uv_req structure. */
  req = &handle->accept_req;
  assert(!(req->flags & UV_REQ_PENDING));
  req->type = UV_ACCEPT;
  req->flags |= UV_REQ_PENDING;

  /* Open a socket for the accepted connection. */
  accept_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (accept_socket == INVALID_SOCKET) {
    req->error = uv_new_sys_error(WSAGetLastError());
    uv_insert_pending_req(req);
    return;
  }

  /* Prepare the overlapped structure. */
  memset(&(req->overlapped), 0, sizeof(req->overlapped));

  success = pAcceptEx(handle->socket,
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
    /* Destroy the preallocated client socket. */
    closesocket(accept_socket);
    return;
  }

  handle->accept_socket = accept_socket;

  handle->reqs_pending++;
  req->flags |= UV_REQ_PENDING;
}


static void uv_queue_read(uv_tcp_t* handle) {
  uv_req_t* req;
  uv_buf_t buf;
  int result;
  DWORD bytes, flags;

  assert(handle->flags & UV_HANDLE_READING);

  req = &handle->read_req;
  assert(!(req->flags & UV_REQ_PENDING));
  memset(&req->overlapped, 0, sizeof(req->overlapped));
  req->type = UV_READ;

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
    return;
  }

  req->flags |= UV_REQ_PENDING;
  handle->reqs_pending++;
}


int uv_listen(uv_tcp_t* handle, int backlog, uv_connection_cb cb) {
  assert(backlog > 0);

  if (handle->flags & UV_HANDLE_BIND_ERROR) {
    uv_last_error_ = handle->error;
    return -1;
  }

  if (handle->flags & UV_HANDLE_LISTENING ||
      handle->flags & UV_HANDLE_READING) {
    /* Already listening. */
    uv_set_sys_error(WSAEALREADY);
    return -1;
  }

  if (listen(handle->socket, backlog) == SOCKET_ERROR) {
    uv_set_sys_error(WSAGetLastError());
    return -1;
  }

  handle->flags |= UV_HANDLE_LISTENING;
  handle->connection_cb = cb;

  uv_req_init(&(handle->accept_req), (uv_handle_t*)handle, NULL);
  uv_queue_accept(handle);

  return 0;
}


int uv_accept(uv_tcp_t* server, uv_tcp_t* client,
    uv_close_cb close_cb, void* data) {
  int rv = 0;

  if (server->accept_socket == INVALID_SOCKET) {
    uv_set_sys_error(WSAENOTCONN);
    return -1;
  }

  if (uv_tcp_init_socket(client, close_cb, data, server->accept_socket) == -1) {
    closesocket(server->accept_socket);
    rv = -1;
  }

  uv_tcp_init_connection(client);

  server->accept_socket = INVALID_SOCKET;

  if (!(server->flags & UV_HANDLE_CLOSING)) {
    uv_queue_accept(server);
  }

  return rv;
}


int uv_read_start(uv_tcp_t* handle, uv_alloc_cb alloc_cb, uv_read_cb read_cb) {
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
  if (!(handle->read_req.flags & UV_REQ_PENDING))
    uv_queue_read(handle);

  return 0;
}


int uv_read_stop(uv_tcp_t* handle) {
  handle->flags &= ~UV_HANDLE_READING;

  return 0;
}


int uv_connect(uv_req_t* req, struct sockaddr_in addr) {
  int addrsize = sizeof(struct sockaddr_in);
  BOOL success;
  DWORD bytes;
  uv_tcp_t* handle = (uv_tcp_t*)req->handle;

  assert(!(req->flags & UV_REQ_PENDING));

  if (handle->flags & UV_HANDLE_BIND_ERROR) {
    uv_last_error_ = handle->error;
    return -1;
  }

  if (addr.sin_family != AF_INET) {
    uv_set_sys_error(WSAEFAULT);
    return -1;
  }

	if (!(handle->flags & UV_HANDLE_BOUND) &&
			uv_bind(handle, uv_addr_ip4_any_) < 0)
		return -1;

  memset(&req->overlapped, 0, sizeof(req->overlapped));
  req->type = UV_CONNECT;

  success = pConnectEx(handle->socket,
                       (struct sockaddr*)&addr,
                       addrsize,
                       NULL,
                       0,
                       &bytes,
                       &req->overlapped);

  if (!success && WSAGetLastError() != ERROR_IO_PENDING) {
    uv_set_sys_error(WSAGetLastError());
    return -1;
  }

  req->flags |= UV_REQ_PENDING;
  handle->reqs_pending++;

  return 0;
}


static size_t uv_count_bufs(uv_buf_t bufs[], int count) {
  size_t bytes = 0;
  int i;

  for (i = 0; i < count; i++) {
    bytes += (size_t)bufs[i].len;
  }

  return bytes;
}


int uv_write(uv_req_t* req, uv_buf_t bufs[], int bufcnt) {
  int result;
  DWORD bytes, err;
  uv_tcp_t* handle = (uv_tcp_t*) req->handle;

  assert(!(req->flags & UV_REQ_PENDING));

  if (!(req->handle->flags & UV_HANDLE_CONNECTION)) {
    uv_set_sys_error(WSAEINVAL);
    return -1;
  }

  if (req->handle->flags & UV_HANDLE_SHUTTING) {
    uv_set_sys_error(WSAESHUTDOWN);
    return -1;
  }

  memset(&req->overlapped, 0, sizeof(req->overlapped));
  req->type = UV_WRITE;

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

  req->flags |= UV_REQ_PENDING;
  handle->reqs_pending++;
  handle->write_reqs_pending++;

  return 0;
}


int uv_shutdown(uv_req_t* req) {
  uv_tcp_t* handle = (uv_tcp_t*) req->handle;
  int status = 0;

  if (!(req->handle->flags & UV_HANDLE_CONNECTION)) {
    uv_set_sys_error(WSAEINVAL);
    return -1;
  }

  if (handle->flags & UV_HANDLE_SHUTTING) {
    uv_set_sys_error(WSAESHUTDOWN);
    return -1;
  }

  req->type = UV_SHUTDOWN;
  req->flags |= UV_REQ_PENDING;

  handle->flags |= UV_HANDLE_SHUTTING;
    handle->shutdown_req = req;
  handle->reqs_pending++;

  uv_want_endgame((uv_handle_t*)handle);

  return 0;
}


static void uv_tcp_return_req(uv_tcp_t* handle, uv_req_t* req) {
  DWORD bytes, flags, err;
  uv_buf_t buf;

  assert(handle->type == UV_TCP);

  /* Mark the request non-pending */
  req->flags &= ~UV_REQ_PENDING;

  switch (req->type) {
    case UV_WRITE:
      handle->write_queue_size -= req->queued_bytes;
      if (req->cb) {
        uv_last_error_ = req->error;
        ((uv_write_cb)req->cb)(req, uv_last_error_.code == UV_OK ? 0 : -1);
      }
      handle->write_reqs_pending--;
      if (handle->write_reqs_pending == 0 &&
          handle->flags & UV_HANDLE_SHUTTING) {
        uv_want_endgame((uv_handle_t*)handle);
      }
      break;

    case UV_READ:
      if (req->error.code != UV_OK) {
        /* An error occurred doing the 0-read. */
        /* Stop reading and report error. */
        handle->flags &= ~UV_HANDLE_READING;
        uv_last_error_ = req->error;
        buf.base = 0;
        buf.len = 0;
        ((uv_read_cb)handle->read_cb)(handle, -1, buf);
        break;
      }

      /* Do nonblocking reads until the buffer is empty */
      while (handle->flags & UV_HANDLE_READING) {
        buf = handle->alloc_cb(handle, 65536);
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
            ((uv_read_cb)handle->read_cb)(handle, bytes, buf);
            /* Read again only if bytes == buf.len */
            if (bytes < buf.len) {
              break;
            }
          } else {
            /* Connection closed */
            handle->flags &= ~UV_HANDLE_READING;
            handle->flags |= UV_HANDLE_EOF;
            uv_last_error_.code = UV_EOF;
            uv_last_error_.sys_errno_ = ERROR_SUCCESS;
            ((uv_read_cb)handle->read_cb)(handle, -1, buf);
            break;
          }
        } else {
          err = WSAGetLastError();
          if (err == WSAEWOULDBLOCK) {
            /* Read buffer was completely empty, report a 0-byte read. */
            uv_set_sys_error(WSAEWOULDBLOCK);
            ((uv_read_cb)handle->read_cb)(handle, 0, buf);
          } else {
            /* Ouch! serious error. */
            uv_set_sys_error(err);
            ((uv_read_cb)handle->read_cb)(handle, -1, buf);
          }
          break;
        }
      }
      /* Post another 0-read if still reading and not closing. */
      if (!(handle->flags & UV_HANDLE_CLOSING) &&
          handle->flags & UV_HANDLE_READING) {
        uv_queue_read(handle);
      }
      break;

    case UV_ACCEPT:
      /* If handle->accepted_socket is not a valid socket, then */
      /* uv_queue_accept must have failed. This is a serious error. We stop */
      /* accepting connections and report this error to the connection */
      /* callback. */
      if (handle->accept_socket == INVALID_SOCKET) {
        handle->flags &= ~UV_HANDLE_LISTENING;
        if (handle->connection_cb) {
          uv_last_error_ = req->error;
          ((uv_connection_cb)handle->connection_cb)(handle, -1);
        }
        break;
      }

      if (req->error.code == UV_OK &&
          setsockopt(handle->accept_socket,
                     SOL_SOCKET,
                     SO_UPDATE_ACCEPT_CONTEXT,
                     (char*)&handle->socket,
                     sizeof(handle->socket)) == 0) {
        /* Accept and SO_UPDATE_ACCEPT_CONTEXT were successful. */
        if (handle->connection_cb) {
          ((uv_connection_cb)handle->connection_cb)(handle, 0);
        }
      } else {
        /* Error related to accepted socket is ignored because the server */
        /* socket may still be healthy. If the server socket is broken
        /* uv_queue_accept will detect it. */
        closesocket(handle->accept_socket);
        if (!(handle->flags & UV_HANDLE_CLOSING)) {
          uv_queue_accept(handle);
        }
      }
      break;

    case UV_CONNECT:
      if (req->cb) {
        if (req->error.code == UV_OK) {
          if (setsockopt(handle->socket,
                         SOL_SOCKET,
                         SO_UPDATE_CONNECT_CONTEXT,
                         NULL,
                         0) == 0) {
            uv_tcp_init_connection(handle);
            ((uv_connect_cb)req->cb)(req, 0);
          } else {
            uv_set_sys_error(WSAGetLastError());
            ((uv_connect_cb)req->cb)(req, -1);
          }
        } else {
          uv_last_error_ = req->error;
          ((uv_connect_cb)req->cb)(req, -1);
        }
      }
      break;

    default:
      assert(0);
  }

   /* The number of pending requests is now down by one */
  handle->reqs_pending--;

  /* Queue the handle's close callback if it is closing and there are no */
  /* more pending requests. */
  if (handle->flags & UV_HANDLE_CLOSING &&
      handle->reqs_pending == 0) {
    uv_want_endgame((uv_handle_t*)handle);
  }
}


static int uv_timer_compare(uv_timer_t* a, uv_timer_t* b) {
  if (a->due < b->due)
    return -1;
  if (a->due > b->due)
    return 1;
  if ((intptr_t)a < (intptr_t)b)
    return -1;
  if ((intptr_t)a > (intptr_t)b)
    return 1;
  return 0;
}


RB_GENERATE_STATIC(uv_timer_tree_s, uv_timer_s, tree_entry, uv_timer_compare);


int uv_timer_init(uv_timer_t* handle, uv_close_cb close_cb, void* data) {
  handle->type = UV_TIMER;
  handle->close_cb = (void*) close_cb;
  handle->data = data;
  handle->flags = 0;
  handle->error = uv_ok_;
  handle->timer_cb = NULL;
  handle->repeat = 0;

  uv_refs_++;

  return 0;
}


int uv_timer_start(uv_timer_t* handle, uv_loop_cb timer_cb, int64_t timeout, int64_t repeat) {
  if (handle->flags & UV_HANDLE_ACTIVE) {
    RB_REMOVE(uv_timer_tree_s, &uv_timers_, handle);
  }

  handle->timer_cb = (void*) timer_cb;
  handle->due = uv_now_ + timeout;
  handle->repeat = repeat;
  handle->flags |= UV_HANDLE_ACTIVE;

  if (RB_INSERT(uv_timer_tree_s, &uv_timers_, handle) != NULL) {
    uv_fatal_error(ERROR_INVALID_DATA, "RB_INSERT");
  }

  return 0;
}


int uv_timer_stop(uv_timer_t* handle) {
  if (!(handle->flags & UV_HANDLE_ACTIVE))
    return 0;

  RB_REMOVE(uv_timer_tree_s, &uv_timers_, handle);

  handle->flags &= ~UV_HANDLE_ACTIVE;

  return 0;
}


int uv_timer_again(uv_timer_t* handle) {
  /* If timer_cb is NULL that means that the timer was never started. */
  if (!handle->timer_cb) {
    uv_set_sys_error(ERROR_INVALID_DATA);
    return -1;
  }

  if (handle->flags & UV_HANDLE_ACTIVE) {
    RB_REMOVE(uv_timer_tree_s, &uv_timers_, handle);
    handle->flags &= ~UV_HANDLE_ACTIVE;
  }

  if (handle->repeat) {
    handle->due = uv_now_ + handle->repeat;

    if (RB_INSERT(uv_timer_tree_s, &uv_timers_, handle) != NULL) {
      uv_fatal_error(ERROR_INVALID_DATA, "RB_INSERT");
    }

    handle->flags |= UV_HANDLE_ACTIVE;
  }

  return 0;
}


void uv_timer_set_repeat(uv_timer_t* handle, int64_t repeat) {
  assert(handle->type == UV_TIMER);
  handle->repeat = repeat;
}


int64_t uv_timer_get_repeat(uv_timer_t* handle) {
  assert(handle->type == UV_TIMER);
  return handle->repeat;
}


void uv_update_time() {
  LARGE_INTEGER counter;

  if (!QueryPerformanceCounter(&counter))
    uv_fatal_error(GetLastError(), "QueryPerformanceCounter");

  uv_now_ = counter.QuadPart / uv_ticks_per_msec_;
}


int64_t uv_now() {
  return uv_now_;
}


int uv_loop_init(uv_handle_t* handle, uv_close_cb close_cb, void* data) {
  handle->close_cb = (void*) close_cb;
  handle->data = data;
  handle->flags = 0;
  handle->error = uv_ok_;

  uv_refs_++;

  return 0;
}


static int uv_loop_start(uv_handle_t* handle, uv_loop_cb loop_cb,
    uv_handle_t** list) {
  uv_handle_t* old_head;

  if (handle->flags & UV_HANDLE_ACTIVE)
    return 0;

  old_head = *list;

  handle->loop_next = old_head;
  handle->loop_prev = NULL;

  if (old_head) {
    old_head->loop_prev = handle;
  }

  *list = handle;

  handle->loop_cb = loop_cb;
  handle->flags |= UV_HANDLE_ACTIVE;

  return 0;
}


static int uv_loop_stop(uv_handle_t* handle, uv_handle_t** list) {
  if (!(handle->flags & UV_HANDLE_ACTIVE))
    return 0;

  /* Update loop head if needed */
  if (*list == handle) {
    *list = handle->loop_next;
  }

  /* Update the iterator-next pointer of needed */
  if (uv_next_loop_handle_ == handle) {
    uv_next_loop_handle_ = handle->loop_next;
  }

  if (handle->loop_prev) {
    handle->loop_prev->loop_next = handle->loop_next;
  }
  if (handle->loop_next) {
    handle->loop_next->loop_prev = handle->loop_prev;
  }

  handle->flags &= ~UV_HANDLE_ACTIVE;

  return 0;
}


static void uv_loop_invoke(uv_handle_t* list) {
  uv_handle_t *handle;

  uv_next_loop_handle_ = list;

  while (uv_next_loop_handle_ != NULL) {
    handle = uv_next_loop_handle_;
    uv_next_loop_handle_ = handle->loop_next;

    ((uv_loop_cb)handle->loop_cb)(handle, 0);
  }
}


int uv_prepare_init(uv_prepare_t* handle, uv_close_cb close_cb, void* data) {
  handle->type = UV_PREPARE;
  return uv_loop_init((uv_handle_t*)handle, close_cb, data);
}


int uv_check_init(uv_check_t* handle, uv_close_cb close_cb, void* data) {
  handle->type = UV_CHECK;
  return uv_loop_init((uv_handle_t*)handle, close_cb, data);
}


int uv_idle_init(uv_idle_t* handle, uv_close_cb close_cb, void* data) {
  handle->type = UV_IDLE;
  return uv_loop_init((uv_handle_t*)handle, close_cb, data);
}


int uv_prepare_start(uv_prepare_t* handle, uv_loop_cb loop_cb) {
  assert(handle->type == UV_PREPARE);
  return uv_loop_start((uv_handle_t*)handle, loop_cb, &uv_prepare_handles_);
}


int uv_check_start(uv_check_t* handle, uv_loop_cb loop_cb) {
  assert(handle->type == UV_CHECK);
  return uv_loop_start((uv_handle_t*)handle, loop_cb, &uv_check_handles_);
}


int uv_idle_start(uv_idle_t* handle, uv_loop_cb loop_cb) {
  assert(handle->type == UV_IDLE);
  return uv_loop_start((uv_handle_t*)handle, loop_cb, &uv_idle_handles_);
}


int uv_prepare_stop(uv_prepare_t* handle) {
  assert(handle->type == UV_PREPARE);
  return uv_loop_stop((uv_handle_t*)handle, &uv_prepare_handles_);
}


int uv_check_stop(uv_check_t* handle) {
  assert(handle->type == UV_CHECK);
  return uv_loop_stop((uv_handle_t*)handle, &uv_check_handles_);
}


int uv_idle_stop(uv_idle_t* handle) {
  assert(handle->type == UV_IDLE);
  return uv_loop_stop((uv_handle_t*)handle, &uv_idle_handles_);
}


int uv_is_active(uv_handle_t* handle) {
  switch (handle->type) {
    case UV_TIMER:
    case UV_IDLE:
    case UV_PREPARE:
    case UV_CHECK:
      return (handle->flags & UV_HANDLE_ACTIVE) ? 1 : 0;

    default:
      return 1;
  }
}


int uv_async_init(uv_async_t* handle, uv_async_cb async_cb,
                   uv_close_cb close_cb, void* data) {
  uv_req_t* req;

  handle->type = UV_ASYNC;
  handle->close_cb = (void*) close_cb;
  handle->data = data;
  handle->flags = 0;
  handle->async_sent = 0;
  handle->error = uv_ok_;

  req = &handle->async_req;
  uv_req_init(req, (uv_handle_t*)handle, async_cb);
  req->type = UV_WAKEUP;

  uv_refs_++;

  return 0;
}


int uv_async_send(uv_async_t* handle) {
  if (handle->type != UV_ASYNC) {
    /* Can't set errno because that's not thread-safe. */
    return -1;
  }

  /* The user should make sure never to call uv_async_send to a closing */
  /* or closed handle. */
  assert(!(handle->flags & UV_HANDLE_CLOSING));

  if (!uv_atomic_exchange_set(&handle->async_sent)) {
    if (!PostQueuedCompletionStatus(uv_iocp_,
                                    0,
                                    0,
                                    &handle->async_req.overlapped)) {
      uv_fatal_error(GetLastError(), "PostQueuedCompletionStatus");
    }
  }

  return 0;
}


static void uv_async_return_req(uv_async_t* handle, uv_req_t* req) {
  assert(handle->type == UV_ASYNC);
  assert(req->type == UV_WAKEUP);

  handle->async_sent = 0;
  if (req->cb) {
    ((uv_async_cb)req->cb)((uv_handle_t*)handle, 0);
  }
  if (handle->flags & UV_HANDLE_CLOSING) {
    uv_want_endgame((uv_handle_t*)handle);
  }
}


static void uv_process_reqs() {
  uv_req_t* req;
  uv_handle_t* handle;

  while (req = uv_remove_pending_req()) {
    handle = req->handle;

    switch (handle->type) {
      case UV_TCP:
        uv_tcp_return_req((uv_tcp_t*)handle, req);
        break;

      case UV_ASYNC:
        uv_async_return_req((uv_async_t*)handle, req);
        break;

      default:
        assert(0);
    }
  }
}


static void uv_process_timers() {
  uv_timer_t* timer;

  /* Call timer callbacks */
  for (timer = RB_MIN(uv_timer_tree_s, &uv_timers_);
       timer != NULL && timer->due <= uv_now_;
       timer = RB_MIN(uv_timer_tree_s, &uv_timers_)) {
    RB_REMOVE(uv_timer_tree_s, &uv_timers_, timer);

    if (timer->repeat != 0) {
      /* If it is a repeating timer, reschedule with repeat timeout. */
      timer->due += timer->repeat;
      if (timer->due < uv_now_) {
        timer->due = uv_now_;
      }
      if (RB_INSERT(uv_timer_tree_s, &uv_timers_, timer) != NULL) {
        uv_fatal_error(ERROR_INVALID_DATA, "RB_INSERT");
      }
    } else {
      /* If non-repeating, mark the timer as inactive. */
      timer->flags &= ~UV_HANDLE_ACTIVE;
    }

    ((uv_loop_cb) timer->timer_cb)((uv_handle_t*)timer, 0);
  }
}


static DWORD uv_get_poll_timeout() {
  uv_timer_t* timer;
  int64_t delta;

  /* Check if there are any running timers */
  timer = RB_MIN(uv_timer_tree_s, &uv_timers_);
  if (timer) {
    uv_update_time();

    delta = timer->due - uv_now_;
    if (delta >= UINT_MAX) {
      /* Can't have a timeout greater than UINT_MAX, and a timeout value of */
      /* UINT_MAX means infinite, so that's no good either. */
      return UINT_MAX - 1;
    } else if (delta < 0) {
      /* Negative timeout values are not allowed */
      return 0;
    } else {
      return (DWORD)delta;
    }
  } else {
    /* No timers */
    return INFINITE;
  }
}


static void uv_poll() {
  BOOL success;
  DWORD bytes;
  ULONG_PTR key;
  OVERLAPPED* overlapped;
  uv_req_t* req;

  success = GetQueuedCompletionStatus(uv_iocp_,
                                      &bytes,
                                      &key,
                                      &overlapped,
                                      uv_get_poll_timeout());

  uv_update_time();

  if (overlapped) {
    /* Package was dequeued */
    req = uv_overlapped_to_req(overlapped);

    if (success) {
      req->error = uv_ok_;
    } else {
      req->error = uv_new_sys_error(GetLastError());
    }

    uv_insert_pending_req(req);

  } else if (GetLastError() != WAIT_TIMEOUT) {
    /* Serious error */
    uv_fatal_error(GetLastError(), "GetQueuedCompletionStatus");
  }
}


int uv_run() {
  while (1) {
    uv_update_time();
    uv_process_timers();

    /* Terrible: please fix me! */
    while (uv_refs_ > 0 &&
        (uv_idle_handles_ || uv_pending_reqs_tail_ || uv_endgame_handles_)) {
      /* Terrible: please fix me! */
      while (uv_pending_reqs_tail_ || uv_endgame_handles_) {
        uv_process_endgames();
        uv_process_reqs();
      }

      /* Call idle callbacks */
      uv_loop_invoke(uv_idle_handles_);
    }

    if (uv_refs_ <= 0) {
      break;
    }

    uv_loop_invoke(uv_prepare_handles_);

    uv_poll();

    uv_loop_invoke(uv_check_handles_);
  }

  assert(uv_refs_ == 0);
  return 0;
}


void uv_ref() {
  uv_refs_++;
}


void uv_unref() {
  uv_refs_--;
}


int uv_utf16_to_utf8(wchar_t* utf16Buffer, size_t utf16Size, char* utf8Buffer, size_t utf8Size) {
  return WideCharToMultiByte(CP_UTF8, 0, utf16Buffer, utf16Size, utf8Buffer, utf8Size, NULL, NULL);
}


int uv_get_exepath(char* buffer, size_t* size) {
  int retVal;
  size_t utf16Size;
  wchar_t* utf16Buffer;

  if (!buffer || !size) {
    return -1;
  }

  utf16Buffer = (wchar_t*)malloc(sizeof(wchar_t) * *size);
  if (!utf16Buffer) {
    retVal = -1;
    goto done;
  }

  /* Get the path as UTF-16 */
  utf16Size = GetModuleFileNameW(NULL, utf16Buffer, *size - 1);
  if (utf16Size <= 0) {
    uv_set_sys_error(GetLastError());
    retVal = -1;
    goto done;
  }

  utf16Buffer[utf16Size] = L'\0';

  /* Convert to UTF-8 */
  *size = uv_utf16_to_utf8(utf16Buffer, utf16Size, buffer, *size);
  if (!*size) {
    uv_set_sys_error(GetLastError());
    retVal = -1;
    goto done;
  }

  buffer[*size] = '\0';
  retVal = 0;

done:
  if (utf16Buffer) {
    free(utf16Buffer);
  }

  return retVal;
}
