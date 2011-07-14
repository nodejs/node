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

typedef struct uv_err_s uv_err_t;
typedef struct uv_handle_s uv_handle_t;
typedef struct uv_stream_s uv_stream_t;
typedef struct uv_tcp_s uv_tcp_t;
typedef struct uv_pipe_s uv_pipe_t;
typedef struct uv_timer_s uv_timer_t;
typedef struct uv_prepare_s uv_prepare_t;
typedef struct uv_check_s uv_check_t;
typedef struct uv_idle_s uv_idle_t;
typedef struct uv_req_s uv_req_t;
typedef struct uv_async_s uv_async_t;
typedef struct uv_getaddrinfo_s uv_getaddrinfo_t;


#if defined(__unix__) || defined(__POSIX__) || defined(__APPLE__)
# include "uv-unix.h"
#else
# include "uv-win.h"
#endif


/* The status parameter is 0 if the request completed successfully,
 * and should be -1 if the request was cancelled or failed.
 * For uv_close_cb, -1 means that the handle was closed due to an error.
 * Error details can be obtained by calling uv_last_error().
 *
 * In the case of uv_read_cb the uv_buf_t returned should be freed by the
 * user.
 */
typedef uv_buf_t (*uv_alloc_cb)(uv_stream_t* tcp, size_t suggested_size);
typedef void (*uv_read_cb)(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf);
typedef void (*uv_write_cb)(uv_req_t* req, int status);
typedef void (*uv_connect_cb)(uv_req_t* req, int status);
typedef void (*uv_shutdown_cb)(uv_req_t* req, int status);
typedef void (*uv_connection_cb)(uv_handle_t* server, int status);
typedef void (*uv_close_cb)(uv_handle_t* handle);
typedef void (*uv_timer_cb)(uv_timer_t* handle, int status);
/* TODO: do these really need a status argument? */
typedef void (*uv_async_cb)(uv_async_t* handle, int status);
typedef void (*uv_prepare_cb)(uv_prepare_t* handle, int status);
typedef void (*uv_check_cb)(uv_check_t* handle, int status);
typedef void (*uv_idle_cb)(uv_idle_t* handle, int status);
typedef void (*uv_getaddrinfo_cb)(uv_getaddrinfo_t* handle, int status, struct addrinfo* res);


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
  UV_EPROTO,
  UV_EPROTONOSUPPORT,
  UV_EPROTOTYPE,
  UV_ETIMEDOUT,
  UV_ECHARSET,
  UV_EAIFAMNOSUPPORT,
  UV_EAINONAME,
  UV_EAISERVICE,
  UV_EAISOCKTYPE,
  UV_ESHUTDOWN
} uv_err_code;

typedef enum {
  UV_UNKNOWN_HANDLE = 0,
  UV_TCP,
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
  UV_GETADDRINFO
} uv_handle_type;

typedef enum {
  UV_UNKNOWN_REQ = 0,
  UV_CONNECT,
  UV_ACCEPT,
  UV_READ,
  UV_WRITE,
  UV_SHUTDOWN,
  UV_WAKEUP,
  /* TODO: remove the req suffix */
  UV_ARES_EVENT_REQ,
  UV_ARES_CLEANUP_REQ,
  UV_GETADDRINFO_REQ
} uv_req_type;


struct uv_err_s {
  /* read-only */
  uv_err_code code;
  /* private */
  int sys_errno_;
};


struct uv_req_s {
  /* read-only */
  uv_req_type type;
  /* public */
  uv_handle_t* handle;
  void *(*cb)(void *);
  void* data;
  /* private */
  UV_REQ_PRIVATE_FIELDS
};

/*
 * Initialize a request for use with uv_write, uv_shutdown, or uv_connect.
 */
void uv_req_init(uv_req_t* req, uv_handle_t* handle, void *(*cb)(void *));

int uv_shutdown(uv_req_t* req);


#define UV_HANDLE_FIELDS \
  /* read-only */ \
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
 */
int uv_close(uv_handle_t* handle, uv_close_cb close_cb);


