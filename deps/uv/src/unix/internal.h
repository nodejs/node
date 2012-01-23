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

#include <stddef.h> /* offsetof */

#if __STRICT_ANSI__
# define inline __inline
#endif

#undef HAVE_FUTIMES
#undef HAVE_KQUEUE
#undef HAVE_PORTS_FS

#if defined(__linux__)

# undef HAVE_SYS_UTIMESAT
# undef HAVE_SYS_PIPE2
# undef HAVE_SYS_ACCEPT4

# undef _GNU_SOURCE
# define _GNU_SOURCE

# include <linux/version.h>
# include <sys/syscall.h>
# include <features.h>
# include <unistd.h>

# if __NR_utimensat
#  define HAVE_SYS_UTIMESAT 1
# endif
# if __NR_pipe2
#  define HAVE_SYS_PIPE2 1
# endif
# if __NR_accept4
#  define HAVE_SYS_ACCEPT4 1
# endif

# ifndef O_CLOEXEC
#  define O_CLOEXEC 02000000
# endif

# ifndef SOCK_CLOEXEC
#  define SOCK_CLOEXEC O_CLOEXEC
# endif

# ifndef SOCK_NONBLOCK
#  define SOCK_NONBLOCK O_NONBLOCK
# endif

# if HAVE_SYS_UTIMESAT
inline static int sys_utimesat(int dirfd,
                               const char* path,
                               const struct timespec times[2],
                               int flags)
{
  return syscall(__NR_utimensat, dirfd, path, times, flags);
}
inline static int sys_futimes(int fd, const struct timeval times[2])
{
  struct timespec ts[2];
  ts[0].tv_sec = times[0].tv_sec, ts[0].tv_nsec = times[0].tv_usec * 1000;
  ts[1].tv_sec = times[1].tv_sec, ts[1].tv_nsec = times[1].tv_usec * 1000;
  return sys_utimesat(fd, NULL, ts, 0);
}
#  undef HAVE_FUTIMES
#  define HAVE_FUTIMES 1
#  define futimes(fd, times) sys_futimes(fd, times)
# endif /* HAVE_SYS_FUTIMESAT */

# if HAVE_SYS_PIPE2
inline static int sys_pipe2(int pipefd[2], int flags)
{
  return syscall(__NR_pipe2, pipefd, flags);
}
# endif /* HAVE_SYS_PIPE2 */

# if HAVE_SYS_ACCEPT4
inline static int sys_accept4(int fd,
                              struct sockaddr* addr,
                              socklen_t* addrlen,
                              int flags)
{
  return syscall(__NR_accept4, fd, addr, addrlen, flags);
}
# endif /* HAVE_SYS_ACCEPT4 */

#endif /* __linux__ */

#if defined(__sun)
# include <sys/port.h>
# include <port.h>
# ifdef PORT_SOURCE_FILE
#  define HAVE_PORTS_FS 1
# endif
#endif /* __sun */

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__sun)
# define HAVE_FUTIMES 1
#endif

/* FIXME exact copy of the #ifdef guard in uv-unix.h */
#if (defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1060) \
  || defined(__FreeBSD__) \
  || defined(__OpenBSD__) \
  || defined(__NetBSD__)
# define HAVE_KQUEUE 1
#endif

#define container_of(ptr, type, member) \
  ((type *) ((char *) (ptr) - offsetof(type, member)))

#define SAVE_ERRNO(block) \
  do { \
    int _saved_errno = errno; \
    do { block; } while (0); \
    errno = _saved_errno; \
  } \
  while (0);

/* flags */
enum {
  UV_CLOSING       = 0x01,   /* uv_close() called but not finished. */
  UV_CLOSED        = 0x02,   /* close(2) finished. */
  UV_READING       = 0x04,   /* uv_read_start() called. */
  UV_SHUTTING      = 0x08,   /* uv_shutdown() called but not complete. */
  UV_SHUT          = 0x10,   /* Write side closed. */
  UV_READABLE      = 0x20,   /* The stream is readable */
  UV_WRITABLE      = 0x40,   /* The stream is writable */
  UV_TCP_NODELAY   = 0x080,  /* Disable Nagle. */
  UV_TCP_KEEPALIVE = 0x100   /* Turn on keep-alive. */
};

/* core */
void uv__handle_init(uv_loop_t* loop, uv_handle_t* handle, uv_handle_type type);
int uv__nonblock(int fd, int set) __attribute__((unused));
int uv__cloexec(int fd, int set) __attribute__((unused));
int uv__socket(int domain, int type, int protocol);

/* We used to handle EINTR in uv__close() but linux 2.6 will have closed the
 * file descriptor anyway, even on EINTR. Retrying in that case isn't merely
 * useless, it's actively harmful - the file descriptor may have been acquired
 * by another thread.
 */
#define uv__close(fd) close(fd)

/* error */
uv_err_code uv_translate_sys_error(int sys_errno);
void uv_fatal_error(const int errorno, const char* syscall);

/* requests */
void uv__req_init(uv_loop_t* loop, uv_req_t*);

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
int uv_pipe_cleanup(uv_pipe_t* handle);

/* udp */
void uv__udp_start_close(uv_udp_t* handle);
void uv__udp_finish_close(uv_udp_t* handle);

/* fs */
void uv__fs_event_destroy(uv_fs_event_t* handle);

#define UV__F_IPC        (1 << 0)
#define UV__F_NONBLOCK   (1 << 1)
int uv__make_socketpair(int fds[2], int flags);
int uv__make_pipe(int fds[2], int flags);

#endif /* UV_UNIX_INTERNAL_H_ */
