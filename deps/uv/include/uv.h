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

/* See http://nikhilm.github.com/uvbook/ for an introduction. */

#ifndef UV_H
#define UV_H
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
  /* Windows - set up dll import/export decorators. */
# if defined(BUILDING_UV_SHARED)
    /* Building shared library. */
#   define UV_EXTERN __declspec(dllexport)
# elif defined(USING_UV_SHARED)
    /* Using shared library. */
#   define UV_EXTERN __declspec(dllimport)
# else
    /* Building static library. */
#   define UV_EXTERN /* nothing */
# endif
#elif __GNUC__ >= 4
# define UV_EXTERN __attribute__((visibility("default")))
#else
# define UV_EXTERN /* nothing */
#endif


#define UV_VERSION_MAJOR 0
#define UV_VERSION_MINOR 10


#if defined(_MSC_VER) && _MSC_VER < 1600
# include "uv-private/stdint-msvc2008.h"
#else
# include <stdint.h>
#endif

#include <sys/types.h> /* size_t */

#if defined(__SVR4) && !defined(__unix__)
# define __unix__
#endif

#if defined(__unix__) || defined(__POSIX__) || \
    defined(__APPLE__) || defined(_AIX)
# include "uv-private/uv-unix.h"
#else
# include "uv-private/uv-win.h"
#endif

/* Expand this list if necessary. */
#define UV_ERRNO_MAP(XX)                                                      \
  XX( -1, UNKNOWN, "unknown error")                                           \
  XX(  0, OK, "success")                                                      \
  XX(  1, EOF, "end of file")                                                 \
  XX(  2, EADDRINFO, "getaddrinfo error")                                     \
  XX(  3, EACCES, "permission denied")                                        \
  XX(  4, EAGAIN, "resource temporarily unavailable")                         \
  XX(  5, EADDRINUSE, "address already in use")                               \
  XX(  6, EADDRNOTAVAIL, "address not available")                             \
  XX(  7, EAFNOSUPPORT, "address family not supported")                       \
  XX(  8, EALREADY, "connection already in progress")                         \
  XX(  9, EBADF, "bad file descriptor")                                       \
  XX( 10, EBUSY, "resource busy or locked")                                   \
  XX( 11, ECONNABORTED, "software caused connection abort")                   \
  XX( 12, ECONNREFUSED, "connection refused")                                 \
  XX( 13, ECONNRESET, "connection reset by peer")                             \
  XX( 14, EDESTADDRREQ, "destination address required")                       \
  XX( 15, EFAULT, "bad address in system call argument")                      \
  XX( 16, EHOSTUNREACH, "host is unreachable")                                \
  XX( 17, EINTR, "interrupted system call")                                   \
  XX( 18, EINVAL, "invalid argument")                                         \
  XX( 19, EISCONN, "socket is already connected")                             \
  XX( 20, EMFILE, "too many open files")                                      \
  XX( 21, EMSGSIZE, "message too long")                                       \
  XX( 22, ENETDOWN, "network is down")                                        \
  XX( 23, ENETUNREACH, "network is unreachable")                              \
  XX( 24, ENFILE, "file table overflow")                                      \
  XX( 25, ENOBUFS, "no buffer space available")                               \
  XX( 26, ENOMEM, "not enough memory")                                        \
  XX( 27, ENOTDIR, "not a directory")                                         \
  XX( 28, EISDIR, "illegal operation on a directory")                         \
  XX( 29, ENONET, "machine is not on the network")                            \
  XX( 31, ENOTCONN, "socket is not connected")                                \
  XX( 32, ENOTSOCK, "socket operation on non-socket")                         \
  XX( 33, ENOTSUP, "operation not supported on socket")                       \
  XX( 34, ENOENT, "no such file or directory")                                \
  XX( 35, ENOSYS, "function not implemented")                                 \
  XX( 36, EPIPE, "broken pipe")                                               \
  XX( 37, EPROTO, "protocol error")                                           \
  XX( 38, EPROTONOSUPPORT, "protocol not supported")                          \
  XX( 39, EPROTOTYPE, "protocol wrong type for socket")                       \
  XX( 40, ETIMEDOUT, "connection timed out")                                  \
  XX( 41, ECHARSET, "invalid Unicode character")                              \
  XX( 42, EAIFAMNOSUPPORT, "address family for hostname not supported")       \
  XX( 44, EAISERVICE, "servname not supported for ai_socktype")               \
  XX( 45, EAISOCKTYPE, "ai_socktype not supported")                           \
  XX( 46, ESHUTDOWN, "cannot send after transport endpoint shutdown")         \
  XX( 47, EEXIST, "file already exists")                                      \
  XX( 48, ESRCH, "no such process")                                           \
  XX( 49, ENAMETOOLONG, "name too long")                                      \
  XX( 50, EPERM, "operation not permitted")                                   \
  XX( 51, ELOOP, "too many symbolic links encountered")                       \
  XX( 52, EXDEV, "cross-device link not permitted")                           \
  XX( 53, ENOTEMPTY, "directory not empty")                                   \
  XX( 54, ENOSPC, "no space left on device")                                  \
  XX( 55, EIO, "i/o error")                                                   \
  XX( 56, EROFS, "read-only file system")                                     \
  XX( 57, ENODEV, "no such device")                                           \
  XX( 58, ESPIPE, "invalid seek")                                             \
  XX( 59, ECANCELED, "operation canceled")                                    \
  XX( 60, EFBIG, "file too large")                                            \
  XX( 61, ENOPROTOOPT, "protocol not available")                              \
  XX( 62, ETXTBSY, "text file is busy")                                       \
  XX( 63, ERANGE, "result too large")                                         \
  XX( 64, ENXIO, "no such device or address")                                 \
  XX( 65, EMLINK, "too many links")                                           \


#define UV_ERRNO_GEN(val, name, s) UV_##name = val,
typedef enum {
  UV_ERRNO_MAP(UV_ERRNO_GEN)
  UV_MAX_ERRORS
} uv_err_code;
#undef UV_ERRNO_GEN

#define UV_HANDLE_TYPE_MAP(XX)                                                \
  XX(ASYNC, async)                                                            \
  XX(CHECK, check)                                                            \
  XX(FS_EVENT, fs_event)                                                      \
  XX(FS_POLL, fs_poll)                                                        \
  XX(HANDLE, handle)                                                          \
  XX(IDLE, idle)                                                              \
  XX(NAMED_PIPE, pipe)                                                        \
  XX(POLL, poll)                                                              \
  XX(PREPARE, prepare)                                                        \
  XX(PROCESS, process)                                                        \
  XX(STREAM, stream)                                                          \
  XX(TCP, tcp)                                                                \
  XX(TIMER, timer)                                                            \
  XX(TTY, tty)                                                                \
  XX(UDP, udp)                                                                \
  XX(SIGNAL, signal)                                                          \

#define UV_REQ_TYPE_MAP(XX)                                                   \
  XX(REQ, req)                                                                \
  XX(CONNECT, connect)                                                        \
  XX(WRITE, write)                                                            \
  XX(SHUTDOWN, shutdown)                                                      \
  XX(UDP_SEND, udp_send)                                                      \
  XX(FS, fs)                                                                  \
  XX(WORK, work)                                                              \
  XX(GETADDRINFO, getaddrinfo)                                                \

typedef enum {
  UV_UNKNOWN_HANDLE = 0,
#define XX(uc, lc) UV_##uc,
  UV_HANDLE_TYPE_MAP(XX)
#undef XX
  UV_FILE,
  UV_HANDLE_TYPE_MAX
} uv_handle_type;

typedef enum {
  UV_UNKNOWN_REQ = 0,
#define XX(uc, lc) UV_##uc,
  UV_REQ_TYPE_MAP(XX)
#undef XX
  UV_REQ_TYPE_PRIVATE
  UV_REQ_TYPE_MAX
} uv_req_type;