#define UV_STREAM_FIELDS \
  /* number of bytes queued for writing */ \
  size_t write_queue_size; \
  /* private */ \
  UV_STREAM_PRIVATE_FIELDS

/* The abstract base class for all streams. */
struct uv_stream_s {
  UV_HANDLE_FIELDS
  UV_STREAM_FIELDS
};

/* This call is used in conjunction with uv_listen() to accept incoming
 * connections. Call uv_accept after receiving a uv_connection_cb to accept
 * the connection. Before calling uv_accept use uv_*_init() must be
 * called on the client. Non-zero return value indicates an error.
 *
 * When the uv_connection_cb is called it is guaranteed that uv_accept will
 * complete successfully the first time. If you attempt to use it more than
 * once, it may fail. It is suggested to only call uv_accept once per
 * uv_connection_cb call.
 */
int uv_accept(uv_handle_t* server, uv_stream_t* client);

/* Read data from an incoming stream. The callback will be made several
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

/* Write data to stream. Buffers are written in order. Example:
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
 *   uv_write(req, a, 2);
 *   uv_write(req, b, 2);
 *
 */
int uv_write(uv_req_t* req, uv_buf_t bufs[], int bufcnt);


/*
 * A subclass of uv_stream_t representing a TCP stream or TCP server. In the
 * future this will probably be split into two classes - one a stream and
 * the other a server.
 */
struct uv_tcp_s {
  UV_HANDLE_FIELDS
  UV_STREAM_FIELDS
  UV_TCP_PRIVATE_FIELDS
};

int uv_tcp_init(uv_tcp_t* handle);

int uv_tcp_bind(uv_tcp_t* handle, struct sockaddr_in);
int uv_tcp_bind6(uv_tcp_t* handle, struct sockaddr_in6);

int uv_tcp_connect(uv_req_t* req, struct sockaddr_in);
int uv_tcp_connect6(uv_req_t* req, struct sockaddr_in6);

int uv_tcp_listen(uv_tcp_t* handle, int backlog, uv_connection_cb cb);

int uv_getsockname(uv_tcp_t* handle, struct sockaddr* name, int* namelen);


/*
 * A subclass of uv_stream_t representing a pipe stream or pipe server.
 */
struct uv_pipe_s { 
  UV_HANDLE_FIELDS 
  UV_STREAM_FIELDS 
  UV_PIPE_PRIVATE_FIELDS 
};

int uv_pipe_init(uv_pipe_t* handle);

int uv_pipe_bind(uv_pipe_t* handle, const char* name);

int uv_pipe_listen(uv_pipe_t* handle, uv_connection_cb cb);

int uv_pipe_connect(uv_req_t* req, const char* name);


/*
 * Subclass of uv_handle_t. libev wrapper. Every active prepare handle gets
 * its callback called exactly once per loop iteration, just before the
 * system blocks to wait for completed i/o.
 */
struct uv_prepare_s {
  UV_HANDLE_FIELDS
  UV_PREPARE_PRIVATE_FIELDS
};

int uv_prepare_init(uv_prepare_t* prepare);

int uv_prepare_start(uv_prepare_t* prepare, uv_prepare_cb cb);

int uv_prepare_stop(uv_prepare_t* prepare);


/*
 * Subclass of uv_handle_t. libev wrapper. Every active check handle gets
 * its callback called exactly once per loop iteration, just after the
 * system returns from blocking.
 */
struct uv_check_s {
  UV_HANDLE_FIELDS
  UV_CHECK_PRIVATE_FIELDS
};

int uv_check_init(uv_check_t* check);

int uv_check_start(uv_check_t* check, uv_check_cb cb);

int uv_check_stop(uv_check_t* check);


/*
 * Subclass of uv_handle_t. libev wrapper. Every active idle handle gets its
 * callback called repeatedly until it is stopped. This happens after all
 * other types of callbacks are processed.  When there are multiple "idle"
 * handles active, their callbacks are called in turn.
 */
