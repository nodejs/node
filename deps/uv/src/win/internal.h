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
#include "winapi.h"
#include "winsock.h"


/*
 * Timers
 */
void uv_timer_endgame(uv_loop_t* loop, uv_timer_t* handle);

DWORD uv_get_poll_timeout(uv_loop_t* loop);
void uv_process_timers(uv_loop_t* loop);


/*
 * Handles
 */

/* Private uv_handle flags */
#define UV_HANDLE_CLOSING                       0x00000001
#define UV_HANDLE_CLOSED                        0x00000002
#define UV_HANDLE_BOUND                         0x00000004
#define UV_HANDLE_LISTENING                     0x00000008
#define UV_HANDLE_CONNECTION                    0x00000010
#define UV_HANDLE_CONNECTED                     0x00000020
#define UV_HANDLE_READING                       0x00000040
#define UV_HANDLE_ACTIVE                        0x00000040
#define UV_HANDLE_EOF                           0x00000080
#define UV_HANDLE_SHUTTING                      0x00000100
#define UV_HANDLE_SHUT                          0x00000200
#define UV_HANDLE_ENDGAME_QUEUED                0x00000400
#define UV_HANDLE_BIND_ERROR                    0x00001000
#define UV_HANDLE_IPV6                          0x00002000
#define UV_HANDLE_PIPESERVER                    0x00004000
#define UV_HANDLE_READ_PENDING                  0x00008000
#define UV_HANDLE_UV_ALLOCED                    0x00010000
#define UV_HANDLE_SYNC_BYPASS_IOCP              0x00020000
#define UV_HANDLE_ZERO_READ                     0x00040000
#define UV_HANDLE_TTY_RAW                       0x00080000
#define UV_HANDLE_EMULATE_IOCP                  0x00100000
#define UV_HANDLE_NON_OVERLAPPED_PIPE           0x00200000
#define UV_HANDLE_TTY_SAVED_POSITION            0x00400000
#define UV_HANDLE_TTY_SAVED_ATTRIBUTES          0x00800000
#define UV_HANDLE_SHARED_TCP_SERVER             0x01000000
#define UV_HANDLE_TCP_NODELAY                   0x02000000
#define UV_HANDLE_TCP_KEEPALIVE                 0x04000000
#define UV_HANDLE_TCP_SINGLE_ACCEPT             0x08000000
#define UV_HANDLE_TCP_ACCEPT_STATE_CHANGING     0x10000000

void uv_want_endgame(uv_loop_t* loop, uv_handle_t* handle);
void uv_process_endgames(uv_loop_t* loop);

#define DECREASE_PENDING_REQ_COUNT(handle)    \
  do {                                        \
    assert(handle->reqs_pending > 0);         \
    handle->reqs_pending--;                   \
                                              \
    if (handle->flags & UV_HANDLE_CLOSING &&  \
        handle->reqs_pending == 0) {          \
      uv_want_endgame(loop, (uv_handle_t*)handle);  \
    }                                         \
  } while (0)

#define UV_SUCCEEDED_WITHOUT_IOCP(result)                     \
  ((result) && (handle->flags & UV_HANDLE_SYNC_BYPASS_IOCP))

#define UV_SUCCEEDED_WITH_IOCP(result)                        \
  ((result) || (GetLastError() == ERROR_IO_PENDING))


/*
 * Requests
 */
void uv_req_init(uv_loop_t* loop, uv_req_t* req);

uv_req_t* uv_overlapped_to_req(OVERLAPPED* overlapped);

void uv_insert_pending_req(uv_loop_t* loop, uv_req_t* req);
void uv_process_reqs(uv_loop_t* loop);

#define POST_COMPLETION_FOR_REQ(loop, req)                              \
  if (!PostQueuedCompletionStatus((loop)->iocp,                         \
                                  0,                                    \
                                  0,                                    \
                                  &((req)->overlapped))) {              \
    uv_fatal_error(GetLastError(), "PostQueuedCompletionStatus");       \
  }


/*
 * Streams
 */
void uv_stream_init(uv_loop_t* loop, uv_stream_t* handle);
void uv_connection_init(uv_stream_t* handle);

size_t uv_count_bufs(uv_buf_t bufs[], int count);


/*
 * TCP
 */
