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
# define _WIN32_WINNT   0x0501
#endif

#include <stdint.h>
#include <winsock2.h>
#include <mswsock.h>
#include <ws2tcpip.h>
#include <windows.h>

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

/*
 * Private uv_pipe_instance state.
 */
typedef enum {
  UV_PIPEINSTANCE_CONNECTED = 0,
  UV_PIPEINSTANCE_DISCONNECTED,
  UV_PIPEINSTANCE_ACTIVE
} uv_pipeinstance_state;

/* Used to store active pipe instances inside a linked list. */
typedef struct uv_pipe_instance_s {
  HANDLE handle;
  uv_pipeinstance_state state;
  struct uv_pipe_instance_s* next;
} uv_pipe_instance_t;

#define UV_REQ_PRIVATE_FIELDS             \
  union {                                 \
    /* Used by I/O operations */          \
    struct {                              \
      OVERLAPPED overlapped;              \
      size_t queued_bytes;                \
    };                                    \
  };                                      \
  int flags;                              \
  uv_err_t error;                         \
  struct uv_req_s* next_req;

#define uv_stream_connection_fields       \
  unsigned int write_reqs_pending;        \
  uv_req_t* shutdown_req;

#define uv_stream_server_fields           \
  uv_connection_cb connection_cb;

#define UV_STREAM_PRIVATE_FIELDS          \
  unsigned int reqs_pending;              \
  uv_alloc_cb alloc_cb;                   \
  uv_read_cb read_cb;                     \
  struct uv_req_s read_req;               \
  union {                                 \
    struct { uv_stream_connection_fields };  \
    struct { uv_stream_server_fields     };  \
  };

#define UV_TCP_PRIVATE_FIELDS             \
  union {                                 \
    SOCKET socket;                        \
    HANDLE handle;                        \
  };                                      \
  SOCKET accept_socket;                   \
  char accept_buffer[sizeof(struct sockaddr_storage) * 2 + 32]; \
  struct uv_req_s accept_req;

#define uv_pipe_server_fields             \
    char* name;                           \
    uv_pipe_instance_t* connections;      \
    struct uv_req_s accept_reqs[4];

#define uv_pipe_connection_fields         \
    uv_pipe_t* server;                    \
    uv_pipe_instance_t* connection;       \
    uv_pipe_instance_t clientConnection;

#define UV_PIPE_PRIVATE_FIELDS            \
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
  unsigned int flags;                     \
  uv_err_t error;

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

int uv_utf16_to_utf8(wchar_t* utf16Buffer, size_t utf16Size, char* utf8Buffer, size_t utf8Size);
int uv_utf8_to_utf16(const char* utf8Buffer, wchar_t* utf16Buffer, size_t utf16Size);
