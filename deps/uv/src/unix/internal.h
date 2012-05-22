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

#ifndef UV_UNIX_INTERNAL_H_
#define UV_UNIX_INTERNAL_H_

#include "uv-common.h"
#include "uv-eio.h"

#include <assert.h>
#include <stdlib.h> /* abort */

#if __STRICT_ANSI__
# define inline __inline
#endif

#undef HAVE_FUTIMES
#undef HAVE_KQUEUE
#undef HAVE_PORTS_FS

#if __linux__
# include "linux/syscalls.h"
# define HAVE_FUTIMES 1 /* emulated with utimesat() */
#endif /* __linux__ */

#if defined(__sun)
# include <sys/port.h>
# include <port.h>
# ifdef PORT_SOURCE_FILE
#  define HAVE_PORTS_FS 1
# endif
# define HAVE_FUTIMES 1
# define futimes(fd, tv) futimesat(fd, (void*)0, tv)
#endif /* __sun */

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__sun)
# define HAVE_FUTIMES 1
#endif

/* FIXME exact copy of the #ifdef guard in uv-unix.h */
#if defined(__APPLE__)  \
  || defined(__FreeBSD__) \
  || defined(__OpenBSD__) \
  || defined(__NetBSD__)
# define HAVE_KQUEUE 1
#endif

#define UNREACHABLE()                                                         \
  do {                                                                        \
    assert(0 && "unreachable code");                                          \
    abort();                                                                  \
  }                                                                           \
  while (0)

#define SAVE_ERRNO(block)                                                     \
  do {                                                                        \
    int _saved_errno = errno;                                                 \
    do { block; } while (0);                                                  \
    errno = _saved_errno;                                                     \
  }                                                                           \
  while (0)

/* flags */
enum {
  UV_CLOSING       = 0x01,   /* uv_close() called but not finished. */
  UV_CLOSED        = 0x02,   /* close(2) finished. */
  UV_STREAM_READING   = 0x04,   /* uv_read_start() called. */
  UV_STREAM_SHUTTING  = 0x08,   /* uv_shutdown() called but not complete. */
  UV_STREAM_SHUT      = 0x10,   /* Write side closed. */
  UV_STREAM_READABLE  = 0x20,   /* The stream is readable */
  UV_STREAM_WRITABLE  = 0x40,   /* The stream is writable */
  UV_TCP_NODELAY   = 0x080,  /* Disable Nagle. */
  UV_TCP_KEEPALIVE = 0x100,  /* Turn on keep-alive. */
  UV_TIMER_REPEAT  = 0x100,
  UV__PENDING      = 0x800
};

inline static int uv__has_pending_handles(const uv_loop_t* loop) {
  return loop->pending_handles != NULL;
}

inline static void uv__make_pending(uv_handle_t* h) {
  if (h->flags & UV__PENDING) return;
  h->next_pending = h->loop->pending_handles;
  h->loop->pending_handles = h;
  h->flags |= UV__PENDING;
}
#define uv__make_pending(h) uv__make_pending((uv_handle_t*)(h))

inline static void uv__req_init(uv_loop_t* loop,
                                uv_req_t* req,
                                uv_req_type type) {
  loop->counters.req_init++;
  req->type = type;
#ifndef UV_LEAN_AND_MEAN
  ngx_queue_insert_tail(&loop->active_reqs, &req->active_queue);
#else
  loop->active_reqs++;
#endif
}
#define uv__req_init(loop, req, type) \
  uv__req_init((loop), (uv_req_t*)(req), (type))

/* core */
void uv__handle_init(uv_loop_t* loop, uv_handle_t* handle, uv_handle_type type);
int uv__nonblock(int fd, int set) __attribute__((unused));
int uv__cloexec(int fd, int set) __attribute__((unused));
int uv__socket(int domain, int type, int protocol);
int uv__dup(int fd);
int uv_async_stop(uv_async_t* handle);

/* loop */
int uv__loop_init(uv_loop_t* loop, int default_loop);
void uv__loop_delete(uv_loop_t* loop);
void uv__run_idle(uv_loop_t* loop);
void uv__run_check(uv_loop_t* loop);
void uv__run_prepare(uv_loop_t* loop);

/* error */
uv_err_code uv_translate_sys_error(int sys_errno);
void uv_fatal_error(const int errorno, const char* syscall);

/* stream */
void uv__stream_init(uv_loop_t* loop, uv_stream_t* stream,
    uv_handle_type type);
int uv__stream_open(uv_stream_t*, int fd, int flags);
void uv__stream_destroy(uv_stream_t* stream);
void uv__stream_io(EV_P_ ev_io* watcher, int revents);
void uv__server_io(EV_P_ ev_io* watcher, int revents);
int uv__accept(int sockfd, struct sockaddr* saddr, socklen_t len);
int uv__connect(uv_connect_t* req, uv_stream_t* stream, struct sockaddr* addr,
    socklen_t addrlen, uv_connect_cb cb);

/* tcp */
int uv_tcp_listen(uv_tcp_t* tcp, int backlog, uv_connection_cb cb);
int uv__tcp_nodelay(uv_tcp_t* handle, int enable);
int uv__tcp_keepalive(uv_tcp_t* handle, int enable, unsigned int delay);

/* pipe */
int uv_pipe_listen(uv_pipe_t* handle, int backlog, uv_connection_cb cb);
void uv__pipe_accept(EV_P_ ev_io* watcher, int revents);

/* poll */
void uv__poll_close(uv_poll_t* handle);
int uv__poll_active(const uv_poll_t* handle);

/* various */
void uv__async_close(uv_async_t* handle);
void uv__check_close(uv_check_t* handle);
void uv__fs_event_close(uv_fs_event_t* handle);
void uv__idle_close(uv_idle_t* handle);
void uv__pipe_close(uv_pipe_t* handle);
void uv__prepare_close(uv_prepare_t* handle);
void uv__process_close(uv_process_t* handle);
void uv__stream_close(uv_stream_t* handle);
void uv__timer_close(uv_timer_t* handle);
void uv__udp_close(uv_udp_t* handle);
void uv__udp_finish_close(uv_udp_t* handle);

void uv__stream_pending(uv_stream_t* handle);

#define UV__F_IPC        (1 << 0)
#define UV__F_NONBLOCK   (1 << 1)
int uv__make_socketpair(int fds[2], int flags);
int uv__make_pipe(int fds[2], int flags);

#endif /* UV_UNIX_INTERNAL_H_ */