/* Handle types. */
typedef struct uv_loop_s uv_loop_t;
typedef struct uv_err_s uv_err_t;
typedef struct uv_handle_s uv_handle_t;
typedef struct uv_stream_s uv_stream_t;
typedef struct uv_tcp_s uv_tcp_t;
typedef struct uv_udp_s uv_udp_t;
typedef struct uv_pipe_s uv_pipe_t;
typedef struct uv_tty_s uv_tty_t;
typedef struct uv_poll_s uv_poll_t;
typedef struct uv_timer_s uv_timer_t;
typedef struct uv_prepare_s uv_prepare_t;
typedef struct uv_check_s uv_check_t;
typedef struct uv_idle_s uv_idle_t;
typedef struct uv_async_s uv_async_t;
typedef struct uv_process_s uv_process_t;
typedef struct uv_fs_event_s uv_fs_event_t;
typedef struct uv_fs_poll_s uv_fs_poll_t;
typedef struct uv_signal_s uv_signal_t;

/* Request types. */
typedef struct uv_req_s uv_req_t;
typedef struct uv_getaddrinfo_s uv_getaddrinfo_t;
typedef struct uv_shutdown_s uv_shutdown_t;
typedef struct uv_write_s uv_write_t;
typedef struct uv_connect_s uv_connect_t;
typedef struct uv_udp_send_s uv_udp_send_t;
typedef struct uv_fs_s uv_fs_t;
typedef struct uv_work_s uv_work_t;

/* None of the above. */
typedef struct uv_cpu_info_s uv_cpu_info_t;
typedef struct uv_interface_address_s uv_interface_address_t;


typedef enum {
  UV_RUN_DEFAULT = 0,
  UV_RUN_ONCE,
  UV_RUN_NOWAIT
} uv_run_mode;


/*
 * Returns the libuv version packed into a single integer. 8 bits are used for
 * each component, with the patch number stored in the 8 least significant
 * bits. E.g. for libuv 1.2.3 this would return 0x010203.
 */
UV_EXTERN unsigned int uv_version(void);

/*
 * Returns the libuv version number as a string. For non-release versions
 * "-pre" is appended, so the version number could be "1.2.3-pre".
 */
UV_EXTERN const char* uv_version_string(void);


/*
 * This function must be called before any other functions in libuv.
 *
 * All functions besides uv_run() are non-blocking.
 *
 * All callbacks in libuv are made asynchronously. That is they are never
 * made by the function that takes them as a parameter.
 */
UV_EXTERN uv_loop_t* uv_loop_new(void);
UV_EXTERN void uv_loop_delete(uv_loop_t*);

/*
 * Returns the default loop.
 */
UV_EXTERN uv_loop_t* uv_default_loop(void);

/*
 * This function runs the event loop. It will act differently depending on the
 * specified mode:
 *  - UV_RUN_DEFAULT: Runs the event loop until the reference count drops to
 *    zero. Always returns zero.
 *  - UV_RUN_ONCE: Poll for new events once. Note that this function blocks if
 *    there are no pending events. Returns zero when done (no active handles
 *    or requests left), or non-zero if more events are expected (meaning you
 *    should run the event loop again sometime in the future).
 *  - UV_RUN_NOWAIT: Poll for new events once but don't block if there are no
 *    pending events.
 */
UV_EXTERN int uv_run(uv_loop_t*, uv_run_mode mode);

/*
 * This function will stop the event loop by forcing uv_run to end
 * as soon as possible, but not sooner than the next loop iteration.
 * If this function was called before blocking for i/o, the loop won't
 * block for i/o on this iteration.
 */
UV_EXTERN void uv_stop(uv_loop_t*);

/*
 * Manually modify the event loop's reference count. Useful if the user wants
 * to have a handle or timeout that doesn't keep the loop alive.
 */
UV_EXTERN void uv_ref(uv_handle_t*);
UV_EXTERN void uv_unref(uv_handle_t*);

/*
 * Update the event loop's concept of "now". Libuv caches the current time
 * at the start of the event loop tick in order to reduce the number of
 * time-related system calls.
 *
 * You won't normally need to call this function unless you have callbacks
 * that block the event loop for longer periods of time, where "longer" is
 * somewhat subjective but probably on the order of a millisecond or more.
 */
UV_EXTERN void uv_update_time(uv_loop_t*);

/*
 * Return the current timestamp in milliseconds. The timestamp is cached at
 * the start of the event loop tick, see |uv_update_time()| for details and
 * rationale.
 *
 * The timestamp increases monotonically from some arbitrary point in time.
 * Don't make assumptions about the starting point, you will only get
 * disappointed.
 *
 * Use uv_hrtime() if you need sub-milliseond granularity.
 */
UV_EXTERN uint64_t uv_now(uv_loop_t*);

/*
 * Get backend file descriptor. Only kqueue, epoll and event ports are
 * supported.
 *
 * This can be used in conjunction with `uv_run(loop, UV_RUN_NOWAIT)` to
 * poll in one thread and run the event loop's event callbacks in another.
 *
 * Useful for embedding libuv's event loop in another event loop.
 * See test/test-embed.c for an example.
 *
 * Note that embedding a kqueue fd in another kqueue pollset doesn't work on
 * all platforms. It's not an error to add the fd but it never generates
 * events.
 */
UV_EXTERN int uv_backend_fd(const uv_loop_t*);

/*
 * Get the poll timeout. The return value is in milliseconds, or -1 for no
 * timeout.
 */
UV_EXTERN int uv_backend_timeout(const uv_loop_t*);


/*
 * Should return a buffer that libuv can use to read data into.
 *
 * `suggested_size` is a hint. Returning a buffer that is smaller is perfectly
 * okay as long as `buf.len > 0`.
 */
typedef uv_buf_t (*uv_alloc_cb)(uv_handle_t* handle, size_t suggested_size);

/*
 * `nread` is > 0 if there is data available, 0 if libuv is done reading for now
 * or -1 on error.
 *
 * Error details can be obtained by calling uv_last_error(). UV_EOF indicates
 * that the stream has been closed.
 *
 * The callee is responsible for closing the stream when an error happens.
 * Trying to read from the stream again is undefined.
 *
 * The callee is responsible for freeing the buffer, libuv does not reuse it.
 */
typedef void (*uv_read_cb)(uv_stream_t* stream, ssize_t nread, uv_buf_t buf);

/*
 * Just like the uv_read_cb except that if the pending parameter is true
 * then you can use uv_accept() to pull the new handle into the process.
 * If no handle is pending then pending will be UV_UNKNOWN_HANDLE.
 */
typedef void (*uv_read2_cb)(uv_pipe_t* pipe, ssize_t nread, uv_buf_t buf,
    uv_handle_type pending);

typedef void (*uv_write_cb)(uv_write_t* req, int status);
typedef void (*uv_connect_cb)(uv_connect_t* req, int status);
typedef void (*uv_shutdown_cb)(uv_shutdown_t* req, int status);
typedef void (*uv_connection_cb)(uv_stream_t* server, int status);
typedef void (*uv_close_cb)(uv_handle_t* handle);
typedef void (*uv_poll_cb)(uv_poll_t* handle, int status, int events);
typedef void (*uv_timer_cb)(uv_timer_t* handle, int status);
/* TODO: do these really need a status argument? */
typedef void (*uv_async_cb)(uv_async_t* handle, int status);
typedef void (*uv_prepare_cb)(uv_prepare_t* handle, int status);
typedef void (*uv_check_cb)(uv_check_t* handle, int status);
typedef void (*uv_idle_cb)(uv_idle_t* handle, int status);
typedef void (*uv_exit_cb)(uv_process_t*, int exit_status, int term_signal);
typedef void (*uv_walk_cb)(uv_handle_t* handle, void* arg);
typedef void (*uv_fs_cb)(uv_fs_t* req);
typedef void (*uv_work_cb)(uv_work_t* req);
typedef void (*uv_after_work_cb)(uv_work_t* req, int status);
typedef void (*uv_getaddrinfo_cb)(uv_getaddrinfo_t* req,
                                  int status,
                                  struct addrinfo* res);

/*
* This will be called repeatedly after the uv_fs_event_t is initialized.
* If uv_fs_event_t was initialized with a directory the filename parameter
* will be a relative path to a file contained in the directory.
* The events parameter is an ORed mask of enum uv_fs_event elements.
*/
typedef void (*uv_fs_event_cb)(uv_fs_event_t* handle, const char* filename,
    int events, int status);

