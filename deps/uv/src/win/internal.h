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

#include "uv/tree.h"
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
 * TCP
 */

typedef enum {
  UV__IPC_SOCKET_XFER_NONE = 0,
  UV__IPC_SOCKET_XFER_TCP_CONNECTION,
  UV__IPC_SOCKET_XFER_TCP_SERVER
} uv__ipc_socket_xfer_type_t;

typedef struct {
  WSAPROTOCOL_INFOW socket_info;
  uint32_t delayed_error;
} uv__ipc_socket_xfer_info_t;

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

int uv__tcp_xfer_export(uv_tcp_t* handle,
                        int pid,
                        uv__ipc_socket_xfer_type_t* xfer_type,
                        uv__ipc_socket_xfer_info_t* xfer_info);
int uv__tcp_xfer_import(uv_tcp_t* tcp,
                        uv__ipc_socket_xfer_type_t xfer_type,
                        uv__ipc_socket_xfer_info_t* xfer_info);


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
int uv__create_stdio_pipe_pair(uv_loop_t* loop,
    uv_pipe_t* parent_pipe, HANDLE* child_pipe_ptr, unsigned int flags);

int uv_pipe_listen(uv_pipe_t* handle, int backlog, uv_connection_cb cb);
int uv_pipe_accept(uv_pipe_t* server, uv_stream_t* client);
int uv_pipe_read_start(uv_pipe_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb);
void uv__pipe_read_stop(uv_pipe_t* handle);
int uv__pipe_write(uv_loop_t* loop,
                   uv_write_t* req,
                   uv_pipe_t* handle,
                   const uv_buf_t bufs[],
                   size_t nbufs,
                   uv_stream_t* send_handle,
                   uv_write_cb cb);

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
void uv_console_init(void);

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
/*
 * uv_process_tty_accept_req() is a stub to keep DELEGATE_STREAM_REQ working
 * TODO: find a way to remove it
 */
void uv_process_tty_accept_req(uv_loop_t* loop, uv_tty_t* handle,
    uv_req_t* raw_req);
/*
 * uv_process_tty_connect_req() is a stub to keep DELEGATE_STREAM_REQ working
 * TODO: find a way to remove it
 */
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
 * Loop watchers
 */
void uv_loop_watcher_endgame(uv_loop_t* loop, uv_handle_t* handle);

void uv_prepare_invoke(uv_loop_t* loop);
void uv_check_invoke(uv_loop_t* loop);
void uv_idle_invoke(uv_loop_t* loop);

void uv__once_init(void);


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
void uv_signals_init(void);
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
void uv_fs_init(void);


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
void uv__util_init(void);

uint64_t uv__hrtime(unsigned int scale);
__declspec(noreturn) void uv_fatal_error(const int errorno, const char* syscall);
int uv__getpwuid_r(uv_passwd_t* pwd);
int uv__convert_utf16_to_utf8(const WCHAR* utf16, int utf16len, char** utf8);
int uv__convert_utf8_to_utf16(const char* utf8, int utf8len, WCHAR** utf16);

typedef int (WINAPI *uv__peersockfunc)(SOCKET, struct sockaddr*, int*);

int uv__getsockpeername(const uv_handle_t* handle,
                        uv__peersockfunc func,
                        struct sockaddr* name,
                        int* namelen,
                        int delayed_error);

int uv__random_rtlgenrandom(void* buf, size_t buflen);


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
void uv_winapi_init(void);


/*
 * Winsock utility functions
 */
void uv_winsock_init(void);

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

/*
 * Wake all loops with fake message
 */
void uv__wake_all_loops(void);

/*
 * Init system wake-up detection
 */
void uv__init_detect_system_wakeup(void);

#endif /* UV_WIN_INTERNAL_H_ */
