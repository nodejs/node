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

#ifdef _MSC_VER
# define INLINE __inline
# define UV_THREAD_LOCAL __declspec( thread )
#else
# define INLINE inline
# define UV_THREAD_LOCAL __thread
#endif


#ifdef _DEBUG

extern UV_THREAD_LOCAL int uv__crt_assert_enabled;

#define UV_BEGIN_DISABLE_CRT_ASSERT()                           \
  {                                                             \
    int uv__saved_crt_assert_enabled = uv__crt_assert_enabled;  \
    uv__crt_assert_enabled = FALSE;


#define UV_END_DISABLE_CRT_ASSERT()                             \
    uv__crt_assert_enabled = uv__saved_crt_assert_enabled;      \
  }

#else
#define UV_BEGIN_DISABLE_CRT_ASSERT()
#define UV_END_DISABLE_CRT_ASSERT()
#endif

/*
 * Handles
 * (also see handle-inl.h)
 */

/* Used by all handles. */
#define UV_HANDLE_CLOSED                        0x00000002
#define UV_HANDLE_ENDGAME_QUEUED                0x00000008

/* uv-common.h: #define UV__HANDLE_CLOSING      0x00000001 */
/* uv-common.h: #define UV__HANDLE_ACTIVE       0x00000040 */
/* uv-common.h: #define UV__HANDLE_REF          0x00000020 */
/* uv-common.h: #define UV_HANDLE_INTERNAL      0x00000080 */

/* Used by streams and UDP handles. */
#define UV_HANDLE_READING                       0x00000100
#define UV_HANDLE_BOUND                         0x00000200
#define UV_HANDLE_LISTENING                     0x00000800
#define UV_HANDLE_CONNECTION                    0x00001000
#define UV_HANDLE_READABLE                      0x00008000
#define UV_HANDLE_WRITABLE                      0x00010000
#define UV_HANDLE_READ_PENDING                  0x00020000
#define UV_HANDLE_SYNC_BYPASS_IOCP              0x00040000
#define UV_HANDLE_ZERO_READ                     0x00080000
#define UV_HANDLE_EMULATE_IOCP                  0x00100000
#define UV_HANDLE_BLOCKING_WRITES               0x00200000
#define UV_HANDLE_CANCELLATION_PENDING          0x00400000

/* Used by uv_tcp_t and uv_udp_t handles */
#define UV_HANDLE_IPV6                          0x01000000

/* Only used by uv_tcp_t handles. */
#define UV_HANDLE_TCP_NODELAY                   0x02000000
#define UV_HANDLE_TCP_KEEPALIVE                 0x04000000
#define UV_HANDLE_TCP_SINGLE_ACCEPT             0x08000000
#define UV_HANDLE_TCP_ACCEPT_STATE_CHANGING     0x10000000
#define UV_HANDLE_TCP_SOCKET_CLOSED             0x20000000
#define UV_HANDLE_SHARED_TCP_SOCKET             0x40000000

/* Only used by uv_pipe_t handles. */
#define UV_HANDLE_NON_OVERLAPPED_PIPE           0x01000000
#define UV_HANDLE_PIPESERVER                    0x02000000
#define UV_HANDLE_PIPE_READ_CANCELABLE          0x04000000

/* Only used by uv_tty_t handles. */
#define UV_HANDLE_TTY_READABLE                  0x01000000
#define UV_HANDLE_TTY_RAW                       0x02000000
#define UV_HANDLE_TTY_SAVED_POSITION            0x04000000
#define UV_HANDLE_TTY_SAVED_ATTRIBUTES          0x08000000

/* Only used by uv_poll_t handles. */
#define UV_HANDLE_POLL_SLOW                     0x02000000


/*
 * Requests: see req-inl.h
 */


/*
 * Streams: see stream-inl.h
 */


/*
 * TCP
 */

typedef struct {
  WSAPROTOCOL_INFOW socket_info;
  int delayed_error;
} uv__ipc_socket_info_ex;

