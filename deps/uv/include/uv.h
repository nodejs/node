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

/* See uv_loop_new for an introduction. */

#ifndef UV_H
#define UV_H
#ifdef __cplusplus
extern "C" {
#endif

#define UV_VERSION_MAJOR 0
#define UV_VERSION_MINOR 1

#define CARES_STATICLIB 1

#include <stdint.h> /* int64_t */
#include <sys/types.h> /* size_t */

#include "ares.h"

#ifndef _SSIZE_T_
typedef intptr_t ssize_t;
#endif

typedef struct uv_loop_s uv_loop_t;
typedef struct uv_ares_task_s uv_ares_task_t;
typedef struct uv_err_s uv_err_t;
typedef struct uv_handle_s uv_handle_t;
typedef struct uv_stream_s uv_stream_t;
typedef struct uv_tcp_s uv_tcp_t;
typedef struct uv_udp_s uv_udp_t;
typedef struct uv_pipe_s uv_pipe_t;
typedef struct uv_tty_s uv_tty_t;
typedef struct uv_timer_s uv_timer_t;
typedef struct uv_prepare_s uv_prepare_t;
typedef struct uv_check_s uv_check_t;
typedef struct uv_idle_s uv_idle_t;
typedef struct uv_async_s uv_async_t;
typedef struct uv_getaddrinfo_s uv_getaddrinfo_t;
typedef struct uv_process_s uv_process_t;
typedef struct uv_counters_s uv_counters_t;
/* Request types */
typedef struct uv_req_s uv_req_t;
typedef struct uv_shutdown_s uv_shutdown_t;
typedef struct uv_write_s uv_write_t;
typedef struct uv_connect_s uv_connect_t;
typedef struct uv_udp_send_s uv_udp_send_t;
typedef struct uv_fs_s uv_fs_t;
/* uv_fs_event_t is a subclass of uv_handle_t. */
typedef struct uv_fs_event_s uv_fs_event_t;
typedef struct uv_work_s uv_work_t;

#if defined(__unix__) || defined(__POSIX__) || defined(__APPLE__)
# include "uv-private/uv-unix.h"
#else
# include "uv-private/uv-win.h"
#endif


/*
 * This function must be called before any other functions in libuv.
 *
 * All functions besides uv_run() are non-blocking.
 *
 * All callbacks in libuv are made asynchronously. That is they are never
 * made by the function that takes them as a parameter.
 */
uv_loop_t* uv_loop_new();
void uv_loop_delete(uv_loop_t*);


/*
 * Returns the default loop.
 */
uv_loop_t* uv_default_loop();

/*
 * This function starts the event loop. It blocks until the reference count
 * of the loop drops to zero.
 */
int uv_run(uv_loop_t*);

/*
 * Manually modify the event loop's reference count. Useful if the user wants
 * to have a handle or timeout that doesn't keep the loop alive.
 */
void uv_ref(uv_loop_t*);
void uv_unref(uv_loop_t*);

void uv_update_time(uv_loop_t*);
int64_t uv_now(uv_loop_t*);


/*
 * The status parameter is 0 if the request completed successfully,
 * and should be -1 if the request was cancelled or failed.
 * For uv_close_cb, -1 means that the handle was closed due to an error.
 * Error details can be obtained by calling uv_last_error().
 *
 * In the case of uv_read_cb the uv_buf_t returned should be freed by the
 * user.
 */
typedef uv_buf_t (*uv_alloc_cb)(uv_handle_t* handle, size_t suggested_size);
typedef void (*uv_read_cb)(uv_stream_t* stream, ssize_t nread, uv_buf_t buf);
typedef void (*uv_write_cb)(uv_write_t* req, int status);
typedef void (*uv_connect_cb)(uv_connect_t* req, int status);
typedef void (*uv_shutdown_cb)(uv_shutdown_t* req, int status);
typedef void (*uv_connection_cb)(uv_stream_t* server, int status);
typedef void (*uv_close_cb)(uv_handle_t* handle);
typedef void (*uv_timer_cb)(uv_timer_t* handle, int status);
/* TODO: do these really need a status argument? */
typedef void (*uv_async_cb)(uv_async_t* handle, int status);
typedef void (*uv_prepare_cb)(uv_prepare_t* handle, int status);
typedef void (*uv_check_cb)(uv_check_t* handle, int status);
typedef void (*uv_idle_cb)(uv_idle_t* handle, int status);
typedef void (*uv_getaddrinfo_cb)(uv_getaddrinfo_t* handle, int status,
    struct addrinfo* res);
typedef void (*uv_exit_cb)(uv_process_t*, int exit_status, int term_signal);
typedef void (*uv_fs_cb)(uv_fs_t* req);
typedef void (*uv_work_cb)(uv_work_t* req);
typedef void (*uv_after_work_cb)(uv_work_t* req);

/*
* This will be called repeatedly after the uv_fs_event_t is initialized.
* If uv_fs_event_t was initialized with a directory the filename parameter
* will be a relative path to a file contained in the directory.
* The events paramenter is an ORed mask of enum uv_fs_event elements.
*/
typedef void (*uv_fs_event_cb)(uv_fs_event_t* handle, const char* filename,
    int events, int status);


/* Expand this list if necessary. */
typedef enum {
  UV_UNKNOWN = -1,
  UV_OK = 0,
  UV_EOF,
  UV_EACCESS,
  UV_EAGAIN,
  UV_EADDRINUSE,
  UV_EADDRNOTAVAIL,
  UV_EAFNOSUPPORT,
  UV_EALREADY,
  UV_EBADF,
  UV_EBUSY,
  UV_ECONNABORTED,
  UV_ECONNREFUSED,
  UV_ECONNRESET,
  UV_EDESTADDRREQ,
  UV_EFAULT,
  UV_EHOSTUNREACH,
  UV_EINTR,
  UV_EINVAL,
  UV_EISCONN,
  UV_EMFILE,
  UV_EMSGSIZE,
  UV_ENETDOWN,
  UV_ENETUNREACH,
  UV_ENFILE,
  UV_ENOBUFS,
  UV_ENOMEM,
  UV_ENONET,
  UV_ENOPROTOOPT,
  UV_ENOTCONN,
  UV_ENOTSOCK,
  UV_ENOTSUP,
  UV_ENOENT,
  UV_EPIPE,
  UV_EPROTO,
  UV_EPROTONOSUPPORT,
  UV_EPROTOTYPE,
  UV_ETIMEDOUT,
  UV_ECHARSET,
  UV_EAIFAMNOSUPPORT,
  UV_EAINONAME,
  UV_EAISERVICE,
  UV_EAISOCKTYPE,
  UV_ESHUTDOWN,
  UV_EEXIST
} uv_err_code;

typedef enum {
  UV_UNKNOWN_HANDLE = 0,
  UV_TCP,
  UV_UDP,
  UV_NAMED_PIPE,
  UV_TTY,
  UV_FILE,
  UV_TIMER,
  UV_PREPARE,
  UV_CHECK,
  UV_IDLE,
  UV_ASYNC,
  UV_ARES_TASK,
  UV_ARES_EVENT,
  UV_PROCESS,
  UV_FS_EVENT
} uv_handle_type;

typedef enum {
  UV_UNKNOWN_REQ = 0,
  UV_CONNECT,
  UV_ACCEPT,
  UV_READ,
  UV_WRITE,
  UV_SHUTDOWN,
  UV_WAKEUP,
  UV_UDP_SEND,
  UV_FS,
  UV_WORK,
  UV_GETADDRINFO,
  UV_REQ_TYPE_PRIVATE
} uv_req_type;


struct uv_err_s {
  /* read-only */
  uv_err_code code;
  /* private */
  int sys_errno_;
};


/*
 * Most functions return boolean: 0 for success and -1 for failure.
 * On error the user should then call uv_last_error() to determine
 * the error code.
 */
uv_err_t uv_last_error(uv_loop_t*);
char* uv_strerror(uv_err_t err);
const char* uv_err_name(uv_err_t err);


#define UV_REQ_FIELDS \
  /* read-only */ \
  uv_req_type type; \
  /* public */ \
  void* data; \
  /* private */ \
  UV_REQ_PRIVATE_FIELDS

/* Abstract base class of all requests. */
struct uv_req_s {
  UV_REQ_FIELDS
};


/* Platform-specific request types */
UV_PRIVATE_REQ_TYPES


/*
 * uv_shutdown_t is a subclass of uv_req_t
 *
 * Shutdown the outgoing (write) side of a duplex stream. It waits for
 * pending write requests to complete. The handle should refer to a
 * initialized stream. req should be an uninitalized shutdown request
 * struct. The cb is a called after shutdown is complete.
 */
int uv_shutdown(uv_shutdown_t* req, uv_stream_t* handle, uv_shutdown_cb cb);

struct uv_shutdown_s {
  UV_REQ_FIELDS
  uv_stream_t* handle;
  uv_shutdown_cb cb;
  UV_SHUTDOWN_PRIVATE_FIELDS
};


#define UV_HANDLE_FIELDS \
  /* read-only */ \
  uv_loop_t* loop; \
  uv_handle_type type; \
  /* public */ \
  uv_close_cb close_cb; \
  void* data; \
  /* private */ \
  UV_HANDLE_PRIVATE_FIELDS

/* The abstract base class of all handles.  */
struct uv_handle_s {
  UV_HANDLE_FIELDS
};

/*
 * Returns 1 if the prepare/check/idle handle has been started, 0 otherwise.
 * For other handle types this always returns 1.
 */
int uv_is_active(uv_handle_t* handle);

/*
 * Request handle to be closed. close_cb will be called asynchronously after
 * this call. This MUST be called on each handle before memory is released.
 *
 * Note that handles that wrap file descriptors are closed immediately but
 * close_cb will still be deferred to the next iteration of the event loop.
 * It gives you a chance to free up any resources associated with the handle.
 */
void uv_close(uv_handle_t* handle, uv_close_cb close_cb);


/*
 * Constructor for uv_buf_t.
 * Due to platform differences the user cannot rely on the ordering of the
 * base and len members of the uv_buf_t struct. The user is responsible for
 * freeing base after the uv_buf_t is done. Return struct passed by value.
 */
uv_buf_t uv_buf_init(char* base, size_t len);


#define UV_STREAM_FIELDS \
  /* number of bytes queued for writing */ \
  size_t write_queue_size; \
  /* private */ \
  UV_STREAM_PRIVATE_FIELDS

/*
 * uv_stream_t is a subclass of uv_handle_t
 *
 * uv_stream is an abstract class.
 *
 * uv_stream_t is the parent class of uv_tcp_t, uv_pipe_t, uv_tty_t
 * and soon uv_file_t.
 */
struct uv_stream_s {
  UV_HANDLE_FIELDS
  UV_STREAM_FIELDS
};

int uv_listen(uv_stream_t* stream, int backlog, uv_connection_cb cb);

/*
 * This call is used in conjunction with uv_listen() to accept incoming
 * connections. Call uv_accept after receiving a uv_connection_cb to accept
 * the connection. Before calling uv_accept use uv_*_init() must be
 * called on the client. Non-zero return value indicates an error.
 *
 * When the uv_connection_cb is called it is guaranteed that uv_accept will
 * complete successfully the first time. If you attempt to use it more than
 * once, it may fail. It is suggested to only call uv_accept once per
 * uv_connection_cb call.
 */
int uv_accept(uv_stream_t* server, uv_stream_t* client);

/*
 * Read data from an incoming stream. The callback will be made several
 * several times until there is no more data to read or uv_read_stop is
 * called. When we've reached EOF nread will be set to -1 and the error is
 * set to UV_EOF. When nread == -1 the buf parameter might not point to a
 * valid buffer; in that case buf.len and buf.base are both set to 0.
 * Note that nread might also be 0, which does *not* indicate an error or
 * eof; it happens when libuv requested a buffer through the alloc callback
 * but then decided that it didn't need that buffer.
 */
int uv_read_start(uv_stream_t*, uv_alloc_cb alloc_cb, uv_read_cb read_cb);

int uv_read_stop(uv_stream_t*);

typedef enum {
  UV_STDIN = 0,
  UV_STDOUT,
  UV_STDERR
} uv_std_type;

uv_stream_t* uv_std_handle(uv_loop_t*, uv_std_type type);

/*
 * Write data to stream. Buffers are written in order. Example:
 *
 *   uv_buf_t a[] = {
 *     { .base = "1", .len = 1 },
 *     { .base = "2", .len = 1 }
 *   };
 *
 *   uv_buf_t b[] = {
 *     { .base = "3", .len = 1 },
 *     { .base = "4", .len = 1 }
 *   };
 *
 *   // writes "1234"
 *   uv_write(req, stream, a, 2);
 *   uv_write(req, stream, b, 2);
 *
 */
int uv_write(uv_write_t* req, uv_stream_t* handle, uv_buf_t bufs[], int bufcnt,
    uv_write_cb cb);

/* uv_write_t is a subclass of uv_req_t */
struct uv_write_s {
  UV_REQ_FIELDS
  uv_write_cb cb;
  uv_stream_t* handle;
  UV_WRITE_PRIVATE_FIELDS
};



/*
 * uv_tcp_t is a subclass of uv_stream_t
 *
 * Represents a TCP stream or TCP server.
 */
struct uv_tcp_s {
  UV_HANDLE_FIELDS
  UV_STREAM_FIELDS
  UV_TCP_PRIVATE_FIELDS
};

int uv_tcp_init(uv_loop_t*, uv_tcp_t* handle);

int uv_tcp_bind(uv_tcp_t* handle, struct sockaddr_in);
int uv_tcp_bind6(uv_tcp_t* handle, struct sockaddr_in6);
int uv_tcp_getsockname(uv_tcp_t* handle, struct sockaddr* name, int* namelen);
int uv_tcp_getpeername(uv_tcp_t* handle, struct sockaddr* name, int* namelen);

/*
 * uv_tcp_connect, uv_tcp_connect6
 * These functions establish IPv4 and IPv6 TCP connections. Provide an
 * initialized TCP handle and an uninitialized uv_connect_t*. The callback
 * will be made when the connection is estabished.
 */
int uv_tcp_connect(uv_connect_t* req, uv_tcp_t* handle,
    struct sockaddr_in address, uv_connect_cb cb);
int uv_tcp_connect6(uv_connect_t* req, uv_tcp_t* handle,
    struct sockaddr_in6 address, uv_connect_cb cb);

/* uv_connect_t is a subclass of uv_req_t */
struct uv_connect_s {
  UV_REQ_FIELDS
  uv_connect_cb cb;
  uv_stream_t* handle;
  UV_CONNECT_PRIVATE_FIELDS
};


/*
 * UDP support.
 */

enum uv_udp_flags {
  /* Disables dual stack mode. Used with uv_udp_bind6(). */
  UV_UDP_IPV6ONLY = 1,
  /*
   * Indicates message was truncated because read buffer was too small. The
   * remainder was discarded by the OS. Used in uv_udp_recv_cb.
   */
  UV_UDP_PARTIAL = 2
};

/*
 * Called after a uv_udp_send() or uv_udp_send6(). status 0 indicates
 * success otherwise error.
 */
typedef void (*uv_udp_send_cb)(uv_udp_send_t* req, int status);

/*
 * Callback that is invoked when a new UDP datagram is received.
 *
 *  handle  UDP handle.
 *  nread   Number of bytes that have been received.
 *          0 if there is no more data to read. You may
 *          discard or repurpose the read buffer.
 *          -1 if a transmission error was detected.
 *  buf     uv_buf_t with the received data.
 *  addr    struct sockaddr_in or struct sockaddr_in6.
 *          Valid for the duration of the callback only.
 *  flags   One or more OR'ed UV_UDP_* constants.
 *          Right now only UV_UDP_PARTIAL is used.
 */
typedef void (*uv_udp_recv_cb)(uv_udp_t* handle, ssize_t nread, uv_buf_t buf,
    struct sockaddr* addr, unsigned flags);

/* uv_udp_t is a subclass of uv_handle_t */
struct uv_udp_s {
  UV_HANDLE_FIELDS
  UV_UDP_PRIVATE_FIELDS
};

/* uv_udp_send_t is a subclass of uv_req_t */
struct uv_udp_send_s {
  UV_REQ_FIELDS
  uv_udp_t* handle;
  uv_udp_send_cb cb;
  UV_UDP_SEND_PRIVATE_FIELDS
};

/*
 * Initialize a new UDP handle. The actual socket is created lazily.
 * Returns 0 on success.
 */
int uv_udp_init(uv_loop_t*, uv_udp_t* handle);

/*
 * Bind to a IPv4 address and port.
 *
 * Arguments:
 *  handle    UDP handle. Should have been initialized with `uv_udp_init`.
 *  addr      struct sockaddr_in with the address and port to bind to.
 *  flags     Unused.
 *
 * Returns:
 *  0 on success, -1 on error.
 */
int uv_udp_bind(uv_udp_t* handle, struct sockaddr_in addr, unsigned flags);

/*
 * Bind to a IPv6 address and port.
 *
 * Arguments:
 *  handle    UDP handle. Should have been initialized with `uv_udp_init`.
 *  addr      struct sockaddr_in with the address and port to bind to.
 *  flags     Should be 0 or UV_UDP_IPV6ONLY.
 *
 * Returns:
 *  0 on success, -1 on error.
 */
int uv_udp_bind6(uv_udp_t* handle, struct sockaddr_in6 addr, unsigned flags);
int uv_udp_getsockname(uv_udp_t* handle, struct sockaddr* name, int* namelen);

/*
 * Send data. If the socket has not previously been bound with `uv_udp_bind`
 * or `uv_udp_bind6`, it is bound to 0.0.0.0 (the "all interfaces" address)
 * and a random port number.
 *
 * Arguments:
 *  req       UDP request handle. Need not be initialized.
 *  handle    UDP handle. Should have been initialized with `uv_udp_init`.
 *  bufs      List of buffers to send.
 *  bufcnt    Number of buffers in `bufs`.
 *  addr      Address of the remote peer. See `uv_ip4_addr`.
 *  send_cb   Callback to invoke when the data has been sent out.
 *
 * Returns:
 *  0 on success, -1 on error.
 */
int uv_udp_send(uv_udp_send_t* req, uv_udp_t* handle, uv_buf_t bufs[],
    int bufcnt, struct sockaddr_in addr, uv_udp_send_cb send_cb);

/*
 * Send data. If the socket has not previously been bound with `uv_udp_bind6`,
 * it is bound to ::0 (the "all interfaces" address) and a random port number.
 *
 * Arguments:
 *  req       UDP request handle. Need not be initialized.
 *  handle    UDP handle. Should have been initialized with `uv_udp_init`.
 *  bufs      List of buffers to send.
 *  bufcnt    Number of buffers in `bufs`.
 *  addr      Address of the remote peer. See `uv_ip6_addr`.
 *  send_cb   Callback to invoke when the data has been sent out.
 *
 * Returns:
 *  0 on success, -1 on error.
 */
int uv_udp_send6(uv_udp_send_t* req, uv_udp_t* handle, uv_buf_t bufs[],
    int bufcnt, struct sockaddr_in6 addr, uv_udp_send_cb send_cb);

/*
 * Send data. If the socket has not previously been bound with `uv_udp_bind`
 * or `uv_udp_bind6`, it is bound to 0.0.0.0 (the "all interfaces" address)
 * and a random port number.
 *
 * Arguments:
 *  handle    UDP handle. Should have been initialized with `uv_udp_init`.
 *  alloc_cb  Callback to invoke when temporary storage is needed.
 *  recv_cb   Callback to invoke with received data.
 *
 * Returns:
 *  0 on success, -1 on error.
 */
int uv_udp_recv_start(uv_udp_t* handle, uv_alloc_cb alloc_cb,
    uv_udp_recv_cb recv_cb);

/*
 * Stop listening for incoming datagrams.
 *
 * Arguments:
 *  handle    UDP handle. Should have been initialized with `uv_udp_init`.
 *
 * Returns:
 *  0 on success, -1 on error.
 */
int uv_udp_recv_stop(uv_udp_t* handle);


/*
 * uv_tty_t is a subclass of uv_stream_t
 *
 * Representing a stream for the console.
 */
struct uv_tty_s {
  UV_HANDLE_FIELDS
  UV_STREAM_FIELDS
  UV_TTY_PRIVATE_FIELDS
};

int uv_tty_init(uv_loop_t*, uv_tty_t*, uv_file fd);

/*
 * Set mode. 0 for normal, 1 for raw.
 */
int uv_tty_set_mode(uv_tty_t*, int mode);

/*
 * Gets the current Window size. On success zero is returned.
 */
int uv_tty_get_winsize(uv_tty_t*, int* width, int* height);

/*
 * Used to detect what type of stream should be used with a given file
 * descriptor. Usually this will be used during initialization to guess the
 * type of the stdio streams.
 * For isatty() functionality use this function and test for UV_TTY.
 */
uv_handle_type uv_guess_handle(uv_file file);

/*
 * uv_pipe_t is a subclass of uv_stream_t
 *
 * Representing a pipe stream or pipe server. On Windows this is a Named
 * Pipe. On Unix this is a UNIX domain socket.
 */
struct uv_pipe_s {
  UV_HANDLE_FIELDS
  UV_STREAM_FIELDS
  UV_PIPE_PRIVATE_FIELDS
};

int uv_pipe_init(uv_loop_t*, uv_pipe_t* handle);

/*
 * Opens an existing file descriptor or HANDLE as a pipe.
 */
void uv_pipe_open(uv_pipe_t*, uv_file file);

int uv_pipe_bind(uv_pipe_t* handle, const char* name);

int uv_pipe_connect(uv_connect_t* req, uv_pipe_t* handle,
    const char* name, uv_connect_cb cb);


/*
 * uv_prepare_t is a subclass of uv_handle_t.
 *
 * libev wrapper. Every active prepare handle gets its callback called
 * exactly once per loop iteration, just before the system blocks to wait
 * for completed i/o.
 */
struct uv_prepare_s {
  UV_HANDLE_FIELDS
  UV_PREPARE_PRIVATE_FIELDS
};

int uv_prepare_init(uv_loop_t*, uv_prepare_t* prepare);

int uv_prepare_start(uv_prepare_t* prepare, uv_prepare_cb cb);

int uv_prepare_stop(uv_prepare_t* prepare);


/*
 * uv_check_t is a subclass of uv_handle_t.
 *
 * libev wrapper. Every active check handle gets its callback called exactly
 * once per loop iteration, just after the system returns from blocking.
 */
struct uv_check_s {
  UV_HANDLE_FIELDS
  UV_CHECK_PRIVATE_FIELDS
};

int uv_check_init(uv_loop_t*, uv_check_t* check);

int uv_check_start(uv_check_t* check, uv_check_cb cb);

int uv_check_stop(uv_check_t* check);


/*
 * uv_idle_t is a subclass of uv_handle_t.
 *
 * libev wrapper. Every active idle handle gets its callback called
 * repeatedly until it is stopped. This happens after all other types of
 * callbacks are processed.  When there are multiple "idle" handles active,
 * their callbacks are called in turn.
 */
struct uv_idle_s {
  UV_HANDLE_FIELDS
  UV_IDLE_PRIVATE_FIELDS
};

int uv_idle_init(uv_loop_t*, uv_idle_t* idle);

int uv_idle_start(uv_idle_t* idle, uv_idle_cb cb);

int uv_idle_stop(uv_idle_t* idle);


/*
 * uv_async_t is a subclass of uv_handle_t.
 *
 * libev wrapper. uv_async_send wakes up the event
 * loop and calls the async handle's callback There is no guarantee that
 * every uv_async_send call leads to exactly one invocation of the callback;
 * The only guarantee is that the callback function is  called at least once
 * after the call to async_send. Unlike all other libuv functions,
 * uv_async_send can be called from another thread.
 */
struct uv_async_s {
  UV_HANDLE_FIELDS
  UV_ASYNC_PRIVATE_FIELDS
};

int uv_async_init(uv_loop_t*, uv_async_t* async, uv_async_cb async_cb);

/*
 * This can be called from other threads to wake up a libuv thread.
 *
 * libuv is single threaded at the moment.
 */
int uv_async_send(uv_async_t* async);


/*
 * uv_timer_t is a subclass of uv_handle_t.
 *
 * Wraps libev's ev_timer watcher. Used to get woken up at a specified time
 * in the future.
 */
struct uv_timer_s {
  UV_HANDLE_FIELDS
  UV_TIMER_PRIVATE_FIELDS
};

int uv_timer_init(uv_loop_t*, uv_timer_t* timer);

int uv_timer_start(uv_timer_t* timer, uv_timer_cb cb, int64_t timeout,
    int64_t repeat);

int uv_timer_stop(uv_timer_t* timer);

/*
 * Stop the timer, and if it is repeating restart it using the repeat value
 * as the timeout. If the timer has never been started before it returns -1 and
 * sets the error to UV_EINVAL.
 */
int uv_timer_again(uv_timer_t* timer);

/*
 * Set the repeat value. Note that if the repeat value is set from a timer
 * callback it does not immediately take effect. If the timer was nonrepeating
 * before, it will have been stopped. If it was repeating, then the old repeat
 * value will have been used to schedule the next timeout.
 */
void uv_timer_set_repeat(uv_timer_t* timer, int64_t repeat);

int64_t uv_timer_get_repeat(uv_timer_t* timer);


/* c-ares integration initialize and terminate */
int uv_ares_init_options(uv_loop_t*,
                         ares_channel *channelptr,
                         struct ares_options *options,
                         int optmask);

/* TODO remove the loop argument from this function? */
void uv_ares_destroy(uv_loop_t*, ares_channel channel);


/*
 * uv_getaddrinfo_t is a subclass of uv_req_t
 *
 * Request object for uv_getaddrinfo.
 */
struct uv_getaddrinfo_s {
  UV_REQ_FIELDS
  /* read-only */
  uv_loop_t* loop; \
  UV_GETADDRINFO_PRIVATE_FIELDS
};


/*
 * Asynchronous getaddrinfo(3).
 *
 * Return code 0 means that request is accepted and callback will be called
 * with result. Other return codes mean that there will not be a callback.
 * Input arguments may be released after return from this call.
 *
 * uv_freeaddrinfo() must be called after completion to free the addrinfo
 * structure.
 */
 int uv_getaddrinfo(uv_loop_t*,
                    uv_getaddrinfo_t* handle,
                    uv_getaddrinfo_cb getaddrinfo_cb,
                    const char* node,
                    const char* service,
                    const struct addrinfo* hints);

void uv_freeaddrinfo(struct addrinfo* ai);

/* uv_spawn() options */
typedef struct uv_process_options_s {
  uv_exit_cb exit_cb; /* Called after the process exits. */
  const char* file; /* Path to program to execute. */
  /*
   * Command line arguments. args[0] should be the path to the program. On
   * Windows this uses CreateProcess which concatinates the arguments into a
   * string this can cause some strange errors. See the note at
   * windows_verbatim_arguments.
   */
  char** args;
  /*
   * This will be set as the environ variable in the subprocess. If this is
   * NULL then the parents environ will be used.
   */
  char** env;
  /*
   * If non-null this represents a directory the subprocess should execute
   * in. Stands for current working directory.
   */
  char* cwd;

  /*
   * TODO describe how this works.
   */
  int windows_verbatim_arguments;

  /*
   * The user should supply pointers to initialized uv_pipe_t structs for
   * stdio. This is used to to send or receive input from the subprocess.
   * The user is reponsible for calling uv_close on them.
   */
  uv_pipe_t* stdin_stream;
  uv_pipe_t* stdout_stream;
  uv_pipe_t* stderr_stream;
} uv_process_options_t;

/*
 * uv_process_t is a subclass of uv_handle_t
 */
struct uv_process_s {
  UV_HANDLE_FIELDS
  uv_exit_cb exit_cb;
  int pid;
  UV_PROCESS_PRIVATE_FIELDS
};

/* Initializes uv_process_t and starts the process. */
int uv_spawn(uv_loop_t*, uv_process_t*, uv_process_options_t options);

/*
 * Kills the process with the specified signal. The user must still
 * call uv_close on the process.
 */
int uv_process_kill(uv_process_t*, int signum);


/*
 * uv_work_t is a subclass of uv_req_t
 */
struct uv_work_s {
  UV_REQ_FIELDS
  uv_loop_t* loop;
  uv_work_cb work_cb;
  uv_after_work_cb after_work_cb;
  UV_WORK_PRIVATE_FIELDS
};

/* Queues a work request to execute asynchronously on the thread pool. */
int uv_queue_work(uv_loop_t* loop, uv_work_t* req, uv_work_cb work_cb,
    uv_after_work_cb after_work_cb);




/*
 * File System Methods.
 *
 * The uv_fs_* functions execute a blocking system call asynchronously (in a
 * thread pool) and call the specified callback in the specified loop after
 * completion. If the user gives NULL as the callback the blocking system
 * call will be called synchronously. req should be a pointer to an
 * uninitialized uv_fs_t object.
 *
 * uv_fs_req_cleanup() must be called after completion of the uv_fs_
 * function to free any internal memory allocations associted with the
 * request.
 */

typedef enum {
  UV_FS_UNKNOWN = -1,
  UV_FS_CUSTOM,
  UV_FS_OPEN,
  UV_FS_CLOSE,
  UV_FS_READ,
  UV_FS_WRITE,
  UV_FS_SENDFILE,
  UV_FS_STAT,
  UV_FS_LSTAT,
  UV_FS_FSTAT,
  UV_FS_FTRUNCATE,
  UV_FS_UTIME,
  UV_FS_FUTIME,
  UV_FS_CHMOD,
  UV_FS_FCHMOD,
  UV_FS_FSYNC,
  UV_FS_FDATASYNC,
  UV_FS_UNLINK,
  UV_FS_RMDIR,
  UV_FS_MKDIR,
  UV_FS_RENAME,
  UV_FS_READDIR,
  UV_FS_LINK,
  UV_FS_SYMLINK,
  UV_FS_READLINK,
  UV_FS_CHOWN,
  UV_FS_FCHOWN
} uv_fs_type;

/* uv_fs_t is a subclass of uv_req_t */
struct uv_fs_s {
  UV_REQ_FIELDS
  uv_loop_t* loop;
  uv_fs_type fs_type;
  uv_fs_cb cb;
  ssize_t result;
  void* ptr;
  char* path;
  int errorno;
  UV_FS_PRIVATE_FIELDS
};

void uv_fs_req_cleanup(uv_fs_t* req);

int uv_fs_close(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb);

int uv_fs_open(uv_loop_t* loop, uv_fs_t* req, const char* path, int flags,
    int mode, uv_fs_cb cb);

int uv_fs_read(uv_loop_t* loop, uv_fs_t* req, uv_file file, void* buf,
    size_t length, off_t offset, uv_fs_cb cb);

int uv_fs_unlink(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb);

int uv_fs_write(uv_loop_t* loop, uv_fs_t* req, uv_file file, void* buf,
    size_t length, off_t offset, uv_fs_cb cb);

int uv_fs_mkdir(uv_loop_t* loop, uv_fs_t* req, const char* path, int mode,
    uv_fs_cb cb);

int uv_fs_rmdir(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb);

int uv_fs_readdir(uv_loop_t* loop, uv_fs_t* req, const char* path, int flags,
    uv_fs_cb cb);

int uv_fs_stat(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb);

int uv_fs_fstat(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb);

int uv_fs_rename(uv_loop_t* loop, uv_fs_t* req, const char* path,
    const char* new_path, uv_fs_cb cb);

int uv_fs_fsync(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb);

int uv_fs_fdatasync(uv_loop_t* loop, uv_fs_t* req, uv_file file, uv_fs_cb cb);

int uv_fs_ftruncate(uv_loop_t* loop, uv_fs_t* req, uv_file file,
    off_t offset, uv_fs_cb cb);

int uv_fs_sendfile(uv_loop_t* loop, uv_fs_t* req, uv_file out_fd,
    uv_file in_fd, off_t in_offset, size_t length, uv_fs_cb cb);

int uv_fs_chmod(uv_loop_t* loop, uv_fs_t* req, const char* path, int mode,
    uv_fs_cb cb);

int uv_fs_utime(uv_loop_t* loop, uv_fs_t* req, const char* path, double atime,
    double mtime, uv_fs_cb cb);

int uv_fs_futime(uv_loop_t* loop, uv_fs_t* req, uv_file file, double atime,
    double mtime, uv_fs_cb cb);

int uv_fs_lstat(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb);

int uv_fs_link(uv_loop_t* loop, uv_fs_t* req, const char* path,
    const char* new_path, uv_fs_cb cb);

/* 
 * This flag can be used with uv_fs_symlink on Windows
 * to specify whether path argument points to a directory.
 */
#define UV_FS_SYMLINK_DIR          0x0001

int uv_fs_symlink(uv_loop_t* loop, uv_fs_t* req, const char* path,
    const char* new_path, int flags, uv_fs_cb cb);

int uv_fs_readlink(uv_loop_t* loop, uv_fs_t* req, const char* path,
    uv_fs_cb cb);

int uv_fs_fchmod(uv_loop_t* loop, uv_fs_t* req, uv_file file, int mode,
    uv_fs_cb cb);

int uv_fs_chown(uv_loop_t* loop, uv_fs_t* req, const char* path, int uid,
    int gid, uv_fs_cb cb);

int uv_fs_fchown(uv_loop_t* loop, uv_fs_t* req, uv_file file, int uid,
    int gid, uv_fs_cb cb);


enum uv_fs_event {
  UV_RENAME = 1,
  UV_CHANGE = 2
};


struct uv_fs_event_s {
  UV_HANDLE_FIELDS
  char* filename;
  UV_FS_EVENT_PRIVATE_FIELDS
};


/*
* If filename is a directory then we will watch for all events in that
* directory. If filename is a file - we will only get events from that
* file. Subdirectories are not watched.
*/
int uv_fs_event_init(uv_loop_t* loop, uv_fs_event_t* handle,
    const char* filename, uv_fs_event_cb cb);

/* Utility */

/* Convert string ip addresses to binary structures */
struct sockaddr_in uv_ip4_addr(const char* ip, int port);
struct sockaddr_in6 uv_ip6_addr(const char* ip, int port);

/* Convert binary addresses to strings */
int uv_ip4_name(struct sockaddr_in* src, char* dst, size_t size);
int uv_ip6_name(struct sockaddr_in6* src, char* dst, size_t size);

/* Gets the executable path */
int uv_exepath(char* buffer, size_t* size);

/*
 * Returns the current high-resolution real time. This is expressed in
 * nanoseconds. It is relative to an arbitrary time in the past. It is not
 * related to the time of day and therefore not subject to clock drift. The
 * primary use is for measuring performance between intervals.
 *
 * Note not every platform can support nanosecond resolution; however, this
 * value will always be in nanoseconds.
 */
extern uint64_t uv_hrtime(void);


/* the presence of these unions force similar struct layout */
union uv_any_handle {
  uv_tcp_t tcp;
  uv_pipe_t pipe;
  uv_prepare_t prepare;
  uv_check_t check;
  uv_idle_t idle;
  uv_async_t async;
  uv_timer_t timer;
  uv_getaddrinfo_t getaddrinfo;
  uv_fs_event_t fs_event;
};

union uv_any_req {
  uv_req_t req;
  uv_write_t write;
  uv_connect_t connect;
  uv_shutdown_t shutdown;
  uv_fs_t fs_req;
  uv_work_t work_req;
};


struct uv_counters_s {
  uint64_t eio_init;
  uint64_t req_init;
  uint64_t handle_init;
  uint64_t stream_init;
  uint64_t tcp_init;
  uint64_t udp_init;
  uint64_t pipe_init;
  uint64_t tty_init;
  uint64_t prepare_init;
  uint64_t check_init;
  uint64_t idle_init;
  uint64_t async_init;
  uint64_t timer_init;
  uint64_t process_init;
  uint64_t fs_event_init;
};


struct uv_loop_s {
  UV_LOOP_PRIVATE_FIELDS
  /* list used for ares task handles */
  uv_ares_task_t* uv_ares_handles_;
  /* Various thing for libeio. */
  uv_async_t uv_eio_want_poll_notifier;
  uv_async_t uv_eio_done_poll_notifier;
  uv_idle_t uv_eio_poller;
  /* Diagnostic counters */
  uv_counters_t counters;
  /* The last error */
  uv_err_t last_err;
  /* User data - use this for whatever. */
  void* data;
};


/* Don't export the private CPP symbols. */
#undef UV_REQ_TYPE_PRIVATE
#undef UV_REQ_PRIVATE_FIELDS
#undef UV_STREAM_PRIVATE_FIELDS
#undef UV_TCP_PRIVATE_FIELDS
#undef UV_PREPARE_PRIVATE_FIELDS
#undef UV_CHECK_PRIVATE_FIELDS
#undef UV_IDLE_PRIVATE_FIELDS
#undef UV_ASYNC_PRIVATE_FIELDS
#undef UV_TIMER_PRIVATE_FIELDS
#undef UV_GETADDRINFO_PRIVATE_FIELDS
#undef UV_FS_REQ_PRIVATE_FIELDS
#undef UV_WORK_PRIVATE_FIELDS
#undef UV_FS_EVENT_PRIVATE_FIELDS

#ifdef __cplusplus
}
#endif
#endif /* UV_H */