int uv_tcp_listen(uv_tcp_t* handle, int backlog, uv_connection_cb cb);
int uv_tcp_accept(uv_tcp_t* server, uv_tcp_t* client);
int uv_tcp_read_start(uv_tcp_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb);
int uv_tcp_write(uv_loop_t* loop, uv_write_t* req, uv_tcp_t* handle,
    uv_buf_t bufs[], int bufcnt, uv_write_cb cb);

void uv_process_tcp_read_req(uv_loop_t* loop, uv_tcp_t* handle, uv_req_t* req);
void uv_process_tcp_write_req(uv_loop_t* loop, uv_tcp_t* handle,
    uv_write_t* req);
void uv_process_tcp_accept_req(uv_loop_t* loop, uv_tcp_t* handle,
    uv_req_t* req);
void uv_process_tcp_connect_req(uv_loop_t* loop, uv_tcp_t* handle,
    uv_connect_t* req);

void uv_tcp_endgame(uv_loop_t* loop, uv_tcp_t* handle);

int uv_tcp_import(uv_tcp_t* tcp, WSAPROTOCOL_INFOW* socket_protocol_info);

int uv_tcp_duplicate_socket(uv_tcp_t* handle, int pid,
    LPWSAPROTOCOL_INFOW protocol_info);


/*
 * UDP
 */
void uv_process_udp_recv_req(uv_loop_t* loop, uv_udp_t* handle, uv_req_t* req);
void uv_process_udp_send_req(uv_loop_t* loop, uv_udp_t* handle,
    uv_udp_send_t* req);

void uv_udp_endgame(uv_loop_t* loop, uv_udp_t* handle);


/*
 * Pipes
 */
int uv_stdio_pipe_server(uv_loop_t* loop, uv_pipe_t* handle, DWORD access,
    char* name, size_t nameSize);
void close_pipe(uv_pipe_t* handle, int* status, uv_err_t* err);
void uv_pipe_endgame(uv_loop_t* loop, uv_pipe_t* handle);

int uv_pipe_listen(uv_pipe_t* handle, int backlog, uv_connection_cb cb);
int uv_pipe_accept(uv_pipe_t* server, uv_stream_t* client);
int uv_pipe_read_start(uv_pipe_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb);
int uv_pipe_read2_start(uv_pipe_t* handle, uv_alloc_cb alloc_cb,
    uv_read2_cb read_cb);
int uv_pipe_write(uv_loop_t* loop, uv_write_t* req, uv_pipe_t* handle,
    uv_buf_t bufs[], int bufcnt, uv_write_cb cb);
int uv_pipe_write2(uv_loop_t* loop, uv_write_t* req, uv_pipe_t* handle,
    uv_buf_t bufs[], int bufcnt, uv_stream_t* send_handle, uv_write_cb cb);

void uv_process_pipe_read_req(uv_loop_t* loop, uv_pipe_t* handle,
    uv_req_t* req);
void uv_process_pipe_write_req(uv_loop_t* loop, uv_pipe_t* handle,
    uv_write_t* req);
void uv_process_pipe_accept_req(uv_loop_t* loop, uv_pipe_t* handle,
    uv_req_t* raw_req);
void uv_process_pipe_connect_req(uv_loop_t* loop, uv_pipe_t* handle,
    uv_connect_t* req);
void uv_process_pipe_shutdown_req(uv_loop_t* loop, uv_pipe_t* handle,
    uv_shutdown_t* req);


/*
 * TTY
 */
void uv_console_init();

int uv_tty_read_start(uv_tty_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb);
int uv_tty_read_stop(uv_tty_t* handle);
int uv_tty_write(uv_loop_t* loop, uv_write_t* req, uv_tty_t* handle,
    uv_buf_t bufs[], int bufcnt, uv_write_cb cb);
void uv_tty_close(uv_tty_t* handle);

void uv_process_tty_read_req(uv_loop_t* loop, uv_tty_t* handle,
    uv_req_t* req);
void uv_process_tty_write_req(uv_loop_t* loop, uv_tty_t* handle,
    uv_write_t* req);
/* TODO: remove me */
void uv_process_tty_accept_req(uv_loop_t* loop, uv_tty_t* handle,
    uv_req_t* raw_req);
/* TODO: remove me */
void uv_process_tty_connect_req(uv_loop_t* loop, uv_tty_t* handle,
    uv_connect_t* req);

void uv_tty_endgame(uv_loop_t* loop, uv_tty_t* handle);


/*
 * Loop watchers
 */