typedef void (*uv_fs_poll_cb)(uv_fs_poll_t* handle,
                              int status,
                              const uv_statbuf_t* prev,
                              const uv_statbuf_t* curr);

typedef void (*uv_signal_cb)(uv_signal_t* handle, int signum);


typedef enum {
  UV_LEAVE_GROUP = 0,
  UV_JOIN_GROUP
} uv_membership;


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
UV_EXTERN uv_err_t uv_last_error(uv_loop_t*);
UV_EXTERN const char* uv_strerror(uv_err_t err);
UV_EXTERN const char* uv_err_name(uv_err_t err);


#define UV_REQ_FIELDS                                                         \
  /* public */                                                                \
  void* data;                                                                 \
  /* read-only */                                                             \
  uv_req_type type;                                                           \
  /* private */                                                               \
  ngx_queue_t active_queue;                                                   \
  UV_REQ_PRIVATE_FIELDS                                                       \

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
 * initialized stream. req should be an uninitialized shutdown request
 * struct. The cb is called after shutdown is complete.
 */
UV_EXTERN int uv_shutdown(uv_shutdown_t* req, uv_stream_t* handle,
    uv_shutdown_cb cb);

struct uv_shutdown_s {
  UV_REQ_FIELDS
  uv_stream_t* handle;
  uv_shutdown_cb cb;
  UV_SHUTDOWN_PRIVATE_FIELDS
};


#define UV_HANDLE_FIELDS                                                      \
  /* public */                                                                \
  uv_close_cb close_cb;                                                       \
  void* data;                                                                 \
  /* read-only */                                                             \
  uv_loop_t* loop;                                                            \
  uv_handle_type type;                                                        \
  /* private */                                                               \
  ngx_queue_t handle_queue;                                                   \
  UV_HANDLE_PRIVATE_FIELDS                                                    \

/* The abstract base class of all handles.  */
struct uv_handle_s {
  UV_HANDLE_FIELDS
};

/*
 * Returns size of various handle types, useful for FFI
 * bindings to allocate correct memory without copying struct
 * definitions
 */
UV_EXTERN size_t uv_handle_size(uv_handle_type type);

/*
 * Returns size of request types, useful for dynamic lookup with FFI
 */
UV_EXTERN size_t uv_req_size(uv_req_type type);

/*
 * Returns 1 if the prepare/check/idle/timer handle has been started, 0
 * otherwise. For other handle types this always returns 1.
 */
UV_EXTERN int uv_is_active(const uv_handle_t* handle);

/*
 * Walk the list of open handles.
 */
UV_EXTERN void uv_walk(uv_loop_t* loop, uv_walk_cb walk_cb, void* arg);


/*
 * Request handle to be closed. close_cb will be called asynchronously after
 * this call. This MUST be called on each handle before memory is released.
 *
 * Note that handles that wrap file descriptors are closed immediately but
 * close_cb will still be deferred to the next iteration of the event loop.
 * It gives you a chance to free up any resources associated with the handle.
 *
 * In-progress requests, like uv_connect_t or uv_write_t, are cancelled and
 * have their callbacks called asynchronously with status=-1 and the error code
 * set to UV_ECANCELED.
 */
UV_EXTERN void uv_close(uv_handle_t* handle, uv_close_cb close_cb);


/*
 * Constructor for uv_buf_t.
 * Due to platform differences the user cannot rely on the ordering of the
 * base and len members of the uv_buf_t struct. The user is responsible for
 * freeing base after the uv_buf_t is done. Return struct passed by value.
 */
UV_EXTERN uv_buf_t uv_buf_init(char* base, unsigned int len);


/*
 * Utility function. Copies up to `size` characters from `src` to `dst`
 * and ensures that `dst` is properly NUL terminated unless `size` is zero.
 */
UV_EXTERN size_t uv_strlcpy(char* dst, const char* src, size_t size);

/*
 * Utility function. Appends `src` to `dst` and ensures that `dst` is
 * properly NUL terminated unless `size` is zero or `dst` does not
 * contain a NUL byte. `size` is the total length of `dst` so at most
 * `size - strlen(dst) - 1` characters will be copied from `src`.
 */
UV_EXTERN size_t uv_strlcat(char* dst, const char* src, size_t size);


#define UV_STREAM_FIELDS                                                      \
  /* number of bytes queued for writing */                                    \
  size_t write_queue_size;                                                    \
  uv_alloc_cb alloc_cb;                                                       \
  uv_read_cb read_cb;                                                         \
  uv_read2_cb read2_cb;                                                       \
  /* private */                                                               \
  UV_STREAM_PRIVATE_FIELDS

/*
 * uv_stream_t is a subclass of uv_handle_t
 *
 * uv_stream is an abstract class.
 *
 * uv_stream_t is the parent class of uv_tcp_t, uv_pipe_t, uv_tty_t, and
 * soon uv_file_t.
 */
struct uv_stream_s {
  UV_HANDLE_FIELDS
  UV_STREAM_FIELDS
};

UV_EXTERN int uv_listen(uv_stream_t* stream, int backlog, uv_connection_cb cb);

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
UV_EXTERN int uv_accept(uv_stream_t* server, uv_stream_t* client);

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
UV_EXTERN int uv_read_start(uv_stream_t*, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb);

UV_EXTERN int uv_read_stop(uv_stream_t*);

/*
 * Extended read methods for receiving handles over a pipe. The pipe must be
 * initialized with ipc == 1.
 */
UV_EXTERN int uv_read2_start(uv_stream_t*, uv_alloc_cb alloc_cb,
    uv_read2_cb read_cb);


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
 *   uv_write_t req1;
 *   uv_write_t req2;
 *
 *   // writes "1234"
 *   uv_write(&req1, stream, a, 2);
 *   uv_write(&req2, stream, b, 2);
 *
 */
UV_EXTERN int uv_write(uv_write_t* req, uv_stream_t* handle,
    uv_buf_t bufs[], int bufcnt, uv_write_cb cb);

/*
 * Extended write function for sending handles over a pipe. The pipe must be
 * initialized with ipc == 1.
 * send_handle must be a TCP socket or pipe, which is a server or a connection
 * (listening or connected state).  Bound sockets or pipes will be assumed to
 * be servers.
 */
UV_EXTERN int uv_write2(uv_write_t* req, uv_stream_t* handle, uv_buf_t bufs[],
    int bufcnt, uv_stream_t* send_handle, uv_write_cb cb);

/* uv_write_t is a subclass of uv_req_t */
struct uv_write_s {
  UV_REQ_FIELDS
  uv_write_cb cb;
  uv_stream_t* send_handle;
  uv_stream_t* handle;
  UV_WRITE_PRIVATE_FIELDS
};


/*
 * Used to determine whether a stream is readable or writable.
 */
UV_EXTERN int uv_is_readable(const uv_stream_t* handle);
UV_EXTERN int uv_is_writable(const uv_stream_t* handle);


/*
 * Used to determine whether a stream is closing or closed.
 *
 * N.B. is only valid between the initialization of the handle
 *      and the arrival of the close callback, and cannot be used
 *      to validate the handle.
 */
UV_EXTERN int uv_is_closing(const uv_handle_t* handle);


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

UV_EXTERN int uv_tcp_init(uv_loop_t*, uv_tcp_t* handle);

/*
 * Opens an existing file descriptor or SOCKET as a tcp handle.
 */
UV_EXTERN int uv_tcp_open(uv_tcp_t* handle, uv_os_sock_t sock);

/* Enable/disable Nagle's algorithm. */
UV_EXTERN int uv_tcp_nodelay(uv_tcp_t* handle, int enable);

/*
 * Enable/disable TCP keep-alive.
 *
 * `delay` is the initial delay in seconds, ignored when `enable` is zero.
 */
UV_EXTERN int uv_tcp_keepalive(uv_tcp_t* handle,
                               int enable,
                               unsigned int delay);