int uv_tcp_listen(uv_tcp_t* handle, int backlog, uv_connection_cb cb);
int uv_tcp_accept(uv_tcp_t* server, uv_tcp_t* client);
int uv_tcp_read_start(uv_tcp_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb);
int uv_tcp_write(uv_loop_t* loop, uv_write_t* req, uv_tcp_t* handle,
    const uv_buf_t bufs[], unsigned int nbufs, uv_write_cb cb);
int uv__tcp_try_write(uv_tcp_t* handle, const uv_buf_t bufs[],
    unsigned int nbufs);

void uv_process_tcp_read_req(uv_loop_t* loop, uv_tcp_t* handle, uv_req_t* req);
void uv_process_tcp_write_req(uv_loop_t* loop, uv_tcp_t* handle,
    uv_write_t* req);
void uv_process_tcp_accept_req(uv_loop_t* loop, uv_tcp_t* handle,
    uv_req_t* req);
void uv_process_tcp_connect_req(uv_loop_t* loop, uv_tcp_t* handle,
    uv_connect_t* req);

void uv_tcp_close(uv_loop_t* loop, uv_tcp_t* tcp);
void uv_tcp_endgame(uv_loop_t* loop, uv_tcp_t* handle);

int uv_tcp_import(uv_tcp_t* tcp, uv__ipc_socket_info_ex* socket_info_ex,
    int tcp_connection);

int uv_tcp_duplicate_socket(uv_tcp_t* handle, int pid,
    LPWSAPROTOCOL_INFOW protocol_info);


/*
 * UDP
 */
void uv_process_udp_recv_req(uv_loop_t* loop, uv_udp_t* handle, uv_req_t* req);
void uv_process_udp_send_req(uv_loop_t* loop, uv_udp_t* handle,
    uv_udp_send_t* req);

void uv_udp_close(uv_loop_t* loop, uv_udp_t* handle);
void uv_udp_endgame(uv_loop_t* loop, uv_udp_t* handle);


/*
 * Pipes
 */
int uv_stdio_pipe_server(uv_loop_t* loop, uv_pipe_t* handle, DWORD access,
    char* name, size_t nameSize);

int uv_pipe_listen(uv_pipe_t* handle, int backlog, uv_connection_cb cb);
int uv_pipe_accept(uv_pipe_t* server, uv_stream_t* client);
int uv_pipe_read_start(uv_pipe_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb);
int uv_pipe_write(uv_loop_t* loop, uv_write_t* req, uv_pipe_t* handle,
    const uv_buf_t bufs[], unsigned int nbufs, uv_write_cb cb);
int uv_pipe_write2(uv_loop_t* loop, uv_write_t* req, uv_pipe_t* handle,
    const uv_buf_t bufs[], unsigned int nbufs, uv_stream_t* send_handle,
    uv_write_cb cb);
void uv__pipe_pause_read(uv_pipe_t* handle);
void uv__pipe_unpause_read(uv_pipe_t* handle);
void uv__pipe_stop_read(uv_pipe_t* handle);

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

void uv_pipe_close(uv_loop_t* loop, uv_pipe_t* handle);
void uv_pipe_cleanup(uv_loop_t* loop, uv_pipe_t* handle);
void uv_pipe_endgame(uv_loop_t* loop, uv_pipe_t* handle);


/*
 * TTY
 */
void uv_console_init();

int uv_tty_read_start(uv_tty_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb);
int uv_tty_read_stop(uv_tty_t* handle);
int uv_tty_write(uv_loop_t* loop, uv_write_t* req, uv_tty_t* handle,
    const uv_buf_t bufs[], unsigned int nbufs, uv_write_cb cb);
int uv__tty_try_write(uv_tty_t* handle, const uv_buf_t bufs[],
    unsigned int nbufs);
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
 * Poll watchers
 */
