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

#ifndef _WIN32_WINNT
# define _WIN32_WINNT   0x0502
#endif

#include <stdint.h>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <sys/stat.h>

#include "tree.h"

#define MAX_PIPENAME_LEN 256

/**
 * It should be possible to cast uv_buf_t[] to WSABUF[]
 * see http://msdn.microsoft.com/en-us/library/ms741542(v=vs.85).aspx
 */
typedef struct uv_buf_t {
  ULONG len;
  char* base;
} uv_buf_t;

typedef int uv_file;

RB_HEAD(uv_timer_tree_s, uv_timer_s);

#define UV_LOOP_PRIVATE_FIELDS                                                \
    /* The loop's I/O completion port */                                      \
  HANDLE iocp;                                                                \
  /* Reference count that keeps the event loop alive */                       \
  int refs;                                                                   \
  /* The current time according to the event loop. in msecs. */               \
  int64_t time;                                                               \
  /* Tail of a single-linked circular queue of pending reqs. If the queue */  \
  /* is empty, tail_ is NULL. If there is only one item, */                   \
  /* tail_->next_req == tail_ */                                              \
  uv_req_t* pending_reqs_tail;                                                \
  /* Head of a single-linked list of closed handles */                        \
  uv_handle_t* endgame_handles;                                               \
  /* The head of the timers tree */                                           \
  struct uv_timer_tree_s timers;                                              \
    /* Lists of active loop (prepare / check / idle) watchers */              \
  uv_prepare_t* prepare_handles;                                              \
  uv_check_t* check_handles;                                                  \
  uv_idle_t* idle_handles;                                                    \
  /* This pointer will refer to the prepare/check/idle handle whose */        \
  /* callback is scheduled to be called next. This is needed to allow */      \
  /* safe removal from one of the lists above while that list being */        \
  /* iterated over. */                                                        \
  uv_prepare_t* next_prepare_handle;                                          \
  uv_check_t* next_check_handle;                                              \
  uv_idle_t* next_idle_handle;                                                \
  ares_channel ares_chan;                                                     \
  int ares_active_sockets;                                                    \
  uv_timer_t ares_polling_timer;                                              \
  /* Last error code */                                                       \
  uv_err_t last_error;

#define UV_REQ_TYPE_PRIVATE               \
  /* TODO: remove the req suffix */       \
  UV_ARES_EVENT_REQ,                      \
  UV_ARES_CLEANUP_REQ,                    \
  UV_GETADDRINFO_REQ,                     \
  UV_PROCESS_EXIT,                        \
  UV_PROCESS_CLOSE,                       \
  UV_UDP_RECV,                            \
  UV_FS_EVENT_REQ

#define UV_REQ_PRIVATE_FIELDS             \
  union {                                 \
    /* Used by I/O operations */          \
    struct {                              \
      OVERLAPPED overlapped;              \
      size_t queued_bytes;                \
    };                                    \
  };                                      \
  struct uv_req_s* next_req;

#define UV_WRITE_PRIVATE_FIELDS           \
  /* empty */

#define UV_CONNECT_PRIVATE_FIELDS         \
  /* empty */

#define UV_SHUTDOWN_PRIVATE_FIELDS        \
  /* empty */

#define UV_UDP_SEND_PRIVATE_FIELDS        \
  /* empty */

#define UV_PRIVATE_REQ_TYPES              \
  typedef struct uv_pipe_accept_s {       \
    UV_REQ_FIELDS                         \
    HANDLE pipeHandle;                    \
    struct uv_pipe_accept_s* next_pending; \
  } uv_pipe_accept_t;                     \
  typedef struct uv_tcp_accept_s {        \
    UV_REQ_FIELDS                         \
    SOCKET accept_socket;                 \
    char accept_buffer[sizeof(struct sockaddr_storage) * 2 + 32]; \
    struct uv_tcp_accept_s* next_pending; \
  } uv_tcp_accept_t;

#define uv_stream_connection_fields       \
  unsigned int write_reqs_pending;        \
  uv_shutdown_t* shutdown_req;

#define uv_stream_server_fields           \
  uv_connection_cb connection_cb;

#define UV_STREAM_PRIVATE_FIELDS          \
  unsigned int reqs_pending;              \
  uv_alloc_cb alloc_cb;                   \
  uv_read_cb read_cb;                     \
  uv_req_t read_req;                      \
  union {                                 \
    struct { uv_stream_connection_fields };  \
    struct { uv_stream_server_fields     };  \
  };

#define uv_tcp_server_fields              \
  uv_tcp_accept_t* accept_reqs;           \
  uv_tcp_accept_t* pending_accepts;

#define uv_tcp_connection_fields          \
  uv_buf_t read_buffer;