/*
 * Enable/disable simultaneous asynchronous accept requests that are
 * queued by the operating system when listening for new tcp connections.
 * This setting is used to tune a tcp server for the desired performance.
 * Having simultaneous accepts can significantly improve the rate of
 * accepting connections (which is why it is enabled by default) but
 * may lead to uneven load distribution in multi-process setups.
 */
UV_EXTERN int uv_tcp_simultaneous_accepts(uv_tcp_t* handle, int enable);

UV_EXTERN int uv_tcp_bind(uv_tcp_t* handle, struct sockaddr_in);
UV_EXTERN int uv_tcp_bind6(uv_tcp_t* handle, struct sockaddr_in6);
UV_EXTERN int uv_tcp_getsockname(uv_tcp_t* handle, struct sockaddr* name,
    int* namelen);
UV_EXTERN int uv_tcp_getpeername(uv_tcp_t* handle, struct sockaddr* name,
    int* namelen);

/*
 * uv_tcp_connect, uv_tcp_connect6
 * These functions establish IPv4 and IPv6 TCP connections. Provide an
 * initialized TCP handle and an uninitialized uv_connect_t*. The callback
 * will be made when the connection is established.
 */
UV_EXTERN int uv_tcp_connect(uv_connect_t* req, uv_tcp_t* handle,
    struct sockaddr_in address, uv_connect_cb cb);
