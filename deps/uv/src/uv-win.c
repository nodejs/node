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
#include "uv-common.h"
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


/*
 * MinGW is missing this too
 */
#ifndef _MSC_VER
  typedef struct addrinfoW {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    size_t ai_addrlen;
    wchar_t* ai_canonname;
    struct sockaddr* ai_addr;
    struct addrinfoW* ai_next;
  } ADDRINFOW, *PADDRINFOW;

  DECLSPEC_IMPORT int WSAAPI GetAddrInfoW(const wchar_t* node,
                                          const wchar_t* service,
                                          const ADDRINFOW* hints,
                                          PADDRINFOW* result);

  DECLSPEC_IMPORT void WSAAPI FreeAddrInfoW(PADDRINFOW pAddrInfo);
#endif


/*
 * Pointers to winsock extension functions to be retrieved dynamically
 */
static LPFN_CONNECTEX               pConnectEx;
static LPFN_ACCEPTEX                pAcceptEx;
static LPFN_GETACCEPTEXSOCKADDRS    pGetAcceptExSockAddrs;
static LPFN_DISCONNECTEX            pDisconnectEx;
static LPFN_TRANSMITFILE            pTransmitFile;
/* need IPv6 versions of winsock extension functions */
static LPFN_CONNECTEX               pConnectEx6;
static LPFN_ACCEPTEX                pAcceptEx6;
static LPFN_GETACCEPTEXSOCKADDRS    pGetAcceptExSockAddrs6;
static LPFN_DISCONNECTEX            pDisconnectEx6;
static LPFN_TRANSMITFILE            pTransmitFile6;


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
#define UV_HANDLE_IPV6             0x2000
#define UV_HANDLE_PIPESERVER       0x4000

/*
 * Private uv_req flags.
 */
/* The request is currently queued. */
#define UV_REQ_PENDING             0x01


/* Binary tree used to keep the list of timers sorted. */
static int uv_timer_compare(uv_timer_t* handle1, uv_timer_t* handle2);
RB_HEAD(uv_timer_tree_s, uv_timer_s);
RB_PROTOTYPE_STATIC(uv_timer_tree_s, uv_timer_s, tree_entry, uv_timer_compare)

/* The head of the timers tree */
static struct uv_timer_tree_s uv_timers_ = RB_INITIALIZER(uv_timers_);


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
static struct sockaddr_in6 uv_addr_ip6_any_;


/* A zero-size buffer for use by uv_read */
static char uv_zero_[] = "";


/* mark if IPv6 sockets are supported */
static BOOL uv_allow_ipv6 = FALSE;

/*
 * Subclass of uv_handle_t. Used for integration of c-ares.
 */
typedef struct uv_ares_action_s uv_ares_action_t;

struct uv_ares_action_s {
  UV_HANDLE_FIELDS
  struct uv_req_s ares_req;
  SOCKET sock;
  int read;
  int write;
};

void uv_process_ares_event_req(uv_ares_action_t* handle, uv_req_t* req);
void uv_process_ares_cleanup_req(uv_ares_task_t* handle, uv_req_t* req);
void uv_ares_poll(uv_timer_t* handle, int status);

static void close_pipe(uv_pipe_t* handle, int* status, uv_err_t* err);

/* memory used per ares_channel */
struct uv_ares_channel_s {
  ares_channel channel;
  int activesockets;
  uv_timer_t pollingtimer;
};

typedef struct uv_ares_channel_s uv_ares_channel_t;

/* static data to hold single ares_channel */
static uv_ares_channel_t uv_ares_data = { NULL, 0 };

/* default timeout per socket request if ares does not specify value */
/* use 20 sec */
#define ARES_TIMEOUT_MS            20000


/* getaddrinfo integration */
static void uv_process_getaddrinfo_req(uv_getaddrinfo_t* handle, uv_req_t* req);
/* adjust size value to be multiple of 4. Use to keep pointer aligned */
/* Do we need different versions of this for different architectures? */
#define ALIGNED_SIZE(X)     ((((X) + 3) >> 2) << 2)


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
    case ERROR_INSUFFICIENT_BUFFER:         return UV_EINVAL;
    case ERROR_INVALID_FLAGS:               return UV_EBADF;
    case ERROR_INVALID_PARAMETER:           return UV_EINVAL;
    case ERROR_NO_UNICODE_TRANSLATION:      return UV_ECHARSET;
    case ERROR_BROKEN_PIPE:                 return UV_EOF;
    case ERROR_PIPE_BUSY:                   return UV_EBUSY;
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


void uv_req_init(uv_req_t* req, uv_handle_t* handle, void *(*cb)(void *)) {
  uv_counters()->req_init++;
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
    uv_pending_reqs_tail_->next_req = req;
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
                             uv_iocp_,
                             (ULONG_PTR)socket,
                             0) == NULL) {
    uv_set_sys_error(GetLastError());
    return -1;
  }

  handle->socket = socket;

  return 0;
}


static void uv_init_connection(uv_stream_t* handle) {
  handle->flags |= UV_HANDLE_CONNECTION;
  handle->write_reqs_pending = 0;
  uv_req_init(&(handle->read_req), (uv_handle_t*)handle, NULL);
}


int uv_stream_init(uv_stream_t* handle) {
  handle->write_queue_size = 0;
  handle->flags = 0;
  handle->error = uv_ok_;

  uv_counters()->handle_init++;
  uv_counters()->stream_init++;

  uv_refs_++;

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
      handle->close_cb((uv_handle_t*)handle);
    }

    uv_refs_--;
  }
}


static void uv_pipe_endgame(uv_pipe_t* handle) {
  uv_err_t err;
  int status;

  if (handle->flags & UV_HANDLE_SHUTTING &&
      !(handle->flags & UV_HANDLE_SHUT) &&
      handle->write_reqs_pending == 0) {
    close_pipe(handle, &status, &err);

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
      handle->close_cb((uv_handle_t*)handle);
    }

    uv_refs_--;
  }
}


static void uv_timer_endgame(uv_timer_t* handle) {
  if (handle->flags & UV_HANDLE_CLOSING) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    handle->flags |= UV_HANDLE_CLOSED;

    if (handle->close_cb) {
      handle->close_cb((uv_handle_t*)handle);
    }

    uv_refs_--;
  }
}


