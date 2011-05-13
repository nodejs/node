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
 * In the case of uv_read_cb the uv_buf returned should be freed by the
 * user.
 */
typedef uv_buf (*uv_alloc_cb)(uv_handle_t* handle, size_t suggested_size);
typedef void (*uv_read_cb)(uv_handle_t *handle, int nread, uv_buf buf);
typedef void (*uv_write_cb)(uv_req_t* req, int status);
typedef void (*uv_connect_cb)(uv_req_t* req, int status);
typedef void (*uv_shutdown_cb)(uv_req_t* req, int status);
typedef void (*uv_accept_cb)(uv_handle_t* handle);
typedef void (*uv_close_cb)(uv_handle_t* handle, int status);
typedef void (*uv_timer_cb)(uv_req_t* req, int64_t skew, int status);
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
  UV_TIMEOUT,
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
  uv_req_private_fields
};


struct uv_handle_s {
  /* read-only */
  uv_handle_type type;
  /* public */
  uv_close_cb close_cb;
  void* data;
  /* number of bytes queued for writing */
  size_t write_queue_size;
  /* private */
  uv_handle_private_fields
};


/* Most functions return boolean: 0 for success and -1 for failure.
 * On error the user should then call uv_last_error() to determine
 * the error code.
 */
uv_err_t uv_last_error();
char* uv_strerror(uv_err_t err);


void uv_init(uv_alloc_cb alloc);
int uv_run();

/* Manually modify the event loop's reference count. Useful if the user wants
 * to have a handle or timeout that doesn't keep the loop alive.
 */
void uv_ref();
void uv_unref();

void uv_update_time();
int64_t uv_now();

void uv_req_init(uv_req_t* req, uv_handle_t* handle, void* cb);

/*
 * TODO:
 * - uv_(pipe|pipe_tty)_handle_init
 * - uv_bind_pipe(char* name)
 * - uv_continuous_read(uv_handle_t* handle, uv_continuous_read_cb* cb)
 * - A way to list cancelled uv_reqs after before/on uv_close_cb
 */

/* TCP socket methods.
 * Handle and callback bust be set by calling uv_req_init.
 */
int uv_tcp_init(uv_handle_t* handle, uv_close_cb close_cb, void* data);
int uv_bind(uv_handle_t* handle, struct sockaddr* addr);

int uv_connect(uv_req_t* req, struct sockaddr* addr);
int uv_shutdown(uv_req_t* req);

/* TCP server methods. */
int uv_listen(uv_handle_t* handle, int backlog, uv_accept_cb cb);

/* Call this after accept_cb. client does not need to be initialized. */
int uv_accept(uv_handle_t* server, uv_handle_t* client,
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
int uv_read_start(uv_handle_t* handle, uv_read_cb cb);
int uv_read_stop(uv_handle_t* handle);

int uv_write(uv_req_t* req, uv_buf bufs[], int bufcnt);

/* Timer methods */
int uv_timeout(uv_req_t* req, int64_t timeout);

/* libev wrapper. Every active prepare handle gets its callback called
 * exactly once per loop iteration, just before the system blocks to wait
 * for completed i/o.
 */
int uv_prepare_init(uv_handle_t* handle, uv_close_cb close_cb, void* data);
int uv_prepare_start(uv_handle_t* handle, uv_loop_cb cb);
int uv_prepare_stop(uv_handle_t* handle);

/* libev wrapper. Every active check handle gets its callback called exactly
 * once per loop iteration, just after the system returns from blocking.
 */
int uv_check_init(uv_handle_t* handle, uv_close_cb close_cb, void* data);
int uv_check_start(uv_handle_t* handle, uv_loop_cb cb);
int uv_check_stop(uv_handle_t* handle);

/* libev wrapper. Every active idle handle gets its callback called repeatedly until it is
 * stopped. This happens after all other types of callbacks are processed.
 * When there are multiple "idle" handles active, their callbacks are called
 * in turn.
 */
int uv_idle_init(uv_handle_t* handle, uv_close_cb close_cb, void* data);
int uv_idle_start(uv_handle_t* handle, uv_loop_cb cb);
int uv_idle_stop(uv_handle_t* handle);

/* libev wrapper. uv_async_send wakes up the event loop and calls the async
 * handle's callback There is no guarantee that every uv_async_send call
 * leads to exactly one invocation of the callback; The only guarantee is
 * that the callback function is  called at least once after the call to
 * async_send. Unlike everything else, uv_async_send can be called from
 * another thread.
 *
 * QUESTION(ryan) Can UV_ASYNC just use uv_loop_cb? Same signature on my
 * side.
 */
int uv_async_init(uv_handle_t* handle, uv_async_cb async_cb,
                   uv_close_cb close_cb, void* data);
int uv_async_send(uv_handle_t* handle);

/* Request handle to be closed. close_cb will be called
 * asynchronously after this call.
 */
int uv_close(uv_handle_t* handle);


/* Utility */
struct sockaddr_in uv_ip4_addr(char* ip, int port);

#ifdef __cplusplus
}
#endif
#endif /* UV_H */