UV_EXTERN int uv_tcp_connect6(uv_connect_t* req, uv_tcp_t* handle,
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
UV_EXTERN int uv_udp_init(uv_loop_t*, uv_udp_t* handle);

/*
 * Opens an existing file descriptor or SOCKET as a udp handle.
 */
UV_EXTERN int uv_udp_open(uv_udp_t* handle, uv_os_sock_t sock);

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
UV_EXTERN int uv_udp_bind(uv_udp_t* handle, struct sockaddr_in addr,
    unsigned flags);

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
UV_EXTERN int uv_udp_bind6(uv_udp_t* handle, struct sockaddr_in6 addr,
    unsigned flags);
UV_EXTERN int uv_udp_getsockname(uv_udp_t* handle, struct sockaddr* name,
    int* namelen);

/*
 * Set membership for a multicast address
 *
 * Arguments:
 *  handle              UDP handle. Should have been initialized with
 *                      `uv_udp_init`.
 *  multicast_addr      multicast address to set membership for
 *  interface_addr      interface address
 *  membership          Should be UV_JOIN_GROUP or UV_LEAVE_GROUP
 *
 * Returns:
 *  0 on success, -1 on error.
 */
UV_EXTERN int uv_udp_set_membership(uv_udp_t* handle,
    const char* multicast_addr, const char* interface_addr,
    uv_membership membership);

/*
 * Set IP multicast loop flag. Makes multicast packets loop back to
 * local sockets.
 *
 * Arguments:
 *  handle              UDP handle. Should have been initialized with
 *                      `uv_udp_init`.
 *  on                  1 for on, 0 for off
 *
 * Returns:
 *  0 on success, -1 on error.
 */
UV_EXTERN int uv_udp_set_multicast_loop(uv_udp_t* handle, int on);

/*
 * Set the multicast ttl
 *
 * Arguments:
 *  handle              UDP handle. Should have been initialized with
 *                      `uv_udp_init`.
 *  ttl                 1 through 255
 *
 * Returns:
 *  0 on success, -1 on error.
 */
UV_EXTERN int uv_udp_set_multicast_ttl(uv_udp_t* handle, int ttl);

/*
 * Set broadcast on or off
 *
 * Arguments:
 *  handle              UDP handle. Should have been initialized with
 *                      `uv_udp_init`.
 *  on                  1 for on, 0 for off
 *
 * Returns:
 *  0 on success, -1 on error.
 */
UV_EXTERN int uv_udp_set_broadcast(uv_udp_t* handle, int on);

/*
 * Set the time to live
 *
 * Arguments:
 *  handle              UDP handle. Should have been initialized with
 *                      `uv_udp_init`.
 *  ttl                 1 through 255
 *
 * Returns:
 *  0 on success, -1 on error.
 */
UV_EXTERN int uv_udp_set_ttl(uv_udp_t* handle, int ttl);

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
UV_EXTERN int uv_udp_send(uv_udp_send_t* req, uv_udp_t* handle,
    uv_buf_t bufs[], int bufcnt, struct sockaddr_in addr,
    uv_udp_send_cb send_cb);

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
UV_EXTERN int uv_udp_send6(uv_udp_send_t* req, uv_udp_t* handle,
    uv_buf_t bufs[], int bufcnt, struct sockaddr_in6 addr,
    uv_udp_send_cb send_cb);

/*
 * Receive data. If the socket has not previously been bound with `uv_udp_bind`
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
UV_EXTERN int uv_udp_recv_start(uv_udp_t* handle, uv_alloc_cb alloc_cb,
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
UV_EXTERN int uv_udp_recv_stop(uv_udp_t* handle);


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

/*
 * Initialize a new TTY stream with the given file descriptor. Usually the
 * file descriptor will be
 *   0 = stdin
 *   1 = stdout
 *   2 = stderr
 * The last argument, readable, specifies if you plan on calling
 * uv_read_start with this stream. stdin is readable, stdout is not.
 *
 * TTY streams which are not readable have blocking writes.
 */
UV_EXTERN int uv_tty_init(uv_loop_t*, uv_tty_t*, uv_file fd, int readable);

/*
 * Set mode. 0 for normal, 1 for raw.
 */
UV_EXTERN int uv_tty_set_mode(uv_tty_t*, int mode);

/*
 * To be called when the program exits. Resets TTY settings to default
 * values for the next process to take over.
 */
UV_EXTERN void uv_tty_reset_mode(void);

/*
 * Gets the current Window size. On success zero is returned.
 */
UV_EXTERN int uv_tty_get_winsize(uv_tty_t*, int* width, int* height);

/*
 * Used to detect what type of stream should be used with a given file
 * descriptor. Usually this will be used during initialization to guess the
 * type of the stdio streams.
 * For isatty() functionality use this function and test for UV_TTY.
 */
UV_EXTERN uv_handle_type uv_guess_handle(uv_file file);

/*
 * uv_pipe_t is a subclass of uv_stream_t
 *
 * Representing a pipe stream or pipe server. On Windows this is a Named
 * Pipe. On Unix this is a UNIX domain socket.
 */
struct uv_pipe_s {
  UV_HANDLE_FIELDS
  UV_STREAM_FIELDS
  int ipc; /* non-zero if this pipe is used for passing handles */
  UV_PIPE_PRIVATE_FIELDS
};

/*
 * Initialize a pipe. The last argument is a boolean to indicate if
 * this pipe will be used for handle passing between processes.
 */
UV_EXTERN int uv_pipe_init(uv_loop_t*, uv_pipe_t* handle, int ipc);

/*
 * Opens an existing file descriptor or HANDLE as a pipe.
 */
UV_EXTERN int uv_pipe_open(uv_pipe_t*, uv_file file);

UV_EXTERN int uv_pipe_bind(uv_pipe_t* handle, const char* name);

UV_EXTERN void uv_pipe_connect(uv_connect_t* req, uv_pipe_t* handle,
    const char* name, uv_connect_cb cb);

/*
 * This setting applies to Windows only.
 * Set the number of pending pipe instance handles when the pipe server
 * is waiting for connections.
 */
UV_EXTERN void uv_pipe_pending_instances(uv_pipe_t* handle, int count);


/*
 * uv_poll_t is a subclass of uv_handle_t.
 *
 * The uv_poll watcher is used to watch file descriptors for readability and
 * writability, similar to the purpose of poll(2).
 *
 * The purpose of uv_poll is to enable integrating external libraries that
 * rely on the event loop to signal it about the socket status changes, like
 * c-ares or libssh2. Using uv_poll_t for any other other purpose is not
 * recommended; uv_tcp_t, uv_udp_t, etc. provide an implementation that is
 * much faster and more scalable than what can be achieved with uv_poll_t,
 * especially on Windows.
 *
 * It is possible that uv_poll occasionally signals that a file descriptor is
 * readable or writable even when it isn't. The user should therefore always
 * be prepared to handle EAGAIN or equivalent when it attempts to read from or
 * write to the fd.
 *
 * It is not okay to have multiple active uv_poll watchers for the same socket.
 * This can cause libuv to busyloop or otherwise malfunction.
 *
 * The user should not close a file descriptor while it is being polled by an
 * active uv_poll watcher. This can cause the poll watcher to report an error,
 * but it might also start polling another socket. However the fd can be safely
 * closed immediately after a call to uv_poll_stop() or uv_close().
 *
 * On windows only sockets can be polled with uv_poll. On unix any file
 * descriptor that would be accepted by poll(2) can be used with uv_poll.
 */
struct uv_poll_s {
  UV_HANDLE_FIELDS
  uv_poll_cb poll_cb;
  UV_POLL_PRIVATE_FIELDS
};

enum uv_poll_event {
  UV_READABLE = 1,
  UV_WRITABLE = 2
};

/* Initialize the poll watcher using a file descriptor. */
UV_EXTERN int uv_poll_init(uv_loop_t* loop, uv_poll_t* handle, int fd);

/* Initialize the poll watcher using a socket descriptor. On unix this is */
/* identical to uv_poll_init. On windows it takes a SOCKET handle. */
UV_EXTERN int uv_poll_init_socket(uv_loop_t* loop, uv_poll_t* handle,
    uv_os_sock_t socket);

/*
 * Starts polling the file descriptor. `events` is a bitmask consisting made up
 * of UV_READABLE and UV_WRITABLE. As soon as an event is detected the callback
 * will be called with `status` set to 0, and the detected events set en the
 * `events` field.
 *
 * If an error happens while polling status may be set to -1 and the error
 * code can be retrieved with uv_last_error. The user should not close the
 * socket while uv_poll is active. If the user does that anyway, the callback
 * *may* be called reporting an error status, but this is not guaranteed.
 *
 * Calling uv_poll_start on an uv_poll watcher that is already active is fine.
 * Doing so will update the events mask that is being watched for.
 */
UV_EXTERN int uv_poll_start(uv_poll_t* handle, int events, uv_poll_cb cb);

/* Stops polling the file descriptor. */
UV_EXTERN int uv_poll_stop(uv_poll_t* handle);


/*
 * uv_prepare_t is a subclass of uv_handle_t.
 *
 * Every active prepare handle gets its callback called exactly once per loop
 * iteration, just before the system blocks to wait for completed i/o.
 */
struct uv_prepare_s {
  UV_HANDLE_FIELDS
  UV_PREPARE_PRIVATE_FIELDS
};

UV_EXTERN int uv_prepare_init(uv_loop_t*, uv_prepare_t* prepare);

UV_EXTERN int uv_prepare_start(uv_prepare_t* prepare, uv_prepare_cb cb);

UV_EXTERN int uv_prepare_stop(uv_prepare_t* prepare);


/*
 * uv_check_t is a subclass of uv_handle_t.
 *
 * Every active check handle gets its callback called exactly once per loop
 * iteration, just after the system returns from blocking.
 */
struct uv_check_s {
  UV_HANDLE_FIELDS
  UV_CHECK_PRIVATE_FIELDS
};

UV_EXTERN int uv_check_init(uv_loop_t*, uv_check_t* check);

UV_EXTERN int uv_check_start(uv_check_t* check, uv_check_cb cb);

UV_EXTERN int uv_check_stop(uv_check_t* check);


/*
 * uv_idle_t is a subclass of uv_handle_t.
 *
 * Every active idle handle gets its callback called repeatedly until it is
 * stopped. This happens after all other types of callbacks are processed.
 * When there are multiple "idle" handles active, their callbacks are called
 * in turn.
 */
struct uv_idle_s {
  UV_HANDLE_FIELDS
  UV_IDLE_PRIVATE_FIELDS
};

UV_EXTERN int uv_idle_init(uv_loop_t*, uv_idle_t* idle);

UV_EXTERN int uv_idle_start(uv_idle_t* idle, uv_idle_cb cb);

UV_EXTERN int uv_idle_stop(uv_idle_t* idle);


/*
 * uv_async_t is a subclass of uv_handle_t.
 *
 * uv_async_send wakes up the event loop and calls the async handle's callback.
 * There is no guarantee that every uv_async_send call leads to exactly one
 * invocation of the callback; the only guarantee is that the callback function
 * is called at least once after the call to async_send. Unlike all other
 * libuv functions, uv_async_send can be called from another thread.
 */
struct uv_async_s {
  UV_HANDLE_FIELDS
  UV_ASYNC_PRIVATE_FIELDS
};

UV_EXTERN int uv_async_init(uv_loop_t*, uv_async_t* async,
    uv_async_cb async_cb);

/*
 * This can be called from other threads to wake up a libuv thread.
 *
 * libuv is single threaded at the moment.
 */
UV_EXTERN int uv_async_send(uv_async_t* async);


/*
 * uv_timer_t is a subclass of uv_handle_t.
 *
 * Used to get woken up at a specified time in the future.
 */
struct uv_timer_s {
  UV_HANDLE_FIELDS
  UV_TIMER_PRIVATE_FIELDS
};

UV_EXTERN int uv_timer_init(uv_loop_t*, uv_timer_t* handle);

/*
 * Start the timer. `timeout` and `repeat` are in milliseconds.
 *
 * If timeout is zero, the callback fires on the next tick of the event loop.
 *
 * If repeat is non-zero, the callback fires first after timeout milliseconds
 * and then repeatedly after repeat milliseconds.
 */
UV_EXTERN int uv_timer_start(uv_timer_t* handle,
                             uv_timer_cb cb,
                             uint64_t timeout,
                             uint64_t repeat);

UV_EXTERN int uv_timer_stop(uv_timer_t* handle);

/*
 * Stop the timer, and if it is repeating restart it using the repeat value
 * as the timeout. If the timer has never been started before it returns -1 and
 * sets the error to UV_EINVAL.
 */
UV_EXTERN int uv_timer_again(uv_timer_t* handle);

/*
 * Set the repeat value in milliseconds. Note that if the repeat value is set
 * from a timer callback it does not immediately take effect. If the timer was
 * non-repeating before, it will have been stopped. If it was repeating, then
 * the old repeat value will have been used to schedule the next timeout.
 */
UV_EXTERN void uv_timer_set_repeat(uv_timer_t* handle, uint64_t repeat);

UV_EXTERN uint64_t uv_timer_get_repeat(const uv_timer_t* handle);


/*
 * uv_getaddrinfo_t is a subclass of uv_req_t
 *
 * Request object for uv_getaddrinfo.
 */
struct uv_getaddrinfo_s {
  UV_REQ_FIELDS
  /* read-only */
  uv_loop_t* loop;
  UV_GETADDRINFO_PRIVATE_FIELDS
};


/*
 * Asynchronous getaddrinfo(3).
 *
 * Either node or service may be NULL but not both.
 *
 * hints is a pointer to a struct addrinfo with additional address type
 * constraints, or NULL. Consult `man -s 3 getaddrinfo` for details.
 *
 * Returns 0 on success, -1 on error. Call uv_last_error() to get the error.
 *
 * If successful, your callback gets called sometime in the future with the
 * lookup result, which is either:
 *
 *  a) status == 0, the res argument points to a valid struct addrinfo, or
 *  b) status == -1, the res argument is NULL.
 *
 * On NXDOMAIN, the status code is -1 and uv_last_error() returns UV_ENOENT.
 *
 * Call uv_freeaddrinfo() to free the addrinfo structure.
 */
UV_EXTERN int uv_getaddrinfo(uv_loop_t* loop,
                             uv_getaddrinfo_t* req,
                             uv_getaddrinfo_cb getaddrinfo_cb,
                             const char* node,
                             const char* service,
                             const struct addrinfo* hints);

/*
 * Free the struct addrinfo. Passing NULL is allowed and is a no-op.
 */
UV_EXTERN void uv_freeaddrinfo(struct addrinfo* ai);

/* uv_spawn() options */
typedef enum {
  UV_IGNORE         = 0x00,
  UV_CREATE_PIPE    = 0x01,
  UV_INHERIT_FD     = 0x02,
  UV_INHERIT_STREAM = 0x04,

  /* When UV_CREATE_PIPE is specified, UV_READABLE_PIPE and UV_WRITABLE_PIPE
   * determine the direction of flow, from the child process' perspective. Both
   * flags may be specified to create a duplex data stream.
   */
  UV_READABLE_PIPE  = 0x10,
  UV_WRITABLE_PIPE  = 0x20
} uv_stdio_flags;

typedef struct uv_stdio_container_s {
  uv_stdio_flags flags;

  union {
    uv_stream_t* stream;
    int fd;
  } data;
} uv_stdio_container_t;

typedef struct uv_process_options_s {
  uv_exit_cb exit_cb; /* Called after the process exits. */
  const char* file; /* Path to program to execute. */
  /*
   * Command line arguments. args[0] should be the path to the program. On
   * Windows this uses CreateProcess which concatenates the arguments into a
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
   * Various flags that control how uv_spawn() behaves. See the definition of
   * `enum uv_process_flags` below.
   */
  unsigned int flags;
  /*
   * The `stdio` field points to an array of uv_stdio_container_t structs that
   * describe the file descriptors that will be made available to the child
   * process. The convention is that stdio[0] points to stdin, fd 1 is used for
   * stdout, and fd 2 is stderr.
   *
   * Note that on windows file descriptors greater than 2 are available to the
   * child process only if the child processes uses the MSVCRT runtime.
   */
  int stdio_count;
  uv_stdio_container_t* stdio;
  /*
   * Libuv can change the child process' user/group id. This happens only when
   * the appropriate bits are set in the flags fields. This is not supported on
   * windows; uv_spawn() will fail and set the error to UV_ENOTSUP.
   */
  uv_uid_t uid;
  uv_gid_t gid;
} uv_process_options_t;

/*
 * These are the flags that can be used for the uv_process_options.flags field.
 */
enum uv_process_flags {
  /*
   * Set the child process' user id. The user id is supplied in the `uid` field
   * of the options struct. This does not work on windows; setting this flag
   * will cause uv_spawn() to fail.
   */
  UV_PROCESS_SETUID = (1 << 0),
  /*
   * Set the child process' group id. The user id is supplied in the `gid`
   * field of the options struct. This does not work on windows; setting this
   * flag will cause uv_spawn() to fail.
   */
  UV_PROCESS_SETGID = (1 << 1),
  /*
   * Do not wrap any arguments in quotes, or perform any other escaping, when
   * converting the argument list into a command line string. This option is
   * only meaningful on Windows systems. On unix it is silently ignored.
   */
  UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS = (1 << 2),
  /*
   * Spawn the child process in a detached state - this will make it a process
   * group leader, and will effectively enable the child to keep running after
   * the parent exits.  Note that the child process will still keep the
   * parent's event loop alive unless the parent process calls uv_unref() on
   * the child's process handle.
   */
  UV_PROCESS_DETACHED = (1 << 3),
  /*
   * Hide the subprocess console window that would normally be created. This
   * option is only meaningful on Windows systems. On unix it is silently
   * ignored.
   */
  UV_PROCESS_WINDOWS_HIDE = (1 << 4)
};

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
UV_EXTERN int uv_spawn(uv_loop_t*, uv_process_t*,
    uv_process_options_t options);


/*
 * Kills the process with the specified signal. The user must still
 * call uv_close on the process.
 */
UV_EXTERN int uv_process_kill(uv_process_t*, int signum);


/* Kills the process with the specified signal. */
UV_EXTERN uv_err_t uv_kill(int pid, int signum);


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
UV_EXTERN int uv_queue_work(uv_loop_t* loop, uv_work_t* req,
    uv_work_cb work_cb, uv_after_work_cb after_work_cb);

/* Cancel a pending request. Fails if the request is executing or has finished
 * executing.
 *
 * Returns 0 on success, -1 on error. The loop error code is not touched.
 *
 * Only cancellation of uv_fs_t, uv_getaddrinfo_t and uv_work_t requests is
 * currently supported.
 *
 * Cancelled requests have their callbacks invoked some time in the future.
 * It's _not_ safe to free the memory associated with the request until your
 * callback is called.
 *
 * Here is how cancellation is reported to your callback:
 *
 * - A uv_fs_t request has its req->errorno field set to UV_ECANCELED.
 *
 * - A uv_work_t or uv_getaddrinfo_t request has its callback invoked with
 *   status == -1 and uv_last_error(loop).code == UV_ECANCELED.
 *
 * This function is currently only implemented on UNIX platforms. On Windows,
 * it always returns -1.
 */
UV_EXTERN int uv_cancel(uv_req_t* req);


struct uv_cpu_info_s {
  char* model;
  int speed;
  struct uv_cpu_times_s {
    uint64_t user;
    uint64_t nice;
    uint64_t sys;
    uint64_t idle;
    uint64_t irq;
  } cpu_times;
};

struct uv_interface_address_s {
  char* name;
  int is_internal;
  union {
    struct sockaddr_in address4;
    struct sockaddr_in6 address6;
  } address;
};

UV_EXTERN char** uv_setup_args(int argc, char** argv);
UV_EXTERN uv_err_t uv_get_process_title(char* buffer, size_t size);
UV_EXTERN uv_err_t uv_set_process_title(const char* title);
UV_EXTERN uv_err_t uv_resident_set_memory(size_t* rss);
UV_EXTERN uv_err_t uv_uptime(double* uptime);

/*
 * This allocates cpu_infos array, and sets count.  The array
 * is freed using uv_free_cpu_info().
 */
UV_EXTERN uv_err_t uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count);
UV_EXTERN void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count);