static void uv_loop_watcher_endgame(uv_handle_t* handle) {
  if (handle->flags & UV_HANDLE_CLOSING) {
    assert(!(handle->flags & UV_HANDLE_CLOSED));
    handle->flags |= UV_HANDLE_CLOSED;

    if (handle->close_cb) {
      handle->close_cb(handle);
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
      handle->close_cb((uv_handle_t*)handle);
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

      case UV_NAMED_PIPE:
        uv_pipe_endgame((uv_pipe_t*)handle);
        break;

      case UV_TIMER:
        uv_timer_endgame((uv_timer_t*)handle);
        break;

      case UV_PREPARE:
      case UV_CHECK:
      case UV_IDLE:
        uv_loop_watcher_endgame(handle);
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
  uv_pipe_t* pipe;

  if (handle->flags & UV_HANDLE_CLOSING) {
    return 0;
  }

  handle->error = e;
  handle->flags |= UV_HANDLE_CLOSING;

  /* Handle-specific close actions */
  switch (handle->type) {
    case UV_TCP:
      tcp = (uv_tcp_t*)handle;
      /* If we don't shutdown before calling closesocket, windows will */
      /* silently discard the kernel send buffer and reset the connection. */
      if (!(tcp->flags & UV_HANDLE_SHUT)) {
        shutdown(tcp->socket, SD_SEND);
        tcp->flags |= UV_HANDLE_SHUT;
      }
      tcp->flags &= ~(UV_HANDLE_READING | UV_HANDLE_LISTENING);
      closesocket(tcp->socket);
      if (tcp->reqs_pending == 0) {
        uv_want_endgame(handle);
      }
      return 0;

    case UV_NAMED_PIPE:
      pipe = (uv_pipe_t*)handle;
      pipe->flags &= ~(UV_HANDLE_READING | UV_HANDLE_LISTENING);
      close_pipe(pipe, NULL, NULL);
      if (pipe->reqs_pending == 0) {
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


int uv_close(uv_handle_t* handle, uv_close_cb close_cb) {
  handle->close_cb = close_cb;
  return uv_close_error(handle, uv_ok_);
}


int uv__bind(uv_tcp_t* handle, int domain, struct sockaddr* addr, int addrsize) {
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
  assert(!(req->flags & UV_REQ_PENDING));
  req->type = UV_ACCEPT;
  req->flags |= UV_REQ_PENDING;

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
  req->flags |= UV_REQ_PENDING;
}


static void uv_pipe_queue_accept(uv_pipe_t* handle) {
  uv_req_t* req;
  HANDLE pipeHandle;
  int i;

  assert(handle->flags & UV_HANDLE_LISTENING);

  for (i = 0; i < COUNTOF(handle->accept_reqs); i++) {
    req = &handle->accept_reqs[i];
    if (!(req->flags & UV_REQ_PENDING)) {
      pipeHandle = CreateNamedPipe(handle->name,
                                   PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                   PIPE_UNLIMITED_INSTANCES,
                                   65536,
                                   65536,
                                   0,
                                   NULL);

      if (pipeHandle == INVALID_HANDLE_VALUE) {
        continue;
      }

      if (CreateIoCompletionPort(pipeHandle,
                                 uv_iocp_,
                                 (ULONG_PTR)handle,
                                 0) == NULL) {
        continue;
      }

      /* Prepare the overlapped structure. */
      memset(&(req->overlapped), 0, sizeof(req->overlapped));

      if (!ConnectNamedPipe(pipeHandle, &req->overlapped) && 
          GetLastError() != ERROR_IO_PENDING && GetLastError() != ERROR_PIPE_CONNECTED) {
        /* Make this req pending reporting an error. */
        req->error = uv_new_sys_error(GetLastError());
        uv_insert_pending_req(req);
        handle->reqs_pending++;
        continue;
      }

      req->data = pipeHandle;
      req->flags |= UV_REQ_PENDING;
      handle->reqs_pending++;
    }
  }
}


static void uv_tcp_queue_read(uv_tcp_t* handle) {
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
    handle->reqs_pending++;
    return;
  }

  req->flags |= UV_REQ_PENDING;
  handle->reqs_pending++;
}


static void uv_pipe_queue_read(uv_pipe_t* handle) {
  uv_req_t* req;
  int result;

  assert(handle->flags & UV_HANDLE_READING);
  assert(handle->connection);
  assert(handle->connection->handle != INVALID_HANDLE_VALUE);

  req = &handle->read_req;
  assert(!(req->flags & UV_REQ_PENDING));
  memset(&req->overlapped, 0, sizeof(req->overlapped));
  req->type = UV_READ;

  /* Do 0-read */
  result = ReadFile(handle->connection->handle,
                    &uv_zero_,
                    0,
                    NULL,
                    &req->overlapped);

  if (!result && GetLastError() != ERROR_IO_PENDING) {
    /* Make this req pending reporting an error. */
    req->error = uv_new_sys_error(WSAGetLastError());
    uv_insert_pending_req(req);
    handle->reqs_pending++;
    return;
  }

  req->flags |= UV_REQ_PENDING;
  handle->reqs_pending++;
}


int uv_tcp_listen(uv_tcp_t* handle, int backlog, uv_connection_cb cb) {
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
  uv_tcp_queue_accept(handle);

  return 0;
}


static int uv_tcp_accept(uv_tcp_t* server, uv_tcp_t* client) {
  int rv = 0;

  if (server->accept_socket == INVALID_SOCKET) {
    uv_set_sys_error(WSAENOTCONN);
    return -1;
  }

  if (uv_tcp_set_socket(client, server->accept_socket) == -1) {
    closesocket(server->accept_socket);
    rv = -1;
  } else {
    uv_init_connection((uv_stream_t*)client);
  }

  server->accept_socket = INVALID_SOCKET;

  if (!(server->flags & UV_HANDLE_CLOSING)) {
    uv_tcp_queue_accept(server);
  }

  return rv;
}


static int uv_pipe_accept(uv_pipe_t* server, uv_pipe_t* client) {
  uv_pipe_instance_t* connection = server->connections;

  /* Find a connection instance that has been connected, but not yet accepted. */
  while (connection) {
    if (connection->state == UV_PIPEINSTANCE_CONNECTED) {
      break;
    }

    connection = connection->next;
  }

  if (!connection) {
    /* No valid connections found, so we error out. */
    uv_set_sys_error(UV_ENOTCONN);
    return -1;
  }

  /* Make the connection instance active */
  connection->state = UV_PIPEINSTANCE_ACTIVE;
  
  /* Assign the connection to the client. */
  client->connection = connection;
  client->server = server;

  uv_init_connection((uv_stream_t*)client);
  client->flags |= UV_HANDLE_PIPESERVER;
  uv_req_init(&(client->read_req), (uv_handle_t*)client, NULL);

  if (!(server->flags & UV_HANDLE_CLOSING)) {
    uv_pipe_queue_accept(server);
  }

  return 0;
}


int uv_accept(uv_handle_t* server, uv_stream_t* client) {
  assert(client->type == server->type);

  if (server->type == UV_TCP) {
    return uv_tcp_accept((uv_tcp_t*)server, (uv_tcp_t*)client);
  } else if (server->type == UV_NAMED_PIPE) {
    return uv_pipe_accept((uv_pipe_t*)server, (uv_pipe_t*)client);
  }

  return -1;
}


static int uv_tcp_read_start(uv_tcp_t* handle, uv_alloc_cb alloc_cb, uv_read_cb read_cb) {
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
    uv_tcp_queue_read(handle);

  return 0;
}


static int uv_pipe_read_start(uv_pipe_t* handle, uv_alloc_cb alloc_cb, uv_read_cb read_cb) {
  if (!(handle->flags & UV_HANDLE_CONNECTION)) {
    uv_set_sys_error(UV_EINVAL);
    return -1;
  }

  if (handle->flags & UV_HANDLE_READING) {
    uv_set_sys_error(UV_EALREADY);
    return -1;
  }

  if (handle->flags & UV_HANDLE_EOF) {
    uv_set_sys_error(UV_EOF);
    return -1;
  }

  handle->flags |= UV_HANDLE_READING;
  handle->read_cb = read_cb;
  handle->alloc_cb = alloc_cb;

  /* If reading was stopped and then started again, there could stell be a */
  /* read request pending. */
  if (!(handle->read_req.flags & UV_REQ_PENDING))
    uv_pipe_queue_read(handle);

  return 0;
}


int uv_read_start(uv_stream_t* handle, uv_alloc_cb alloc_cb, uv_read_cb read_cb) {
  if (handle->type == UV_TCP) {
    return uv_tcp_read_start((uv_tcp_t*)handle, alloc_cb, read_cb);
  } else if (handle->type == UV_NAMED_PIPE) {
    return uv_pipe_read_start((uv_pipe_t*)handle, alloc_cb, read_cb);
  }

  return -1;
}


int uv_read_stop(uv_stream_t* handle) {
  handle->flags &= ~UV_HANDLE_READING;

  return 0;
}


int uv_tcp_connect(uv_req_t* req, struct sockaddr_in addr) {
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
      uv_tcp_bind(handle, uv_addr_ip4_any_) < 0)
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


int uv_tcp_connect6(uv_req_t* req, struct sockaddr_in6 addr) {
  int addrsize = sizeof(struct sockaddr_in6);
  BOOL success;
  DWORD bytes;
  uv_tcp_t* handle = (uv_tcp_t*)req->handle;

  if (!uv_allow_ipv6) {
    uv_new_sys_error(UV_EAFNOSUPPORT);
    return -1;
  }

  assert(!(req->flags & UV_REQ_PENDING));

  if (handle->flags & UV_HANDLE_BIND_ERROR) {
    uv_last_error_ = handle->error;
    return -1;
  }

  if (addr.sin6_family != AF_INET6) {
    uv_set_sys_error(WSAEFAULT);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_BOUND) &&
      uv_tcp_bind6(handle, uv_addr_ip6_any_) < 0)
    return -1;

  memset(&req->overlapped, 0, sizeof(req->overlapped));
  req->type = UV_CONNECT;

  success = pConnectEx6(handle->socket,
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


static size_t uv_count_bufs(uv_buf_t bufs[], int count) {
  size_t bytes = 0;
  int i;

  for (i = 0; i < count; i++) {
    bytes += (size_t)bufs[i].len;
  }

  return bytes;
}


int uv_tcp_write(uv_req_t* req, uv_buf_t bufs[], int bufcnt) {
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


int uv_pipe_write(uv_req_t* req, uv_buf_t bufs[], int bufcnt) {
  int result;
  uv_pipe_t* handle = (uv_pipe_t*) req->handle;

  assert(!(req->flags & UV_REQ_PENDING));

  if (bufcnt != 1) {
    uv_set_sys_error(UV_ENOTSUP);
    return -1;
  }

  assert(handle->connection);
  assert(handle->connection->handle != INVALID_HANDLE_VALUE);

  if (!(req->handle->flags & UV_HANDLE_CONNECTION)) {
    uv_set_sys_error(UV_EINVAL);
    return -1;
  }

  if (req->handle->flags & UV_HANDLE_SHUTTING) {
    uv_set_sys_error(UV_EOF);
    return -1;
  }

  memset(&req->overlapped, 0, sizeof(req->overlapped));
  req->type = UV_WRITE;

  result = WriteFile(handle->connection->handle,
                     bufs[0].base,
                     bufs[0].len,
                     NULL,
                     &req->overlapped);

  if (!result && GetLastError() != WSA_IO_PENDING) {
    uv_set_sys_error(GetLastError());
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

  req->flags |= UV_REQ_PENDING;
  handle->reqs_pending++;
  handle->write_reqs_pending++;

  return 0;
}


int uv_write(uv_req_t* req, uv_buf_t bufs[], int bufcnt) {
  if (req->handle->type == UV_TCP) {
    return uv_tcp_write(req, bufs, bufcnt);
  } else if (req->handle->type == UV_NAMED_PIPE) {
    return uv_pipe_write(req, bufs, bufcnt);
  }

  return -1;
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


#define DECREASE_PENDING_REQ_COUNT(handle)    \
  do {                                        \
    handle->reqs_pending--;                   \
                                              \
    if (handle->flags & UV_HANDLE_CLOSING &&  \
        handle->reqs_pending == 0) {          \
      uv_want_endgame((uv_handle_t*)handle);  \
    }                                         \
  } while (0)


static void uv_process_tcp_read_req(uv_tcp_t* handle, uv_req_t* req) {
  DWORD bytes, flags, err;
  uv_buf_t buf;

  assert(handle->type == UV_TCP);

  /* Mark the request non-pending */
  req->flags &= ~UV_REQ_PENDING;

  if (req->error.code != UV_OK) {
    /* An error occurred doing the 0-read. */
    if ((handle->flags & UV_HANDLE_READING)) {
      handle->flags &= ~UV_HANDLE_READING;
      uv_last_error_ = req->error;
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
          uv_last_error_.code = UV_EOF;
          uv_last_error_.sys_errno_ = ERROR_SUCCESS;
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
    if (handle->flags & UV_HANDLE_READING) {
      uv_tcp_queue_read(handle);
    }
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


static void uv_process_tcp_write_req(uv_tcp_t* handle, uv_req_t* req) {
  assert(handle->type == UV_TCP);

  /* Mark the request non-pending */
  req->flags &= ~UV_REQ_PENDING;

  handle->write_queue_size -= req->queued_bytes;

  if (req->cb) {
    uv_last_error_ = req->error;
    ((uv_write_cb)req->cb)(req, uv_last_error_.code == UV_OK ? 0 : -1);
  }

  handle->write_reqs_pending--;
  if (handle->flags & UV_HANDLE_SHUTTING &&
      handle->write_reqs_pending == 0) {
    uv_want_endgame((uv_handle_t*)handle);
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


static void uv_process_tcp_accept_req(uv_tcp_t* handle, uv_req_t* req) {
  assert(handle->type == UV_TCP);

  /* Mark the request non-pending */
  req->flags &= ~UV_REQ_PENDING;

  /* If handle->accepted_socket is not a valid socket, then */
  /* uv_queue_accept must have failed. This is a serious error. We stop */
  /* accepting connections and report this error to the connection */
  /* callback. */
  if (handle->accept_socket == INVALID_SOCKET) {
    if (handle->flags & UV_HANDLE_LISTENING) {
      handle->flags &= ~UV_HANDLE_LISTENING;
      if (handle->connection_cb) {
        uv_last_error_ = req->error;
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


static void uv_process_tcp_connect_req(uv_tcp_t* handle, uv_req_t* req) {
  assert(handle->type == UV_TCP);

  /* Mark the request non-pending */
  req->flags &= ~UV_REQ_PENDING;

  if (req->cb) {
    if (req->error.code == UV_OK) {
      if (setsockopt(handle->socket,
                      SOL_SOCKET,
                      SO_UPDATE_CONNECT_CONTEXT,
                      NULL,
                      0) == 0) {
        uv_init_connection((uv_stream_t*)handle);
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

  DECREASE_PENDING_REQ_COUNT(handle);
}


static void uv_process_pipe_read_req(uv_pipe_t* handle, uv_req_t* req) {
  DWORD bytes, err, mode;
  uv_buf_t buf;

  assert(handle->type == UV_NAMED_PIPE);

  /* Mark the request non-pending */
  req->flags &= ~UV_REQ_PENDING;

  if (req->error.code != UV_OK) {
    /* An error occurred doing the 0-read. */
    if (handle->flags & UV_HANDLE_READING) {
      /* Stop reading and report error. */
      handle->flags &= ~UV_HANDLE_READING;
      uv_last_error_ = req->error;
      buf.base = 0;
      buf.len = 0;
      handle->read_cb((uv_stream_t*)handle, -1, buf);
    }
  } else {
    /*
      * Temporarily switch to non-blocking mode.
      * This is so that ReadFile doesn't block if the read buffer is empty.
      */
    mode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_NOWAIT;
    if (!SetNamedPipeHandleState(handle->connection->handle, &mode, NULL, NULL)) {
      /* We can't continue processing this read. */
      handle->flags &= ~UV_HANDLE_READING;
      uv_set_sys_error(GetLastError());
      buf.base = 0;
      buf.len = 0;
      handle->read_cb((uv_stream_t*)handle, -1, buf);
    }

    /* Do non-blocking reads until the buffer is empty */
    while (handle->flags & UV_HANDLE_READING) {
      buf = handle->alloc_cb((uv_stream_t*)handle, 65536);
      assert(buf.len > 0);

      if (ReadFile(handle->connection->handle,
                  buf.base,
                  buf.len,
                  &bytes,
                  NULL)) {
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
          uv_last_error_.code = UV_EOF;
          uv_last_error_.sys_errno_ = ERROR_SUCCESS;
          handle->read_cb((uv_stream_t*)handle, -1, buf);
          break;
        }
      } else {
        err = GetLastError();
        if (err == ERROR_NO_DATA) {
          /* Read buffer was completely empty, report a 0-byte read. */
          uv_set_sys_error(UV_EAGAIN);
          handle->read_cb((uv_stream_t*)handle, 0, buf);
        } else {
          /* Ouch! serious error. */
          uv_set_sys_error(err);
          handle->read_cb((uv_stream_t*)handle, -1, buf);
        }
        break;
      }
    }

    /* TODO: if the read callback stops reading we can't start reading again
        because the pipe will still be in nowait mode. */
    if (handle->flags & UV_HANDLE_READING) {
      /* Switch back to blocking mode so that we can use IOCP for 0-reads */
      mode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;
      if (SetNamedPipeHandleState(handle->connection->handle, &mode, NULL, NULL)) {
        /* Post another 0-read */
        uv_pipe_queue_read(handle);
      } else {
        /* Report and continue. */
        /* We can't continue processing this read. */
        handle->flags &= ~UV_HANDLE_READING;
        uv_set_sys_error(GetLastError());
        buf.base = 0;
        buf.len = 0;
        handle->read_cb((uv_stream_t*)handle, -1, buf);
      }
    }
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


static void uv_process_pipe_write_req(uv_pipe_t* handle, uv_req_t* req) {
  assert(handle->type == UV_NAMED_PIPE);

  /* Mark the request non-pending */
  req->flags &= ~UV_REQ_PENDING;

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

  DECREASE_PENDING_REQ_COUNT(handle);
}


static void uv_process_pipe_accept_req(uv_pipe_t* handle, uv_req_t* req) {
  uv_pipe_instance_t* pipeInstance;

  assert(handle->type == UV_NAMED_PIPE);

  /* Mark the request non-pending */
  req->flags &= ~UV_REQ_PENDING;

  if (req->error.code == UV_OK) {
    assert(req->data);

    /* Create the connection instance and add it to the connections list. */
    pipeInstance = (uv_pipe_instance_t*)malloc(sizeof(uv_pipe_instance_t));
    if (!pipeInstance) {
      uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
    }

    pipeInstance->handle = req->data;
    pipeInstance->state = UV_PIPEINSTANCE_CONNECTED;
    pipeInstance->next = handle->connections;
    handle->connections = pipeInstance;

    /* Clear the request. */
    req->data = NULL;
    req->flags = 0;

    if (handle->connection_cb) {
      handle->connection_cb((uv_handle_t*)handle, 0);
    }
  } else {
    /* Ignore errors and continue listening */
    if (handle->flags & UV_HANDLE_LISTENING) {
      uv_pipe_queue_accept(handle);
    }
  }

  DECREASE_PENDING_REQ_COUNT(handle);
}


static void uv_process_pipe_connect_req(uv_pipe_t* handle, uv_req_t* req) {
  assert(handle->type == UV_NAMED_PIPE);

  /* Mark the request non-pending */
  req->flags &= ~UV_REQ_PENDING;

  if (req->cb) {
    if (req->error.code == UV_OK) {
      uv_init_connection((uv_stream_t*)handle);
      ((uv_connect_cb)req->cb)(req, 0);
    } else {
      uv_last_error_ = req->error;
      ((uv_connect_cb)req->cb)(req, -1);
    }
  }

  DECREASE_PENDING_REQ_COUNT(handle);
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


int uv_timer_init(uv_timer_t* handle) {
  uv_counters()->handle_init++;
  uv_counters()->timer_init++;

  handle->type = UV_TIMER;
  handle->flags = 0;
  handle->error = uv_ok_;
  handle->timer_cb = NULL;
  handle->repeat = 0;

  uv_refs_++;

  return 0;
}


int uv_timer_start(uv_timer_t* handle, uv_timer_cb timer_cb, int64_t timeout, int64_t repeat) {
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


#define UV_LOOP_WATCHER_DEFINE(name, NAME)                                    \
  /* Lists of active loop (prepare / check / idle) watchers */                \
  static uv_##name##_t* uv_##name##_handles_ = NULL;                          \
                                                                              \
  /* This pointer will refer to the prepare/check/idle handle whose */        \
  /* callback is scheduled to be called next. This is needed to allow */      \
  /* safe removal from one of the lists above while that list being */        \
  /* iterated over. */                                                        \
  static uv_##name##_t* uv_next_##name##_handle_ = NULL;                      \
                                                                              \
                                                                              \
  int uv_##name##_init(uv_##name##_t* handle) {                               \
    handle->type = UV_##NAME;                                                 \
    handle->flags = 0;                                                        \
    handle->error = uv_ok_;                                                   \
                                                                              \
    uv_refs_++;                                                               \
                                                                              \
    uv_counters()->handle_init++;                                             \
    uv_counters()->prepare_init++;                                            \
                                                                              \
    return 0;                                                                 \
  }                                                                           \
                                                                              \
                                                                              \
  int uv_##name##_start(uv_##name##_t* handle, uv_##name##_cb cb) {           \
    uv_##name##_t* old_head;                                                  \
                                                                              \
    assert(handle->type == UV_##NAME);                                        \
                                                                              \
    if (handle->flags & UV_HANDLE_ACTIVE)                                     \
      return 0;                                                               \
                                                                              \
    old_head = uv_##name##_handles_;                                          \
                                                                              \
    handle->name##_next = old_head;                                           \
    handle->name##_prev = NULL;                                               \
                                                                              \
    if (old_head) {                                                           \
      old_head->name##_prev = handle;                                         \
    }                                                                         \
                                                                              \
    uv_##name##_handles_ = handle;                                            \
                                                                              \
    handle->name##_cb = cb;                                                   \
    handle->flags |= UV_HANDLE_ACTIVE;                                        \
                                                                              \
    return 0;                                                                 \
  }                                                                           \
                                                                              \
                                                                              \
  int uv_##name##_stop(uv_##name##_t* handle) {                               \
    assert(handle->type == UV_##NAME);                                        \
                                                                              \
    if (!(handle->flags & UV_HANDLE_ACTIVE))                                  \
      return 0;                                                               \
                                                                              \
    /* Update loop head if needed */                                          \
    if (uv_##name##_handles_ == handle) {                                     \
      uv_##name##_handles_ = handle->name##_next;                             \
    }                                                                         \
                                                                              \
    /* Update the iterator-next pointer of needed */                          \
    if (uv_next_##name##_handle_ == handle) {                                 \
      uv_next_##name##_handle_ = handle->name##_next;                         \
    }                                                                         \
                                                                              \
    if (handle->name##_prev) {                                                \
      handle->name##_prev->name##_next = handle->name##_next;                 \
    }                                                                         \
    if (handle->name##_next) {                                                \
      handle->name##_next->name##_prev = handle->name##_prev;                 \
    }                                                                         \
                                                                              \
    handle->flags &= ~UV_HANDLE_ACTIVE;                                       \
                                                                              \
    return 0;                                                                 \
  }                                                                           \
                                                                              \
                                                                              \
  static void uv_##name##_invoke() {                                          \
    uv_##name##_t* handle;                                                    \
                                                                              \
    uv_next_##name##_handle_ = uv_##name##_handles_;                          \
                                                                              \
    while (uv_next_##name##_handle_ != NULL) {                                \
      handle = uv_next_##name##_handle_;                                      \
      uv_next_##name##_handle_ = handle->name##_next;                         \
                                                                              \
      handle->name##_cb(handle, 0);                                           \
    }                                                                         \
  }


UV_LOOP_WATCHER_DEFINE(prepare, PREPARE)
UV_LOOP_WATCHER_DEFINE(check, CHECK)
UV_LOOP_WATCHER_DEFINE(idle, IDLE)


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


int uv_async_init(uv_async_t* handle, uv_async_cb async_cb) {
  uv_req_t* req;

  uv_counters()->handle_init++;
  uv_counters()->async_init++;

  handle->type = UV_ASYNC;
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


static void uv_process_async_wakeup_req(uv_async_t* handle, uv_req_t* req) {
  assert(handle->type == UV_ASYNC);
  assert(req->type == UV_WAKEUP);

  handle->async_sent = 0;
  if (req->cb) {
    ((uv_async_cb)req->cb)((uv_async_t*) handle, 0);
  }
  if (handle->flags & UV_HANDLE_CLOSING) {
    uv_want_endgame((uv_handle_t*)handle);
  }
}


#define DELEGATE_STREAM_REQ(req, method)                                      \
  do {                                                                        \
    switch (req->handle->type) {                                              \
      case UV_TCP:                                                            \
        uv_process_tcp_##method##_req((uv_tcp_t*) req->handle, req);          \
        break;                                                                \
                                                                              \
      case UV_NAMED_PIPE:                                                     \
        uv_process_pipe_##method##_req((uv_pipe_t*) req->handle, req);        \
        break;                                                                \
                                                                              \
      default:                                                                \
        assert(0);                                                            \
    }                                                                         \
  } while (0)


static void uv_process_reqs() {
  uv_req_t* req;

  while (req = uv_remove_pending_req()) {
    switch (req->type) {
      case UV_READ:
        DELEGATE_STREAM_REQ(req, read);
        break;

      case UV_WRITE:
        DELEGATE_STREAM_REQ(req, write);
        break;

      case UV_ACCEPT:
        DELEGATE_STREAM_REQ(req, accept);
        break;

      case UV_CONNECT:
        DELEGATE_STREAM_REQ(req, connect);
        break;

      case UV_WAKEUP:
        uv_process_async_wakeup_req((uv_async_t*) req->handle, req);
        break;

      case UV_ARES_EVENT_REQ:
        uv_process_ares_event_req((uv_ares_action_t*) req->handle, req);
        break;

      case UV_ARES_CLEANUP_REQ:
        uv_process_ares_cleanup_req((uv_ares_task_t*) req->handle, req);
        break;

      case UV_GETADDRINFO_REQ:
        uv_process_getaddrinfo_req((uv_getaddrinfo_t*) req->handle, req);
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

    timer->timer_cb((uv_timer_t*) timer, 0);
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
      uv_idle_invoke();
    }

    if (uv_refs_ <= 0) {
      break;
    }

    uv_prepare_invoke();

    uv_poll();

    uv_check_invoke();
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

int uv_utf8_to_utf16(const char* utf8Buffer, wchar_t* utf16Buffer, size_t utf16Size) {
  return MultiByteToWideChar(CP_UTF8, 0, utf8Buffer, -1, utf16Buffer, utf16Size);
}


int uv_exepath(char* buffer, size_t* size) {
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


uint64_t uv_hrtime(void) {
  assert(0 && "implement me");
}


/* thread pool callback when socket is signalled */
VOID CALLBACK uv_ares_socksignal_tp(void* parameter, BOOLEAN timerfired) {
  WSANETWORKEVENTS network_events;
  uv_ares_task_t* sockhandle;
  uv_ares_action_t* selhandle;
  uv_req_t* uv_ares_req;

  assert(parameter != NULL);

  if (parameter != NULL) {
    sockhandle = (uv_ares_task_t*)parameter;

    /* clear socket status for this event */
    /* do not fail if error, thread may run after socket close */
    /* The code assumes that c-ares will write all pending data in the callback,
       unless the socket would block. We can clear the state here to avoid unecessary
       signals. */
    WSAEnumNetworkEvents(sockhandle->sock, sockhandle->h_event, &network_events);

    /* setup new handle */
    selhandle = (uv_ares_action_t*)malloc(sizeof(uv_ares_action_t));
    if (selhandle == NULL) {
      uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
    }
    selhandle->type = UV_ARES_EVENT;
    selhandle->close_cb = NULL;
    selhandle->data = sockhandle->data;
    selhandle->sock = sockhandle->sock;
    selhandle->read = (network_events.lNetworkEvents & (FD_READ | FD_CONNECT)) ? 1 : 0;
    selhandle->write = (network_events.lNetworkEvents & (FD_WRITE | FD_CONNECT)) ? 1 : 0;

    uv_ares_req = &selhandle->ares_req;
    uv_req_init(uv_ares_req, (uv_handle_t*)selhandle, NULL);
    uv_ares_req->type = UV_ARES_EVENT_REQ;

    /* post ares needs to called */
    if (!PostQueuedCompletionStatus(uv_iocp_,
                                    0,
                                    0,
                                    &uv_ares_req->overlapped)) {
      uv_fatal_error(GetLastError(), "PostQueuedCompletionStatus");
    }
  }
}

/* callback from ares when socket operation is started */
void uv_ares_sockstate_cb(void *data, ares_socket_t sock, int read, int write) {
  /* look to see if we have a handle for this socket in our list */
  uv_ares_task_t* uv_handle_ares = uv_find_ares_handle(sock);
  uv_ares_channel_t* uv_ares_data_ptr = (uv_ares_channel_t*)data;

  int timeoutms = 0;

  if (read == 0 && write == 0) {
    /* if read and write are 0, cleanup existing data */
    /* The code assumes that c-ares does a callback with read = 0 and write = 0
       when the socket is closed. After we recieve this we stop monitoring the socket. */
    if (uv_handle_ares != NULL) {
      uv_req_t* uv_ares_req;

      uv_handle_ares->h_close_event = CreateEvent(NULL, FALSE, FALSE, NULL);
      /* remove Wait */
      if (uv_handle_ares->h_wait) {
        UnregisterWaitEx(uv_handle_ares->h_wait, uv_handle_ares->h_close_event);
        uv_handle_ares->h_wait = NULL;
      }

      /* detach socket from the event */
      WSAEventSelect(sock, NULL, 0);
      if (uv_handle_ares->h_event != WSA_INVALID_EVENT) {
        WSACloseEvent(uv_handle_ares->h_event);
        uv_handle_ares->h_event = WSA_INVALID_EVENT;
      }
      /* remove handle from list */
      uv_remove_ares_handle(uv_handle_ares);

      /* Post request to cleanup the Task */
      uv_ares_req = &uv_handle_ares->ares_req;
      uv_req_init(uv_ares_req, (uv_handle_t*)uv_handle_ares, NULL);
      uv_ares_req->type = UV_ARES_CLEANUP_REQ;

      /* post ares done with socket - finish cleanup when all threads done. */
      if (!PostQueuedCompletionStatus(uv_iocp_,
                                      0,
                                      0,
                                      &uv_ares_req->overlapped)) {
        uv_fatal_error(GetLastError(), "PostQueuedCompletionStatus");
      }
    } else {
      assert(0);
      uv_fatal_error(ERROR_INVALID_DATA, "ares_SockStateCB");
    }
  } else {
    if (uv_handle_ares == NULL) {
      /* setup new handle */
      /* The code assumes that c-ares will call us when it has an open socket.
        We need to call into c-ares when there is something to read,
        or when it becomes writable. */
      uv_handle_ares = (uv_ares_task_t*)malloc(sizeof(uv_ares_task_t));
      if (uv_handle_ares == NULL) {
        uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
      }
      uv_handle_ares->type = UV_ARES_TASK;
      uv_handle_ares->close_cb = NULL;
      uv_handle_ares->data = uv_ares_data_ptr;
      uv_handle_ares->sock = sock;
      uv_handle_ares->h_wait = NULL;
      uv_handle_ares->flags = 0;

      /* create an event to wait on socket signal */
      uv_handle_ares->h_event = WSACreateEvent();
      if (uv_handle_ares->h_event == WSA_INVALID_EVENT) {
        uv_fatal_error(WSAGetLastError(), "WSACreateEvent");
      }

      /* tie event to socket */
      if (SOCKET_ERROR == WSAEventSelect(sock, uv_handle_ares->h_event, FD_READ | FD_WRITE | FD_CONNECT)) {
        uv_fatal_error(WSAGetLastError(), "WSAEventSelect");
      }

      /* add handle to list */
      uv_add_ares_handle(uv_handle_ares);
      uv_refs_++;

      /*
       * we have a single polling timer for all ares sockets.
       * This is preferred to using ares_timeout. See ares_timeout.c warning.
       * if timer is not running start it, and keep socket count
       */
      if (uv_ares_data_ptr->activesockets == 0) {
        uv_timer_init(&uv_ares_data_ptr->pollingtimer);
        uv_timer_start(&uv_ares_data_ptr->pollingtimer, uv_ares_poll, 1000L, 1000L);
      }
      uv_ares_data_ptr->activesockets++;

      /* specify thread pool function to call when event is signaled */
      if (RegisterWaitForSingleObject(&uv_handle_ares->h_wait,
                                  uv_handle_ares->h_event,
                                  uv_ares_socksignal_tp,
                                  (void*)uv_handle_ares,
                                  INFINITE,
                                  WT_EXECUTEINWAITTHREAD) == 0) {
        uv_fatal_error(GetLastError(), "RegisterWaitForSingleObject");
      }
    } else {
      /* found existing handle.  */
      assert(uv_handle_ares->type == UV_ARES_TASK);
      assert(uv_handle_ares->data != NULL);
      assert(uv_handle_ares->h_event != WSA_INVALID_EVENT);
    }
  }
}

/* called via uv_poll when ares completion port signaled */
void uv_process_ares_event_req(uv_ares_action_t* handle, uv_req_t* req) {
  uv_ares_channel_t* uv_ares_data_ptr = (uv_ares_channel_t*)handle->data;

  ares_process_fd(uv_ares_data_ptr->channel,
                    handle->read ? handle->sock : INVALID_SOCKET,
                    handle->write ?  handle->sock : INVALID_SOCKET);

  /* release handle for select here  */
  free(handle);
}

/* called via uv_poll when ares is finished with socket */
void uv_process_ares_cleanup_req(uv_ares_task_t* handle, uv_req_t* req) {
  /* check for event complete without waiting */
  unsigned int signaled = WaitForSingleObject(handle->h_close_event, 0);

  if (signaled != WAIT_TIMEOUT) {
    uv_ares_channel_t* uv_ares_data_ptr = (uv_ares_channel_t*)handle->data;

    uv_refs_--;

    /* close event handle and free uv handle memory */
    CloseHandle(handle->h_close_event);
    free(handle);

    /* decrement active count. if it becomes 0 stop polling */
    if (uv_ares_data_ptr->activesockets > 0) {
      uv_ares_data_ptr->activesockets--;
      if (uv_ares_data_ptr->activesockets == 0) {
        uv_close((uv_handle_t*)&uv_ares_data_ptr->pollingtimer, NULL);
      }
    }
  } else {
    /* stil busy - repost and try again */
    if (!PostQueuedCompletionStatus(uv_iocp_,
                                    0,
                                    0,
                                    &req->overlapped)) {
      uv_fatal_error(GetLastError(), "PostQueuedCompletionStatus");
    }
  }
}

/* periodically call ares to check for timeouts */
void uv_ares_poll(uv_timer_t* handle, int status) {
  if (uv_ares_data.channel != NULL && uv_ares_data.activesockets > 0) {
    ares_process_fd(uv_ares_data.channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
  }
}


/* set ares SOCK_STATE callback to our handler */
int uv_ares_init_options(ares_channel *channelptr,
                        struct ares_options *options,
                        int optmask) {
  int rc;

  /* only allow single init at a time */
  if (uv_ares_data.channel != NULL) {
    return UV_EALREADY;
  }

  /* set our callback as an option */
  options->sock_state_cb = uv_ares_sockstate_cb;
  options->sock_state_cb_data = &uv_ares_data;
  optmask |= ARES_OPT_SOCK_STATE_CB;

  /* We do the call to ares_init_option for caller. */
  rc = ares_init_options(channelptr, options, optmask);

  /* if success, save channel */
  if (rc == ARES_SUCCESS) {
    uv_ares_data.channel = *channelptr;
  }

  return rc;
}


/* release memory */
void uv_ares_destroy(ares_channel channel) {
  /* only allow destroy if did init */
  if (uv_ares_data.channel != NULL) {
    ares_destroy(channel);
    uv_ares_data.channel = NULL;
  }
}


/*
 * getaddrinfo error code mapping
 * Falls back to uv_translate_sys_error if no match
 */

static uv_err_code uv_translate_eai_error(int eai_errno) {
  switch (eai_errno) {
    case ERROR_SUCCESS:                   return UV_OK;
    case EAI_BADFLAGS:                    return UV_EBADF;
    case EAI_FAIL:                        return UV_EFAULT;
    case EAI_FAMILY:                      return UV_EAIFAMNOSUPPORT;
    case EAI_MEMORY:                      return UV_ENOMEM;
    case EAI_NONAME:                      return UV_EAINONAME;
    case EAI_AGAIN:                       return UV_EAGAIN;
    case EAI_SERVICE:                     return UV_EAISERVICE;
    case EAI_SOCKTYPE:                    return UV_EAISOCKTYPE;
    default:                              return uv_translate_sys_error(eai_errno);
  }
}


/* getaddrinfo worker thread implementation */
static DWORD WINAPI getaddrinfo_thread_proc(void* parameter) {
  uv_getaddrinfo_t* handle = (uv_getaddrinfo_t*)parameter;
  int ret;

  assert(handle != NULL);

  if (handle != NULL) {
    /* call OS function on this thread */
    ret = GetAddrInfoW(handle->node, handle->service, handle->hints, &handle->res);
    handle->retcode = ret;

    /* post getaddrinfo completed */
    if (!PostQueuedCompletionStatus(uv_iocp_,
                                  0,
                                  0,
                                  &handle->getadddrinfo_req.overlapped)) {
      uv_fatal_error(GetLastError(), "PostQueuedCompletionStatus");
    }
  }

  return 0;
}


/*
 * Called from uv_run when complete. Call user specified callback
 * then free returned addrinfo
 * Returned addrinfo strings are converted from UTF-16 to UTF-8.
 *
 * To minimize allocation we calculate total size required,
 * and copy all structs and referenced strings into the one block.
 * Each size calculation is adjusted to avoid unaligned pointers.
 */
static void uv_process_getaddrinfo_req(uv_getaddrinfo_t* handle, uv_req_t* req) {
  int addrinfo_len = 0;
  int name_len = 0;
  size_t addrinfo_struct_len = ALIGNED_SIZE(sizeof(struct addrinfo));
  struct addrinfoW* addrinfow_ptr;
  struct addrinfo* addrinfo_ptr;
  char* alloc_ptr = NULL;
  char* cur_ptr = NULL;
  uv_err_code uv_ret;

  /* release input parameter memory */
  if (handle->alloc != NULL) {
    free(handle->alloc);
    handle->alloc = NULL;
  }

  uv_ret = uv_translate_eai_error(handle->retcode);
  if (handle->retcode == 0) {
    /* convert addrinfoW to addrinfo */
    /* first calculate required length */
    addrinfow_ptr = handle->res;
    while (addrinfow_ptr != NULL) {
      addrinfo_len += addrinfo_struct_len + ALIGNED_SIZE(addrinfow_ptr->ai_addrlen);
      if (addrinfow_ptr->ai_canonname != NULL) {
        name_len = uv_utf16_to_utf8(addrinfow_ptr->ai_canonname, -1, NULL, 0);
        if (name_len == 0) {
          uv_ret = uv_translate_sys_error(GetLastError());
          goto complete;
        }
        addrinfo_len += ALIGNED_SIZE(name_len);
      }
      addrinfow_ptr = addrinfow_ptr->ai_next;
    }

    /* allocate memory for addrinfo results */
    alloc_ptr = (char*)malloc(addrinfo_len);

    /* do conversions */
    if (alloc_ptr != NULL) {
      cur_ptr = alloc_ptr;
      addrinfow_ptr = handle->res;

      while (addrinfow_ptr != NULL) {
        /* copy addrinfo struct data */
        assert(cur_ptr + addrinfo_struct_len <= alloc_ptr + addrinfo_len);
        addrinfo_ptr = (struct addrinfo*)cur_ptr;
        addrinfo_ptr->ai_family = addrinfow_ptr->ai_family;
        addrinfo_ptr->ai_socktype = addrinfow_ptr->ai_socktype;
        addrinfo_ptr->ai_protocol = addrinfow_ptr->ai_protocol;
        addrinfo_ptr->ai_flags = addrinfow_ptr->ai_flags;
        addrinfo_ptr->ai_addrlen = addrinfow_ptr->ai_addrlen;
        addrinfo_ptr->ai_canonname = NULL;
        addrinfo_ptr->ai_addr = NULL;
        addrinfo_ptr->ai_next = NULL;

        cur_ptr += addrinfo_struct_len;

        /* copy sockaddr */
        if (addrinfo_ptr->ai_addrlen > 0) {
          assert(cur_ptr + addrinfo_ptr->ai_addrlen <= alloc_ptr + addrinfo_len);
          memcpy(cur_ptr, addrinfow_ptr->ai_addr, addrinfo_ptr->ai_addrlen);
          addrinfo_ptr->ai_addr = (struct sockaddr*)cur_ptr;
          cur_ptr += ALIGNED_SIZE(addrinfo_ptr->ai_addrlen);
        }

        /* convert canonical name to UTF-8 */
        if (addrinfow_ptr->ai_canonname != NULL) {
          name_len = uv_utf16_to_utf8(addrinfow_ptr->ai_canonname, -1, NULL, 0);
          assert(name_len > 0);
          assert(cur_ptr + name_len <= alloc_ptr + addrinfo_len);
          name_len = uv_utf16_to_utf8(addrinfow_ptr->ai_canonname, -1, cur_ptr, name_len);
          assert(name_len > 0);
          addrinfo_ptr->ai_canonname = cur_ptr;
          cur_ptr += ALIGNED_SIZE(name_len);
        }
        assert(cur_ptr <= alloc_ptr + addrinfo_len);

        /* set next ptr */
        addrinfow_ptr = addrinfow_ptr->ai_next;
        if (addrinfow_ptr != NULL) {
          addrinfo_ptr->ai_next = (struct addrinfo*)cur_ptr;
        }
      }
    } else {
      uv_ret = UV_ENOMEM;
    }

  }

  /* return memory to system */
  if (handle->res != NULL) {
    FreeAddrInfoW(handle->res);
    handle->res = NULL;
  }

complete:
  /* finally do callback with converted result */
  handle->getaddrinfo_cb(handle, uv_ret, (struct addrinfo*)alloc_ptr);

  /* release copied result memory */
  if (alloc_ptr != NULL) {
    free(alloc_ptr);
  }

  uv_refs_--;
}


/*
 * Entry point for getaddrinfo
 * we convert the UTF-8 strings to UNICODE
 * and save the UNICODE string pointers in the handle
 * We also copy hints so that caller does not need to keep memory until the callback.
 * return UV_OK if a callback will be made
 * return error code if validation fails
 *
 * To minimize allocation we calculate total size required,
 * and copy all structs and referenced strings into the one block.
 * Each size calculation is adjusted to avoid unaligned pointers.
 */
int uv_getaddrinfo(uv_getaddrinfo_t* handle,
                   uv_getaddrinfo_cb getaddrinfo_cb,
                   const char* node,
                   const char* service,
                   const struct addrinfo* hints) {
  int nodesize = 0;
  int servicesize = 0;
  int hintssize = 0;
  char* alloc_ptr = NULL;

  if (handle == NULL || getaddrinfo_cb == NULL ||
     (node == NULL && service == NULL)) {
    uv_set_sys_error(WSAEINVAL);
    goto error;
  }

  handle->getaddrinfo_cb = getaddrinfo_cb;
  handle->res = NULL;
  handle->type = UV_GETADDRINFO;

  /* calculate required memory size for all input values */
  if (node != NULL) {
    nodesize = ALIGNED_SIZE(uv_utf8_to_utf16(node, NULL, 0) * sizeof(wchar_t));
    if (nodesize == 0) {
      uv_set_sys_error(GetLastError());
      goto error;
    }
  }

  if (service != NULL) {
    servicesize = ALIGNED_SIZE(uv_utf8_to_utf16(service, NULL, 0) * sizeof(wchar_t));
    if (servicesize == 0) {
      uv_set_sys_error(GetLastError());
      goto error;
    }
  }
  if (hints != NULL) {
    hintssize = ALIGNED_SIZE(sizeof(struct addrinfoW));
  }

  /* allocate memory for inputs, and partition it as needed */
  alloc_ptr = (char*)malloc(nodesize + servicesize + hintssize);
  if (!alloc_ptr) {
    uv_set_sys_error(WSAENOBUFS);
    goto error;
  }

  /* save alloc_ptr now so we can free if error */
  handle->alloc = (void*)alloc_ptr;

  /* convert node string to UTF16 into allocated memory and save pointer in handle */
  if (node != NULL) {
    handle->node = (wchar_t*)alloc_ptr;
    if (uv_utf8_to_utf16(node, (wchar_t*)alloc_ptr, nodesize / sizeof(wchar_t)) == 0) {
      uv_set_sys_error(GetLastError());
      goto error;
    }
    alloc_ptr += nodesize;
  } else {
    handle->node = NULL;
  }

  /* convert service string to UTF16 into allocated memory and save pointer in handle */
  if (service != NULL) {
    handle->service = (wchar_t*)alloc_ptr;
    if (uv_utf8_to_utf16(service, (wchar_t*)alloc_ptr, servicesize / sizeof(wchar_t)) == 0) {
      uv_set_sys_error(GetLastError());
      goto error;
    }
    alloc_ptr += servicesize;
  } else {
    handle->service = NULL;
  }

  /* copy hints to allocated memory and save pointer in handle */
  if (hints != NULL) {
    handle->hints = (struct addrinfoW*)alloc_ptr;
    handle->hints->ai_family = hints->ai_family;
    handle->hints->ai_socktype = hints->ai_socktype;
    handle->hints->ai_protocol = hints->ai_protocol;
    handle->hints->ai_flags = hints->ai_flags;
    handle->hints->ai_addrlen = 0;
    handle->hints->ai_canonname = NULL;
    handle->hints->ai_addr = NULL;
    handle->hints->ai_next = NULL;
  } else {
    handle->hints = NULL;
  }

  /* init request for Post handling */
  uv_req_init(&handle->getadddrinfo_req, (uv_handle_t*)handle, NULL);
  handle->getadddrinfo_req.type = UV_GETADDRINFO_REQ;

  /* Ask thread to run. Treat this as a long operation */
  if (QueueUserWorkItem(&getaddrinfo_thread_proc, handle, WT_EXECUTELONGFUNCTION) == 0) {
    uv_set_sys_error(GetLastError());
    goto error;
  }

  uv_refs_++;

  return 0;

error:
  if (handle != NULL && handle->alloc != NULL) {
    free(handle->alloc);
  }
  return -1;
}


int uv_pipe_init(uv_pipe_t* handle) {
  uv_stream_init((uv_stream_t*)handle);

  handle->type = UV_NAMED_PIPE;
  handle->reqs_pending = 0;

  uv_counters()->pipe_init++;

  return 0;
}


/* Creates a pipe server. */
/* TODO: make this work with UTF8 name */
int uv_pipe_bind(uv_pipe_t* handle, const char* name) {
  int i;

  if (!name) {
    return -1;
  }

  handle->connections = NULL;

  /* Initialize accept requests. */
  for (i = 0; i < COUNTOF(handle->accept_reqs); i++) {
    handle->accept_reqs[i].flags = 0;
    handle->accept_reqs[i].type = UV_ACCEPT;
    handle->accept_reqs[i].handle = (uv_handle_t*)handle;
    handle->accept_reqs[i].cb = NULL;
    handle->accept_reqs[i].data = NULL;
    uv_counters()->req_init++;
  }

  /* Make our own copy of the pipe name */
  handle->name = (char*)malloc(MAX_PIPENAME_LEN);
  if (!handle->name) {
    uv_fatal_error(ERROR_OUTOFMEMORY, "malloc");
  }
  strcpy(handle->name, name);
  handle->name[255] = '\0';

  handle->flags |= UV_HANDLE_PIPESERVER;
  return 0;
}


/* Starts listening for connections for the given pipe. */
int uv_pipe_listen(uv_pipe_t* handle, uv_connection_cb cb) {
  int i, maxInstances, errno;
  HANDLE pipeHandle;
  uv_pipe_instance_t* pipeInstance;

  if (handle->flags & UV_HANDLE_LISTENING ||
      handle->flags & UV_HANDLE_READING) {
    uv_set_sys_error(UV_EALREADY);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_PIPESERVER)) {
    uv_set_sys_error(UV_ENOTSUP);
    return -1;
  }

  handle->flags |= UV_HANDLE_LISTENING;
  handle->connection_cb = cb;

  uv_pipe_queue_accept(handle);
  return 0;
}

/* TODO: make this work with UTF8 name */
int uv_pipe_connect(uv_req_t* req, const char* name) {
  int errno;
  DWORD mode;
  uv_pipe_t* handle = (uv_pipe_t*)req->handle;

  assert(!(req->flags & UV_REQ_PENDING));

  req->type = UV_CONNECT;
  handle->connection = &handle->clientConnection;
  handle->server = NULL;
  memset(&req->overlapped, 0, sizeof(req->overlapped));

  handle->clientConnection.handle = CreateFile(name,
                                               GENERIC_READ | GENERIC_WRITE,
                                               0,
                                               NULL,
                                               OPEN_EXISTING,
                                               FILE_FLAG_OVERLAPPED,
                                               NULL);

  if (handle->clientConnection.handle == INVALID_HANDLE_VALUE &&
      GetLastError() != ERROR_IO_PENDING) {
    errno = GetLastError();
    goto error;
  }

  mode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;

  if (!SetNamedPipeHandleState(handle->clientConnection.handle, &mode, NULL, NULL)) {
    errno = GetLastError();
    goto error;
  }

  if (CreateIoCompletionPort(handle->clientConnection.handle,
                             uv_iocp_,
                             (ULONG_PTR)handle,
                             0) == NULL) {
    errno = GetLastError();
    goto error;
  }

  req->error = uv_ok_;
  req->flags |= UV_REQ_PENDING;
  handle->connection->state = UV_PIPEINSTANCE_ACTIVE;
  uv_insert_pending_req(req);
  handle->reqs_pending++;
  return 0;

error:
  close_pipe(handle, NULL, NULL);
  req->error = uv_new_sys_error(errno);
  uv_insert_pending_req(req);
  handle->reqs_pending++;
  return 0;
}


/* Cleans up uv_pipe_t (server or connection) and all resources associated with it */
static void close_pipe(uv_pipe_t* handle, int* status, uv_err_t* err) {
  uv_pipe_instance_t* connection, *next, *cur, **prev;
  HANDLE pipeHandle;
  int i;

  if (handle->flags & UV_HANDLE_PIPESERVER) {
    if (handle->flags & UV_HANDLE_CONNECTION) {
      /*
       * The handle is for a connection instance on the pipe server.
       * To clean-up, we call DisconnectNamedPipe, and then uv_pipe_queue_accept will cleanup the allocated uv_pipe_instance_t.
       */

      connection = handle->connection;
      if (connection && connection->handle != INVALID_HANDLE_VALUE) {
        /* Disconnect the connection intance and return it to pending state. */
        if (DisconnectNamedPipe(connection->handle)) {
          if (status) *status = 0;
        } else {
          if (status) *status = -1;
          if (err) *err = uv_new_sys_error(GetLastError());
        }

        connection->state = UV_PIPEINSTANCE_DISCONNECTED;
        connection->handle = NULL;

        cur = handle->connections;
        handle->connection = NULL;
        prev = &handle->server->connections;        

        /* Remove the connection from the list. */
        while (connection) {
          if (cur == connection) {
            *prev = connection->next;
            free(connection);
            break;
          } else {
            prev = &connection->next;
            connection = connection->next;
          }
        }

        /* Queue accept now that the instance is in pending state. */
        if (!(handle->server->flags & UV_HANDLE_CLOSING)) {
          uv_pipe_queue_accept(handle->server);
        }
      }
    } else {
      /*
       * The handle is for the pipe server.
       * To clean-up we close every active and pending connection instance.
       */

      if (handle->name) {
        free(handle->name);
        handle->name = NULL;
      }

      connection = handle->connections;
      while (connection) {
        pipeHandle = connection->handle;

        if (pipeHandle) {
          DisconnectNamedPipe(pipeHandle);
          CloseHandle(pipeHandle);
        }

        next = connection->next;
        free(connection);
        connection = next;
      }

      handle->connections = NULL;

      for (i = 0; i < COUNTOF(handle->accept_reqs); i++) {
        if (handle->accept_reqs[i].flags & UV_REQ_PENDING) {
          pipeHandle = handle->accept_reqs[i].data;
          assert(pipeHandle);
          DisconnectNamedPipe(pipeHandle);
          CloseHandle(pipeHandle);
          handle->accept_reqs[i].flags = 0;
          handle->reqs_pending--;
        }
      }

      if (status) *status = 0;
    }
  } else {
    /*
     * The handle is for a connection instance on the pipe client.
     * To clean-up we close the pipe handle.
     */
    connection = handle->connection;
    if (connection && connection->handle != INVALID_HANDLE_VALUE) {
      if (CloseHandle(connection->handle)) {
        connection->state = UV_PIPEINSTANCE_DISCONNECTED;
        handle->connection = NULL;
        if (status) *status = 0;
      } else {
        if (status) *status = -1;
        if (err) *err = uv_new_sys_error(GetLastError());
      }
    }
  }

  handle->flags |= UV_HANDLE_SHUT;
}