struct uv_idle_s {
  UV_HANDLE_FIELDS
  UV_IDLE_PRIVATE_FIELDS
};

int uv_idle_init(uv_idle_t* idle);

int uv_idle_start(uv_idle_t* idle, uv_idle_cb cb);

int uv_idle_stop(uv_idle_t* idle);


/*
 * Subclass of uv_handle_t. libev wrapper. uv_async_send wakes up the event
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

int uv_async_init(uv_async_t* async, uv_async_cb async_cb);

int uv_async_send(uv_async_t* async);


/*
 * Subclass of uv_handle_t. Wraps libev's ev_timer watcher. Used to get
 * woken up at a specified time in the future.
 */
struct uv_timer_s {
  UV_HANDLE_FIELDS
  UV_TIMER_PRIVATE_FIELDS
};

int uv_timer_init(uv_timer_t* timer);

int uv_timer_start(uv_timer_t* timer, uv_timer_cb cb, int64_t timeout, int64_t repeat);

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
int uv_ares_init_options(ares_channel *channelptr,
                        struct ares_options *options,
                        int optmask);

void uv_ares_destroy(ares_channel channel);


/*
 * Subclass of uv_handle_t. Used for integration of getaddrinfo.
 */
struct uv_getaddrinfo_s {
  UV_HANDLE_FIELDS
  UV_GETADDRINFO_PRIVATE_FIELDS
};


/* uv_getaddrinfo
 * return code of UV_OK means that request is accepted,
 * and callback will be called with result.
 * Other return codes mean that there will not be a callback.
 * Input arguments may be released after return from this call.
 * Callback must not call freeaddrinfo
 */
 int uv_getaddrinfo(uv_getaddrinfo_t* handle,
                    uv_getaddrinfo_cb getaddrinfo_cb,
                    const char* node,
                    const char* service,
                    const struct addrinfo* hints);


/*
 * Most functions return boolean: 0 for success and -1 for failure.
 * On error the user should then call uv_last_error() to determine
 * the error code.
 */
uv_err_t uv_last_error();
char* uv_strerror(uv_err_t err);
const char* uv_err_name(uv_err_t err);

void uv_init();
int uv_run();

/*
 * Manually modify the event loop's reference count. Useful if the user wants
 * to have a handle or timeout that doesn't keep the loop alive.
 */
void uv_ref();
void uv_unref();

void uv_update_time();
int64_t uv_now();


/* Utility */
struct sockaddr_in uv_ip4_addr(const char* ip, int port);
struct sockaddr_in6 uv_ip6_addr(const char* ip, int port);

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


/* the presence of this union forces similar struct layout */
union uv_any_handle {
  uv_tcp_t tcp;
  uv_pipe_t pipe;
  uv_prepare_t prepare;
  uv_check_t check;
  uv_idle_t idle;
  uv_async_t async;
  uv_timer_t timer;
  uv_getaddrinfo_t getaddrinfo;
};

/* Diagnostic counters */
typedef struct {
  uint64_t req_init;
  uint64_t handle_init;
  uint64_t stream_init;
  uint64_t tcp_init;
  uint64_t pipe_init;
  uint64_t prepare_init;
  uint64_t check_init;
  uint64_t idle_init;
  uint64_t async_init;
  uint64_t timer_init;
} uv_counters_t;

uv_counters_t* uv_counters();


/* Don't export the private CPP symbols. */
#undef UV_REQ_PRIVATE_FIELDS
#undef UV_STREAM_PRIVATE_FIELDS
#undef UV_TCP_PRIVATE_FIELDS
#undef UV_PREPARE_PRIVATE_FIELDS
#undef UV_CHECK_PRIVATE_FIELDS
#undef UV_IDLE_PRIVATE_FIELDS
#undef UV_ASYNC_PRIVATE_FIELDS
#undef UV_TIMER_PRIVATE_FIELDS
#undef UV_GETADDRINFO_PRIVATE_FIELDS

#ifdef __cplusplus
}
#endif
#endif /* UV_H */