/*
 * This allocates addresses array, and sets count.  The array
 * is freed using uv_free_interface_addresses().
 */
UV_EXTERN uv_err_t uv_interface_addresses(uv_interface_address_t** addresses,
  int* count);
UV_EXTERN void uv_free_interface_addresses(uv_interface_address_t* addresses,
  int count);

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
 * function to free any internal memory allocations associated with the
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
  uv_fs_type fs_type;
  uv_loop_t* loop;
  uv_fs_cb cb;
  ssize_t result;
  void* ptr;
  const char* path;
  uv_err_code errorno;
  uv_statbuf_t statbuf;  /* Stores the result of uv_fs_stat and uv_fs_fstat. */
  UV_FS_PRIVATE_FIELDS
};

UV_EXTERN void uv_fs_req_cleanup(uv_fs_t* req);

UV_EXTERN int uv_fs_close(uv_loop_t* loop, uv_fs_t* req, uv_file file,
    uv_fs_cb cb);

UV_EXTERN int uv_fs_open(uv_loop_t* loop, uv_fs_t* req, const char* path,
    int flags, int mode, uv_fs_cb cb);

UV_EXTERN int uv_fs_read(uv_loop_t* loop, uv_fs_t* req, uv_file file,
    void* buf, size_t length, int64_t offset, uv_fs_cb cb);

UV_EXTERN int uv_fs_unlink(uv_loop_t* loop, uv_fs_t* req, const char* path,
    uv_fs_cb cb);

UV_EXTERN int uv_fs_write(uv_loop_t* loop, uv_fs_t* req, uv_file file,
    void* buf, size_t length, int64_t offset, uv_fs_cb cb);

UV_EXTERN int uv_fs_mkdir(uv_loop_t* loop, uv_fs_t* req, const char* path,
    int mode, uv_fs_cb cb);

UV_EXTERN int uv_fs_rmdir(uv_loop_t* loop, uv_fs_t* req, const char* path,
    uv_fs_cb cb);

UV_EXTERN int uv_fs_readdir(uv_loop_t* loop, uv_fs_t* req,
    const char* path, int flags, uv_fs_cb cb);

UV_EXTERN int uv_fs_stat(uv_loop_t* loop, uv_fs_t* req, const char* path,
    uv_fs_cb cb);