#define UV_TCP_PRIVATE_FIELDS             \
  SOCKET socket;                          \
  uv_err_t bind_error;                    \
  union {                                 \
    struct { uv_tcp_server_fields };      \
    struct { uv_tcp_connection_fields };  \
  };

#define UV_UDP_PRIVATE_FIELDS             \
  SOCKET socket;                          \
  unsigned int reqs_pending;              \
  uv_req_t recv_req;                      \
  uv_buf_t recv_buffer;                   \
  struct sockaddr_storage recv_from;      \
  int recv_from_len;                      \
  uv_udp_recv_cb recv_cb;                 \
  uv_alloc_cb alloc_cb;

#define uv_pipe_server_fields             \
    uv_pipe_accept_t accept_reqs[4];      \
    uv_pipe_accept_t* pending_accepts;

#define uv_pipe_connection_fields         \
  uv_timer_t* eof_timer;

#define UV_PIPE_PRIVATE_FIELDS            \
  HANDLE handle;                          \
  wchar_t* name;                          \
  union {                                 \
    struct { uv_pipe_server_fields };     \
    struct { uv_pipe_connection_fields }; \
  };

#define UV_TIMER_PRIVATE_FIELDS           \
  RB_ENTRY(uv_timer_s) tree_entry;        \
  int64_t due;                            \
  int64_t repeat;                         \
  uv_timer_cb timer_cb;

#define UV_ASYNC_PRIVATE_FIELDS           \
  struct uv_req_s async_req;              \
  uv_async_cb async_cb;                   \
  /* char to avoid alignment issues */    \
  char volatile async_sent;

#define UV_PREPARE_PRIVATE_FIELDS         \
  uv_prepare_t* prepare_prev;             \
  uv_prepare_t* prepare_next;             \
  uv_prepare_cb prepare_cb;

#define UV_CHECK_PRIVATE_FIELDS           \
  uv_check_t* check_prev;                 \
  uv_check_t* check_next;                 \
  uv_check_cb check_cb;

#define UV_IDLE_PRIVATE_FIELDS            \
  uv_idle_t* idle_prev;                   \
  uv_idle_t* idle_next;                   \
  uv_idle_cb idle_cb;

#define UV_HANDLE_PRIVATE_FIELDS          \
  uv_handle_t* endgame_next;              \
  unsigned int flags;

#define UV_ARES_TASK_PRIVATE_FIELDS       \
  struct uv_req_s ares_req;               \
  SOCKET sock;                            \
  HANDLE h_wait;                          \
  WSAEVENT h_event;                       \
  HANDLE h_close_event;

#define UV_GETADDRINFO_PRIVATE_FIELDS     \
  struct uv_req_s getadddrinfo_req;       \
  uv_getaddrinfo_cb getaddrinfo_cb;       \
  void* alloc;                            \
  wchar_t* node;                          \
  wchar_t* service;                       \
  struct addrinfoW* hints;                \
  struct addrinfoW* res;                  \
  int retcode;

#define UV_PROCESS_PRIVATE_FIELDS         \
  struct uv_process_exit_s {              \
    UV_REQ_FIELDS                         \
  } exit_req;                             \
  struct uv_process_close_s {             \
    UV_REQ_FIELDS                         \
  } close_req;                            \
  HANDLE child_stdio[3];                  \
  int exit_signal;                        \
  DWORD spawn_errno;                      \
  HANDLE wait_handle;                     \
  HANDLE process_handle;                  \
  HANDLE close_handle;

#define UV_FS_PRIVATE_FIELDS              \
  int flags;                              \
  int last_error;                         \
  struct _stati64 stat;                   \
  void* arg0;                             \
  union {                                 \
    struct {                              \
      void* arg1;                         \
      void* arg2;                         \
      void* arg3;                         \
    };                                    \
    struct {                              \
      ssize_t arg4;                       \
      ssize_t arg5;                       \
    };                                    \
  };

#define UV_WORK_PRIVATE_FIELDS            \

#define UV_FS_EVENT_PRIVATE_FIELDS        \
  struct uv_fs_event_req_s {              \
    UV_REQ_FIELDS                         \
  } req;                                  \
  HANDLE dir_handle;                      \
  int req_pending;                        \
  uv_fs_event_cb cb;                      \
  wchar_t* filew;                         \
  int is_path_dir;                        \
  char* buffer;

#define UV_TTY_PRIVATE_FIELDS /* empty */

int uv_utf16_to_utf8(const wchar_t* utf16Buffer, size_t utf16Size,
    char* utf8Buffer, size_t utf8Size);
int uv_utf8_to_utf16(const char* utf8Buffer, wchar_t* utf16Buffer,
    size_t utf16Size);
