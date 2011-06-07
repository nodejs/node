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

#include <stdint.h> /* int64_t */
#include <sys/types.h> /* size_t */

typedef struct uv_err_s uv_err_t;
typedef struct uv_handle_s uv_handle_t;
typedef struct uv_tcp_s uv_tcp_t;
typedef struct uv_timer_s uv_timer_t;
typedef struct uv_prepare_s uv_prepare_t;
typedef struct uv_check_s uv_check_t;
typedef struct uv_idle_s uv_idle_t;
typedef struct uv_req_s uv_req_t;


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
typedef uv_buf_t (*uv_alloc_cb)(uv_tcp_t* tcp, size_t suggested_size);
typedef void (*uv_read_cb)(uv_tcp_t* tcp, int nread, uv_buf_t buf);
typedef void (*uv_write_cb)(uv_req_t* req, int status);
typedef void (*uv_connect_cb)(uv_req_t* req, int status);
typedef void (*uv_shutdown_cb)(uv_req_t* req, int status);
typedef void (*uv_connection_cb)(uv_tcp_t* server, int status);
typedef void (*uv_close_cb)(uv_handle_t* handle, int status);
/* TODO: do loop_cb and async_cb really need a status argument? */
typedef void (*uv_loop_cb)(uv_handle_t* handle, int status);
typedef void (*uv_async_cb)(uv_handle_t* handle, int stats);


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
  UV_ETIMEDOUT
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
  UV_ASYNC
} uv_handle_type;

typedef enum {
  UV_UNKNOWN_REQ = 0,
  UV_CONNECT,
  UV_ACCEPT,
  UV_READ,
  UV_WRITE,
  UV_SHUTDOWN,
  UV_WAKEUP
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
  void* cb;
  void* data;
  /* private */
  UV_REQ_PRIVATE_FIELDS
};

/*
 * Initialize a request for use with uv_write, uv_shutdown, or uv_connect.
 */
void uv_req_init(uv_req_t* req, uv_handle_t* handle, void* cb);


#define UV_HANDLE_FIELDS \
  /* read-only */ \
  uv_handle_type type; \
  /* public */ \
  uv_close_cb close_cb; \
  void* data; \
  /* private */ \
  UV_HANDLE_PRIVATE_FIELDS \

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
int uv_close(uv_handle_t* handle);


/*
 * A subclass of uv_handle_t representing a TCP stream or TCP server. In the
 * future this will probably be split into two classes - one a stream and
 * the other a server.
 */
struct uv_tcp_s {
  UV_HANDLE_FIELDS
  size_t write_queue_size; /* number of bytes queued for writing */
  UV_TCP_PRIVATE_FIELDS
};

int uv_tcp_init(uv_tcp_t* handle, uv_close_cb close_cb, void* data);

int uv_bind(uv_tcp_t* handle, struct sockaddr_in);

int uv_connect(uv_req_t* req, struct sockaddr_in);

int uv_shutdown(uv_req_t* req);

int uv_listen(uv_tcp_t* handle, int backlog, uv_connection_cb cb);

/* Call this after connection_cb. client does not need to be initialized. */
int uv_accept(uv_tcp_t* server, uv_tcp_t* client,
    uv_close_cb close_cb, void* data);

/* Read data from an incoming stream. The callback will be made several
 * several times until there is no more data to read or uv_read_stop is
 * called. When we've reached EOF nread will be set to -1 and the error is
 * set to UV_EOF. When nread == -1 the buf parameter might not point to a
 * valid buffer; in that case buf.len and buf.base are both set to 0.
 * Note that nread might also be 0, which does *not* indicate an error or
 * eof; it happens when libuv requested a buffer through the alloc callback
 * but then decided that it didn't need that buffer.
 */
int uv_read_start(uv_tcp_t*, uv_alloc_cb alloc_cb, uv_read_cb read_cb);

int uv_read_stop(uv_tcp_t*);

int uv_write(uv_req_t* req, uv_buf_t bufs[], int bufcnt);


/*
 * Subclass of uv_handle_t. libev wrapper. Every active prepare handle gets
 * its callback called exactly once per loop iteration, just before the
 * system blocks to wait for completed i/o.
 */
struct uv_prepare_s {
  UV_HANDLE_FIELDS
  UV_PREPARE_PRIVATE_FIELDS
};

int uv_prepare_init(uv_prepare_t* prepare, uv_close_cb close_cb, void* data);

int uv_prepare_start(uv_prepare_t* prepare, uv_loop_cb cb);

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

int uv_check_init(uv_check_t* check, uv_close_cb close_cb, void* data);

int uv_check_start(uv_check_t* check, uv_loop_cb cb);

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

int uv_idle_init(uv_idle_t* idle, uv_close_cb close_cb, void* data);

int uv_idle_start(uv_idle_t* idle, uv_loop_cb cb);

int uv_idle_stop(uv_idle_t* idle);


/*
 * Subclass of uv_handle_t. libev wrapper. uv_async_send wakes up the event
 * loop and calls the async handle's callback There is no guarantee that
 * every uv_async_send call leads to exactly one invocation of the callback;
 * The only guarantee is that the callback function is  called at least once
 * after the call to async_send. Unlike all other libuv functions,
 * uv_async_send can be called from another thread.
 */
typedef struct {
  UV_HANDLE_FIELDS
  UV_ASYNC_PRIVATE_FIELDS
} uv_async_t;

int uv_async_init(uv_async_t* async, uv_async_cb async_cb,
    uv_close_cb close_cb, void* data);

int uv_async_send(uv_async_t* async);


/*
 * Subclass of uv_handle_t. Wraps libev's ev_timer watcher. Used to get
 * woken up at a specified time in the future.
 */
struct uv_timer_s {
  UV_HANDLE_FIELDS
  UV_TIMER_PRIVATE_FIELDS
};

int uv_timer_init(uv_timer_t* timer, uv_close_cb close_cb, void* data);

int uv_timer_start(uv_timer_t* timer, uv_loop_cb cb, int64_t timeout, int64_t repeat);

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
struct sockaddr_in uv_ip4_addr(char* ip, int port);

/* Gets the executable path */
int uv_get_exepath(char* buffer, size_t* size);


/* the presence of this union forces similar struct layout */
union uv_any_handle {
  uv_tcp_t tcp;
  uv_prepare_t prepare;
  uv_check_t check;
  uv_idle_t idle;
  uv_async_t async;
  uv_timer_t timer;
};

#ifdef __cplusplus
}
#endif
#endif /* UV_H */