UV_EXTERN int uv_fs_fstat(uv_loop_t* loop, uv_fs_t* req, uv_file file,
    uv_fs_cb cb);

UV_EXTERN int uv_fs_rename(uv_loop_t* loop, uv_fs_t* req, const char* path,
    const char* new_path, uv_fs_cb cb);

UV_EXTERN int uv_fs_fsync(uv_loop_t* loop, uv_fs_t* req, uv_file file,
    uv_fs_cb cb);

UV_EXTERN int uv_fs_fdatasync(uv_loop_t* loop, uv_fs_t* req, uv_file file,
    uv_fs_cb cb);

UV_EXTERN int uv_fs_ftruncate(uv_loop_t* loop, uv_fs_t* req, uv_file file,
    int64_t offset, uv_fs_cb cb);

UV_EXTERN int uv_fs_sendfile(uv_loop_t* loop, uv_fs_t* req, uv_file out_fd,
    uv_file in_fd, int64_t in_offset, size_t length, uv_fs_cb cb);

UV_EXTERN int uv_fs_chmod(uv_loop_t* loop, uv_fs_t* req, const char* path,
    int mode, uv_fs_cb cb);

UV_EXTERN int uv_fs_utime(uv_loop_t* loop, uv_fs_t* req, const char* path,
    double atime, double mtime, uv_fs_cb cb);

UV_EXTERN int uv_fs_futime(uv_loop_t* loop, uv_fs_t* req, uv_file file,
    double atime, double mtime, uv_fs_cb cb);

UV_EXTERN int uv_fs_lstat(uv_loop_t* loop, uv_fs_t* req, const char* path,
    uv_fs_cb cb);

UV_EXTERN int uv_fs_link(uv_loop_t* loop, uv_fs_t* req, const char* path,
    const char* new_path, uv_fs_cb cb);

/*
 * This flag can be used with uv_fs_symlink on Windows
 * to specify whether path argument points to a directory.
 */
#define UV_FS_SYMLINK_DIR          0x0001

/*
 * This flag can be used with uv_fs_symlink on Windows
 * to specify whether the symlink is to be created using junction points.
 */
#define UV_FS_SYMLINK_JUNCTION     0x0002

UV_EXTERN int uv_fs_symlink(uv_loop_t* loop, uv_fs_t* req, const char* path,
    const char* new_path, int flags, uv_fs_cb cb);

UV_EXTERN int uv_fs_readlink(uv_loop_t* loop, uv_fs_t* req, const char* path,
    uv_fs_cb cb);

UV_EXTERN int uv_fs_fchmod(uv_loop_t* loop, uv_fs_t* req, uv_file file,
    int mode, uv_fs_cb cb);

UV_EXTERN int uv_fs_chown(uv_loop_t* loop, uv_fs_t* req, const char* path,
    uv_uid_t uid, uv_gid_t gid, uv_fs_cb cb);

UV_EXTERN int uv_fs_fchown(uv_loop_t* loop, uv_fs_t* req, uv_file file,
    uv_uid_t uid, uv_gid_t gid, uv_fs_cb cb);


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
 * uv_fs_stat() based polling file watcher.
 */
struct uv_fs_poll_s {
  UV_HANDLE_FIELDS
  /* Private, don't touch. */
  void* poll_ctx;
};

UV_EXTERN int uv_fs_poll_init(uv_loop_t* loop, uv_fs_poll_t* handle);

/*
 * Check the file at `path` for changes every `interval` milliseconds.
 *
 * Your callback i invoked with `status == -1` if `path` does not exist
 * or is inaccessible. The watcher is *not* stopped but your callback is
 * not called again until something changes (e.g. when the file is created
 * or the error reason changes).
 *
 * When `status == 0`, your callback receives pointers to the old and new
 * `uv_statbuf_t` structs. They are valid for the duration of the callback
 * only!
 *
 * For maximum portability, use multi-second intervals. Sub-second intervals
 * will not detect all changes on many file systems.
 */
UV_EXTERN int uv_fs_poll_start(uv_fs_poll_t* handle,
                               uv_fs_poll_cb poll_cb,
                               const char* path,
                               unsigned int interval);

UV_EXTERN int uv_fs_poll_stop(uv_fs_poll_t* handle);


/*
 * UNIX signal handling on a per-event loop basis. The implementation is not
 * ultra efficient so don't go creating a million event loops with a million
 * signal watchers.
 *
 * Note to Linux users: SIGRT0 and SIGRT1 (signals 32 and 33) are used by the
 * NPTL pthreads library to manage threads. Installing watchers for those
 * signals will lead to unpredictable behavior and is strongly discouraged.
 * Future versions of libuv may simply reject them.
 *
 * Some signal support is available on Windows:
 *
 *   SIGINT is normally delivered when the user presses CTRL+C. However, like
 *   on Unix, it is not generated when terminal raw mode is enabled.
 *
 *   SIGBREAK is delivered when the user pressed CTRL+BREAK.
 *
 *   SIGHUP is generated when the user closes the console window. On SIGHUP the
 *   program is given approximately 10 seconds to perform cleanup. After that
 *   Windows will unconditionally terminate it.
 *
 *   SIGWINCH is raised whenever libuv detects that the console has been
 *   resized. SIGWINCH is emulated by libuv when the program uses an uv_tty_t
 *   handle to write to the console. SIGWINCH may not always be delivered in a
 *   timely manner; libuv will only detect size changes when the cursor is
 *   being moved. When a readable uv_tty_handle is used in raw mode, resizing
 *   the console buffer will also trigger a SIGWINCH signal.
 *
 * Watchers for other signals can be successfully created, but these signals
 * are never generated. These signals are: SIGILL, SIGABRT, SIGFPE, SIGSEGV,
 * SIGTERM and SIGKILL.
 *
 * Note that calls to raise() or abort() to programmatically raise a signal are
 * not detected by libuv; these will not trigger a signal watcher.
 */
struct uv_signal_s {
  UV_HANDLE_FIELDS
  uv_signal_cb signal_cb;
  int signum;
  UV_SIGNAL_PRIVATE_FIELDS
};

UV_EXTERN int uv_signal_init(uv_loop_t* loop, uv_signal_t* handle);

UV_EXTERN int uv_signal_start(uv_signal_t* handle,
                              uv_signal_cb signal_cb,
                              int signum);

UV_EXTERN int uv_signal_stop(uv_signal_t* handle);


/*
 * Gets load avg
 * See: http://en.wikipedia.org/wiki/Load_(computing)
 * (Returns [0,0,0] for windows and cygwin)
 */
UV_EXTERN void uv_loadavg(double avg[3]);


/*
 * Flags to be passed to uv_fs_event_init.
 */
enum uv_fs_event_flags {
  /*
   * By default, if the fs event watcher is given a directory name, we will
   * watch for all events in that directory. This flags overrides this behavior
   * and makes fs_event report only changes to the directory entry itself. This
   * flag does not affect individual files watched.
   * This flag is currently not implemented yet on any backend.
   */
 UV_FS_EVENT_WATCH_ENTRY = 1,

  /*
   * By default uv_fs_event will try to use a kernel interface such as inotify
   * or kqueue to detect events. This may not work on remote filesystems such
   * as NFS mounts. This flag makes fs_event fall back to calling stat() on a
   * regular interval.
   * This flag is currently not implemented yet on any backend.
   */
  UV_FS_EVENT_STAT = 2,

  /*
   * By default, event watcher, when watching directory, is not registering
   * (is ignoring) changes in it's subdirectories.
   * This flag will override this behaviour on platforms that support it.
   */
  UV_FS_EVENT_RECURSIVE = 3
};


UV_EXTERN int uv_fs_event_init(uv_loop_t* loop, uv_fs_event_t* handle,
    const char* filename, uv_fs_event_cb cb, int flags);

/* Utility */

/* Convert string ip addresses to binary structures */
UV_EXTERN struct sockaddr_in uv_ip4_addr(const char* ip, int port);
UV_EXTERN struct sockaddr_in6 uv_ip6_addr(const char* ip, int port);