void uv_loop_watcher_endgame(uv_loop_t* loop, uv_handle_t* handle);

void uv_prepare_invoke(uv_loop_t* loop);
void uv_check_invoke(uv_loop_t* loop);
void uv_idle_invoke(uv_loop_t* loop);


/*
 * Async watcher
 */
void uv_async_endgame(uv_loop_t* loop, uv_async_t* handle);

void uv_process_async_wakeup_req(uv_loop_t* loop, uv_async_t* handle,
    uv_req_t* req);


/*
 * Spawn
 */
void uv_process_proc_exit(uv_loop_t* loop, uv_process_t* handle);
void uv_process_proc_close(uv_loop_t* loop, uv_process_t* handle);
void uv_process_close(uv_loop_t* loop, uv_process_t* handle);
void uv_process_endgame(uv_loop_t* loop, uv_process_t* handle);


/*
 * C-ares integration
 */
typedef struct uv_ares_action_s uv_ares_action_t;

void uv_process_ares_event_req(uv_loop_t* loop, uv_ares_action_t* handle,
    uv_req_t* req);
void uv_process_ares_cleanup_req(uv_loop_t* loop, uv_ares_task_t* handle,
    uv_req_t* req);

/*
 * Getaddrinfo
 */
void uv_process_getaddrinfo_req(uv_loop_t* loop, uv_getaddrinfo_t* handle,
    uv_req_t* req);


/*
 * FS
 */
void uv_fs_init();
void uv_process_fs_req(uv_loop_t* loop, uv_fs_t* req);


/*
 * Threadpool
 */
void uv_process_work_req(uv_loop_t* loop, uv_work_t* req);


/*
 * FS Event
 */
void uv_process_fs_event_req(uv_loop_t* loop, uv_req_t* req, uv_fs_event_t* handle);
void uv_fs_event_close(uv_loop_t* loop, uv_fs_event_t* handle);
void uv_fs_event_endgame(uv_loop_t* loop, uv_fs_event_t* handle);


/* Utils */
int uv_parent_pid();
void uv_filetime_to_time_t(FILETIME* file_time,  time_t* stat_time);
void uv_fatal_error(const int errorno, const char* syscall);
uv_err_code uv_translate_sys_error(int sys_errno);

#define SET_REQ_STATUS(req, status)                                     \
   (req)->overlapped.Internal = (ULONG_PTR) (status)

#define SET_REQ_ERROR(req, error)                                       \
  SET_REQ_STATUS((req), NTSTATUS_FROM_WIN32((error)))

#define SET_REQ_SUCCESS(req)                                            \
  SET_REQ_STATUS((req), STATUS_SUCCESS)

#define GET_REQ_STATUS(req)                                             \
  ((req)->overlapped.Internal)

#define REQ_SUCCESS(req)                                                \
  (NT_SUCCESS(GET_REQ_STATUS((req))))

#define GET_REQ_ERROR(req)                                              \
  (pRtlNtStatusToDosError(GET_REQ_STATUS((req))))

#define GET_REQ_SOCK_ERROR(req)                                         \
  (uv_ntstatus_to_winsock_error(GET_REQ_STATUS((req))))


/*
 * Winapi and ntapi utility functions
 */
void uv_winapi_init();


/*
 * Winsock utility functions
 */
void uv_winsock_init();

int uv_ntstatus_to_winsock_error(NTSTATUS status);

BOOL uv_get_acceptex_function(SOCKET socket, LPFN_ACCEPTEX* target);
BOOL uv_get_connectex_function(SOCKET socket, LPFN_CONNECTEX* target);

int WSAAPI uv_wsarecv_workaround(SOCKET socket, WSABUF* buffers,
    DWORD buffer_count, DWORD* bytes, DWORD* flags, WSAOVERLAPPED *overlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE completion_routine);
int WSAAPI uv_wsarecvfrom_workaround(SOCKET socket, WSABUF* buffers,
    DWORD buffer_count, DWORD* bytes, DWORD* flags, struct sockaddr* addr,
    int* addr_len, WSAOVERLAPPED *overlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE completion_routine);

/* Whether ipv6 is supported */
extern int uv_allow_ipv6;

/* Ip address used to bind to any port at any interface */
extern struct sockaddr_in uv_addr_ip4_any_;
extern struct sockaddr_in6 uv_addr_ip6_any_;

#endif /* UV_WIN_INTERNAL_H_ */