void uv_process_poll_req(uv_loop_t* loop, uv_poll_t* handle,
    uv_req_t* req);

int uv_poll_close(uv_loop_t* loop, uv_poll_t* handle);
void uv_poll_endgame(uv_loop_t* loop, uv_poll_t* handle);


/*
 * Timers
 */
void uv_timer_endgame(uv_loop_t* loop, uv_timer_t* handle);

DWORD uv__next_timeout(const uv_loop_t* loop);
void uv_process_timers(uv_loop_t* loop);


/*
 * Loop watchers
 */
void uv_loop_watcher_endgame(uv_loop_t* loop, uv_handle_t* handle);

void uv_prepare_invoke(uv_loop_t* loop);
void uv_check_invoke(uv_loop_t* loop);
void uv_idle_invoke(uv_loop_t* loop);

void uv__once_init();


/*
 * Async watcher
 */
void uv_async_close(uv_loop_t* loop, uv_async_t* handle);
void uv_async_endgame(uv_loop_t* loop, uv_async_t* handle);

void uv_process_async_wakeup_req(uv_loop_t* loop, uv_async_t* handle,
    uv_req_t* req);


/*
 * Signal watcher
 */
void uv_signals_init();
int uv__signal_dispatch(int signum);

void uv_signal_close(uv_loop_t* loop, uv_signal_t* handle);
void uv_signal_endgame(uv_loop_t* loop, uv_signal_t* handle);

void uv_process_signal_req(uv_loop_t* loop, uv_signal_t* handle,
    uv_req_t* req);


/*
 * Spawn
 */
void uv_process_proc_exit(uv_loop_t* loop, uv_process_t* handle);
void uv_process_close(uv_loop_t* loop, uv_process_t* handle);
void uv_process_endgame(uv_loop_t* loop, uv_process_t* handle);


/*
 * Error
 */
int uv_translate_sys_error(int sys_errno);


/*
 * FS
 */
void uv_fs_init();


/*
 * FS Event
 */
void uv_process_fs_event_req(uv_loop_t* loop, uv_req_t* req,
    uv_fs_event_t* handle);
void uv_fs_event_close(uv_loop_t* loop, uv_fs_event_t* handle);
void uv_fs_event_endgame(uv_loop_t* loop, uv_fs_event_t* handle);


/*
 * Stat poller.
 */
void uv__fs_poll_endgame(uv_loop_t* loop, uv_fs_poll_t* handle);


/*
 * Utilities.
 */
void uv__util_init();

uint64_t uv__hrtime(double scale);
int uv_parent_pid();
int uv_current_pid();
__declspec(noreturn) void uv_fatal_error(const int errorno, const char* syscall);
int uv__getpwuid_r(uv_passwd_t* pwd);
int uv__convert_utf16_to_utf8(const WCHAR* utf16, int utf16len, char** utf8);


/*
 * Process stdio handles.
 */
int uv__stdio_create(uv_loop_t* loop,
                     const uv_process_options_t* options,
                     BYTE** buffer_ptr);
void uv__stdio_destroy(BYTE* buffer);
void uv__stdio_noinherit(BYTE* buffer);
int uv__stdio_verify(BYTE* buffer, WORD size);
WORD uv__stdio_size(BYTE* buffer);
HANDLE uv__stdio_handle(BYTE* buffer, int fd);


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

int WSAAPI uv_msafd_poll(SOCKET socket, AFD_POLL_INFO* info_in,
    AFD_POLL_INFO* info_out, OVERLAPPED* overlapped);

/* Whether there are any non-IFS LSPs stacked on TCP */
extern int uv_tcp_non_ifs_lsp_ipv4;
extern int uv_tcp_non_ifs_lsp_ipv6;

/* Ip address used to bind to any port at any interface */
extern struct sockaddr_in uv_addr_ip4_any_;
extern struct sockaddr_in6 uv_addr_ip6_any_;

#endif /* UV_WIN_INTERNAL_H_ */