/* Convert binary addresses to strings */
UV_EXTERN int uv_ip4_name(struct sockaddr_in* src, char* dst, size_t size);
UV_EXTERN int uv_ip6_name(struct sockaddr_in6* src, char* dst, size_t size);

/* Cross-platform IPv6-capable implementation of the 'standard' inet_ntop */
/* and inet_pton functions. On success they return UV_OK. If an error */
/* the target of the `dst` pointer is unmodified. */
UV_EXTERN uv_err_t uv_inet_ntop(int af, const void* src, char* dst, size_t size);
UV_EXTERN uv_err_t uv_inet_pton(int af, const char* src, void* dst);

/* Gets the executable path */
UV_EXTERN int uv_exepath(char* buffer, size_t* size);

/* Gets the current working directory */
UV_EXTERN uv_err_t uv_cwd(char* buffer, size_t size);

/* Changes the current working directory */
UV_EXTERN uv_err_t uv_chdir(const char* dir);

/* Gets memory info in bytes */
UV_EXTERN uint64_t uv_get_free_memory(void);
UV_EXTERN uint64_t uv_get_total_memory(void);

/*
 * Returns the current high-resolution real time. This is expressed in
 * nanoseconds. It is relative to an arbitrary time in the past. It is not
 * related to the time of day and therefore not subject to clock drift. The
 * primary use is for measuring performance between intervals.
 *
 * Note not every platform can support nanosecond resolution; however, this
 * value will always be in nanoseconds.
 */
UV_EXTERN extern uint64_t uv_hrtime(void);


/*
 * Disables inheritance for file descriptors / handles that this process
 * inherited from its parent. The effect is that child processes spawned by
 * this process don't accidentally inherit these handles.
 *
 * It is recommended to call this function as early in your program as possible,
 * before the inherited file descriptors can be closed or duplicated.
 *
 * Note that this function works on a best-effort basis: there is no guarantee
 * that libuv can discover all file descriptors that were inherited. In general
 * it does a better job on Windows than it does on unix.
 */
UV_EXTERN void uv_disable_stdio_inheritance(void);

/*
 * Opens a shared library. The filename is in utf-8. Returns 0 on success and
 * -1 on error. Call `uv_dlerror(uv_lib_t*)` to get the error message.
 */
UV_EXTERN int uv_dlopen(const char* filename, uv_lib_t* lib);

/*
 * Close the shared library.
 */
UV_EXTERN void uv_dlclose(uv_lib_t* lib);

/*
 * Retrieves a data pointer from a dynamic library. It is legal for a symbol to
 * map to NULL. Returns 0 on success and -1 if the symbol was not found.
 */
UV_EXTERN int uv_dlsym(uv_lib_t* lib, const char* name, void** ptr);

/*
 * Returns the last uv_dlopen() or uv_dlsym() error message.
 */
UV_EXTERN const char* uv_dlerror(uv_lib_t* lib);

/*
 * The mutex functions return 0 on success, -1 on error
 * (unless the return type is void, of course).
 */
UV_EXTERN int uv_mutex_init(uv_mutex_t* handle);
UV_EXTERN void uv_mutex_destroy(uv_mutex_t* handle);
UV_EXTERN void uv_mutex_lock(uv_mutex_t* handle);
UV_EXTERN int uv_mutex_trylock(uv_mutex_t* handle);
UV_EXTERN void uv_mutex_unlock(uv_mutex_t* handle);

/*
 * Same goes for the read/write lock functions.
 */
UV_EXTERN int uv_rwlock_init(uv_rwlock_t* rwlock);
UV_EXTERN void uv_rwlock_destroy(uv_rwlock_t* rwlock);
UV_EXTERN void uv_rwlock_rdlock(uv_rwlock_t* rwlock);
UV_EXTERN int uv_rwlock_tryrdlock(uv_rwlock_t* rwlock);
UV_EXTERN void uv_rwlock_rdunlock(uv_rwlock_t* rwlock);
UV_EXTERN void uv_rwlock_wrlock(uv_rwlock_t* rwlock);
UV_EXTERN int uv_rwlock_trywrlock(uv_rwlock_t* rwlock);
UV_EXTERN void uv_rwlock_wrunlock(uv_rwlock_t* rwlock);

/*
 * Same goes for the semaphore functions.
 */
UV_EXTERN int uv_sem_init(uv_sem_t* sem, unsigned int value);
UV_EXTERN void uv_sem_destroy(uv_sem_t* sem);
UV_EXTERN void uv_sem_post(uv_sem_t* sem);
UV_EXTERN void uv_sem_wait(uv_sem_t* sem);
UV_EXTERN int uv_sem_trywait(uv_sem_t* sem);

/*
 * Same goes for the condition variable functions.
 */
UV_EXTERN int uv_cond_init(uv_cond_t* cond);
UV_EXTERN void uv_cond_destroy(uv_cond_t* cond);
UV_EXTERN void uv_cond_signal(uv_cond_t* cond);
UV_EXTERN void uv_cond_broadcast(uv_cond_t* cond);
/* Waits on a condition variable without a timeout.
 *
 * Note:
 * 1. callers should be prepared to deal with spurious wakeups.
 */
UV_EXTERN void uv_cond_wait(uv_cond_t* cond, uv_mutex_t* mutex);
/* Waits on a condition variable with a timeout in nano seconds.
 * Returns 0 for success or -1 on timeout, * aborts when other errors happen.
 *
 * Note:
 * 1. callers should be prepared to deal with spurious wakeups.
 * 2. the granularity of timeout on Windows is never less than one millisecond.
 * 3. uv_cond_timedwait takes a relative timeout, not an absolute time.
 */
UV_EXTERN int uv_cond_timedwait(uv_cond_t* cond, uv_mutex_t* mutex,
    uint64_t timeout);

UV_EXTERN int uv_barrier_init(uv_barrier_t* barrier, unsigned int count);
UV_EXTERN void uv_barrier_destroy(uv_barrier_t* barrier);
UV_EXTERN void uv_barrier_wait(uv_barrier_t* barrier);

/* Runs a function once and only once. Concurrent calls to uv_once() with the
 * same guard will block all callers except one (it's unspecified which one).
 * The guard should be initialized statically with the UV_ONCE_INIT macro.
 */
UV_EXTERN void uv_once(uv_once_t* guard, void (*callback)(void));

UV_EXTERN int uv_thread_create(uv_thread_t *tid,
    void (*entry)(void *arg), void *arg);
UV_EXTERN unsigned long uv_thread_self(void);
UV_EXTERN int uv_thread_join(uv_thread_t *tid);

/* the presence of these unions force similar struct layout */
union uv_any_handle {
  uv_handle_t handle;
  uv_stream_t stream;
  uv_tcp_t tcp;
  uv_pipe_t pipe;
  uv_prepare_t prepare;
  uv_check_t check;
  uv_idle_t idle;
  uv_async_t async;
  uv_timer_t timer;
  uv_fs_event_t fs_event;
  uv_fs_poll_t fs_poll;
  uv_poll_t poll;
  uv_process_t process;
  uv_tty_t tty;
  uv_udp_t udp;
};

union uv_any_req {
  uv_req_t req;
  uv_write_t write;
  uv_connect_t connect;
  uv_shutdown_t shutdown;
  uv_fs_t fs_req;
  uv_work_t work_req;
  uv_udp_send_t udp_send_req;
  uv_getaddrinfo_t getaddrinfo_req;
};


struct uv_loop_s {
  /* User data - use this for whatever. */
  void* data;
  /* The last error */
  uv_err_t last_err;
  /* Loop reference counting */
  unsigned int active_handles;
  ngx_queue_t handle_queue;
  ngx_queue_t active_reqs;
  /* Internal flag to signal loop stop */
  unsigned int stop_flag;
  UV_LOOP_PRIVATE_FIELDS
};


/* Don't export the private CPP symbols. */
#undef UV_HANDLE_TYPE_PRIVATE
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
#undef UV_SIGNAL_PRIVATE_FIELDS
#undef UV_LOOP_PRIVATE_FIELDS
#undef UV_LOOP_PRIVATE_PLATFORM_FIELDS

#ifdef __cplusplus
}
#endif
#endif /* UV_H */
