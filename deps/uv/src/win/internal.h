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

#ifndef UV_WIN_INTERNAL_H_
#define UV_WIN_INTERNAL_H_

#include "uv.h"
#include "../uv-common.h"

#include "tree.h"
#include "ntdll.h"


/*
 * Timers
 */
RB_HEAD(uv_timer_tree_s, uv_timer_s);

void uv_timer_endgame(uv_timer_t* handle);

DWORD uv_get_poll_timeout();
void uv_process_timers();


/*
 * Core
 */

/* Loop state struct. We don't support multiplicity right now, but this */
/* should help when we get to that. */
typedef struct uv_loop_s {
  /* The loop's I/O completion port */
  HANDLE iocp;
  /* Reference count that keeps the event loop alive */
  int refs;
  /* The current time according to the event loop. in msecs. */
  int64_t time;
  /* Tail of a single-linked circular queue of pending reqs. If the queue */
  /* is empty, tail_ is NULL. If there is only one item, */
  /* tail_->next_req == tail_ */
  uv_req_t* pending_reqs_tail;
  /* Head of a single-linked list of closed handles */
  uv_handle_t* endgame_handles;
  /* The head of the timers tree */
  struct uv_timer_tree_s timers;
    /* Lists of active loop (prepare / check / idle) watchers */
  uv_prepare_t* prepare_handles;
  uv_check_t* check_handles;
  uv_idle_t* idle_handles;
  /* This pointer will refer to the prepare/check/idle handle whose */
  /* callback is scheduled to be called next. This is needed to allow */
  /* safe removal from one of the lists above while that list being */
  /* iterated over. */
  uv_prepare_t* next_prepare_handle;
  uv_check_t* next_check_handle;
  uv_idle_t* next_idle_handle;
  /* Last error code */
  uv_err_t last_error;
  /* Error string most recently returned by uv_strerror() */
  char* err_str;
} uv_loop_t;

extern uv_loop_t uv_main_loop_;

#define LOOP (&uv_main_loop_)


/*
 * Handles
 */

/* Private uv_handle flags */
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
#define UV_HANDLE_READ_PENDING     0x8000
#define UV_HANDLE_GIVEN_OS_HANDLE  0x10000
#define UV_HANDLE_UV_ALLOCED       0x20000

void uv_want_endgame(uv_handle_t* handle);
void uv_process_endgames();

#define DECREASE_PENDING_REQ_COUNT(handle)    \
  do {                                        \
    assert(handle->reqs_pending > 0);         \
    handle->reqs_pending--;                   \
                                              \
    if (handle->flags & UV_HANDLE_CLOSING &&  \
        handle->reqs_pending == 0) {          \
      uv_want_endgame((uv_handle_t*)handle);  \
    }                                         \
  } while (0)


/*
 * Requests
 */
void uv_req_init(uv_req_t* req);

uv_req_t* uv_overlapped_to_req(OVERLAPPED* overlapped);

void uv_insert_pending_req(uv_req_t* req);
void uv_process_reqs();


/*
 * Streams
 */
void uv_stream_init(uv_stream_t* handle);
void uv_connection_init(uv_stream_t* handle);

size_t uv_count_bufs(uv_buf_t bufs[], int count);


/*
 * TCP
 */
void uv_winsock_startup();

void uv_tcp_endgame(uv_tcp_t* handle);

int uv_tcp_listen(uv_tcp_t* handle, int backlog, uv_connection_cb cb);
int uv_tcp_accept(uv_tcp_t* server, uv_tcp_t* client);
int uv_tcp_read_start(uv_tcp_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb);
int uv_tcp_write(uv_write_t* req, uv_tcp_t* handle, uv_buf_t bufs[],
    int bufcnt, uv_write_cb cb);

void uv_process_tcp_read_req(uv_tcp_t* handle, uv_req_t* req);
void uv_process_tcp_write_req(uv_tcp_t* handle, uv_write_t* req);
void uv_process_tcp_accept_req(uv_tcp_t* handle, uv_req_t* req);
void uv_process_tcp_connect_req(uv_tcp_t* handle, uv_connect_t* req);


/*
 * Pipes
 */
int uv_pipe_init_with_handle(uv_pipe_t* handle, HANDLE pipeHandle);
int uv_stdio_pipe_server(uv_pipe_t* handle, DWORD access, char* name, size_t nameSize);
void close_pipe(uv_pipe_t* handle, int* status, uv_err_t* err);
void uv_pipe_endgame(uv_pipe_t* handle);

int uv_pipe_listen(uv_pipe_t* handle, int backlog, uv_connection_cb cb);
int uv_pipe_accept(uv_pipe_t* server, uv_pipe_t* client);
int uv_pipe_read_start(uv_pipe_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb);
int uv_pipe_write(uv_write_t* req, uv_pipe_t* handle, uv_buf_t bufs[],
    int bufcnt, uv_write_cb cb);

void uv_process_pipe_read_req(uv_pipe_t* handle, uv_req_t* req);
void uv_process_pipe_write_req(uv_pipe_t* handle, uv_write_t* req);
void uv_process_pipe_accept_req(uv_pipe_t* handle, uv_req_t* raw_req);
void uv_process_pipe_connect_req(uv_pipe_t* handle, uv_connect_t* req);

/*
 * Loop watchers
 */
void uv_loop_watcher_endgame(uv_handle_t* handle);

void uv_prepare_invoke();
void uv_check_invoke();
void uv_idle_invoke();


/*
 * Async watcher
 */
void uv_async_endgame(uv_async_t* handle);

void uv_process_async_wakeup_req(uv_async_t* handle, uv_req_t* req);


/*
 * Spawn
 */
void uv_process_proc_exit(uv_process_t* handle);
void uv_process_proc_close(uv_process_t* handle);
void uv_process_close(uv_process_t* handle);
void uv_process_endgame(uv_process_t* handle);


/*
 * C-ares integration
 */
typedef struct uv_ares_action_s uv_ares_action_t;

void uv_process_ares_event_req(uv_ares_action_t* handle, uv_req_t* req);
void uv_process_ares_cleanup_req(uv_ares_task_t* handle, uv_req_t* req);

/*
 * Getaddrinfo
 */
void uv_process_getaddrinfo_req(uv_getaddrinfo_t* handle, uv_req_t* req);


/*
 * Error handling
 */
extern const uv_err_t uv_ok_;

void uv_fatal_error(const int errorno, const char* syscall);

uv_err_code uv_translate_sys_error(int sys_errno);
uv_err_t uv_new_sys_error(int sys_errno);
void uv_set_sys_error(int sys_errno);
void uv_set_error(uv_err_code code, int sys_errno);


/*
 * Windows api functions that we need to retrieve dynamically
 */
void uv_winapi_init();

extern sRtlNtStatusToDosError pRtlNtStatusToDosError;
extern sNtQueryInformationFile pNtQueryInformationFile;


#endif /* UV_WIN_INTERNAL_H_ */
