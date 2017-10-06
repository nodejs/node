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

#include <assert.h>
#include <stdlib.h> /* abort */
#include <string.h> /* strrchr */
#include <fcntl.h>  /* O_CLOEXEC, may be */
#include <stdio.h>

#if defined(__STRICT_ANSI__)
# define inline __inline
#endif

#if defined(__linux__)
# include "linux-syscalls.h"
#endif /* __linux__ */

#if defined(__MVS__)
# include "os390-syscalls.h"
#endif /* __MVS__ */

#if defined(__sun)
# include <sys/port.h>
# include <port.h>
#endif /* __sun */

#if defined(_AIX)
# define reqevents events
# define rtnevents revents
# include <sys/poll.h>
#else
# include <poll.h>
#endif /* _AIX */

#if defined(__APPLE__) && !TARGET_OS_IPHONE
# include <AvailabilityMacros.h>
#endif

#if defined(__ANDROID__)
int uv__pthread_sigmask(int how, const sigset_t* set, sigset_t* oset);
# ifdef pthread_sigmask
# undef pthread_sigmask
# endif
# define pthread_sigmask(how, set, oldset) uv__pthread_sigmask(how, set, oldset)
#endif

#define ACCESS_ONCE(type, var)                                                \
  (*(volatile type*) &(var))

#define ROUND_UP(a, b)                                                        \
  ((a) % (b) ? ((a) + (b)) - ((a) % (b)) : (a))

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

/* The __clang__ and __INTEL_COMPILER checks are superfluous because they
 * define __GNUC__. They are here to convey to you, dear reader, that these
 * macros are enabled when compiling with clang or icc.
 */
#if defined(__clang__) ||                                                     \
    defined(__GNUC__) ||                                                      \
    defined(__INTEL_COMPILER) ||                                              \
    defined(__SUNPRO_C)
# define UV_DESTRUCTOR(declaration) __attribute__((destructor)) declaration
# define UV_UNUSED(declaration)     __attribute__((unused)) declaration
#else
# define UV_DESTRUCTOR(declaration) declaration
# define UV_UNUSED(declaration)     declaration
#endif

/* Leans on the fact that, on Linux, POLLRDHUP == EPOLLRDHUP. */
#ifdef POLLRDHUP
# define UV__POLLRDHUP POLLRDHUP
#else
# define UV__POLLRDHUP 0x2000
#endif

#ifdef POLLPRI
# define UV__POLLPRI POLLPRI
#else
# define UV__POLLPRI 0
#endif

#if !defined(O_CLOEXEC) && defined(__FreeBSD__)
/*
 * It may be that we are just missing `__POSIX_VISIBLE >= 200809`.
 * Try using fixed value const and give up, if it doesn't work
 */
# define O_CLOEXEC 0x00100000
#endif

typedef struct uv__stream_queued_fds_s uv__stream_queued_fds_t;

/* handle flags */
enum {
  UV_CLOSING              = 0x01,   /* uv_close() called but not finished. */
  UV_CLOSED               = 0x02,   /* close(2) finished. */
  UV_STREAM_READING       = 0x04,   /* uv_read_start() called. */
  UV_STREAM_SHUTTING      = 0x08,   /* uv_shutdown() called but not complete. */
  UV_STREAM_SHUT          = 0x10,   /* Write side closed. */
  UV_STREAM_READABLE      = 0x20,   /* The stream is readable */
  UV_STREAM_WRITABLE      = 0x40,   /* The stream is writable */
  UV_STREAM_BLOCKING      = 0x80,   /* Synchronous writes. */
  UV_STREAM_READ_PARTIAL  = 0x100,  /* read(2) read less than requested. */
  UV_STREAM_READ_EOF      = 0x200,  /* read(2) read EOF. */
  UV_TCP_NODELAY          = 0x400,  /* Disable Nagle. */
  UV_TCP_KEEPALIVE        = 0x800,  /* Turn on keep-alive. */
  UV_TCP_SINGLE_ACCEPT    = 0x1000, /* Only accept() when idle. */
  UV_HANDLE_IPV6          = 0x10000, /* Handle is bound to a IPv6 socket. */
  UV_UDP_PROCESSING       = 0x20000, /* Handle is running the send callback queue. */
  UV_HANDLE_BOUND         = 0x40000  /* Handle is bound to an address and port */
};

/* loop flags */
enum {
  UV_LOOP_BLOCK_SIGPROF = 1
};

/* flags of excluding ifaddr */
enum {
  UV__EXCLUDE_IFPHYS,
  UV__EXCLUDE_IFADDR
};

typedef enum {
  UV_CLOCK_PRECISE = 0,  /* Use the highest resolution clock available. */
  UV_CLOCK_FAST = 1      /* Use the fastest clock with <= 1ms granularity. */
} uv_clocktype_t;

struct uv__stream_queued_fds_s {
  unsigned int size;
  unsigned int offset;
  int fds[1];
};


#if defined(_AIX) || \
    defined(__APPLE__) || \
    defined(__DragonFly__) || \
    defined(__FreeBSD__) || \
    defined(__FreeBSD_kernel__) || \
    defined(__linux__) || \
    defined(__OpenBSD__) || \
    defined(__NetBSD__)
#define uv__cloexec uv__cloexec_ioctl
#define uv__nonblock uv__nonblock_ioctl
#else
#define uv__cloexec uv__cloexec_fcntl
#define uv__nonblock uv__nonblock_fcntl
#endif

/* core */
int uv__cloexec_ioctl(int fd, int set);
int uv__cloexec_fcntl(int fd, int set);
int uv__nonblock_ioctl(int fd, int set);
int uv__nonblock_fcntl(int fd, int set);
int uv__close(int fd);
int uv__close_nocheckstdio(int fd);
int uv__socket(int domain, int type, int protocol);
int uv__dup(int fd);
ssize_t uv__recvmsg(int fd, struct msghdr *msg, int flags);
void uv__make_close_pending(uv_handle_t* handle);
int uv__getiovmax(void);

void uv__io_init(uv__io_t* w, uv__io_cb cb, int fd);
void uv__io_start(uv_loop_t* loop, uv__io_t* w, unsigned int events);
void uv__io_stop(uv_loop_t* loop, uv__io_t* w, unsigned int events);
void uv__io_close(uv_loop_t* loop, uv__io_t* w);
void uv__io_feed(uv_loop_t* loop, uv__io_t* w);
int uv__io_active(const uv__io_t* w, unsigned int events);
int uv__io_check_fd(uv_loop_t* loop, int fd);
void uv__io_poll(uv_loop_t* loop, int timeout); /* in milliseconds or -1 */
int uv__io_fork(uv_loop_t* loop);

/* async */
void uv__async_stop(uv_loop_t* loop);
int uv__async_fork(uv_loop_t* loop);


/* loop */
void uv__run_idle(uv_loop_t* loop);
void uv__run_check(uv_loop_t* loop);
void uv__run_prepare(uv_loop_t* loop);

/* stream */
void uv__stream_init(uv_loop_t* loop, uv_stream_t* stream,
    uv_handle_type type);
int uv__stream_open(uv_stream_t*, int fd, int flags);
void uv__stream_destroy(uv_stream_t* stream);
#if defined(__APPLE__)
int uv__stream_try_select(uv_stream_t* stream, int* fd);
#endif /* defined(__APPLE__) */
void uv__server_io(uv_loop_t* loop, uv__io_t* w, unsigned int events);
int uv__accept(int sockfd);
int uv__dup2_cloexec(int oldfd, int newfd);
int uv__open_cloexec(const char* path, int flags);

/* tcp */
int uv_tcp_listen(uv_tcp_t* tcp, int backlog, uv_connection_cb cb);
int uv__tcp_nodelay(int fd, int on);
int uv__tcp_keepalive(int fd, int on, unsigned int delay);

/* pipe */
int uv_pipe_listen(uv_pipe_t* handle, int backlog, uv_connection_cb cb);

/* timer */
void uv__run_timers(uv_loop_t* loop);
int uv__next_timeout(const uv_loop_t* loop);

/* signal */
void uv__signal_close(uv_signal_t* handle);
void uv__signal_global_once_init(void);
void uv__signal_loop_cleanup(uv_loop_t* loop);
int uv__signal_loop_fork(uv_loop_t* loop);

/* platform specific */
uint64_t uv__hrtime(uv_clocktype_t type);
int uv__kqueue_init(uv_loop_t* loop);
int uv__platform_loop_init(uv_loop_t* loop);
void uv__platform_loop_delete(uv_loop_t* loop);
void uv__platform_invalidate_fd(uv_loop_t* loop, int fd);

/* various */
void uv__async_close(uv_async_t* handle);
void uv__check_close(uv_check_t* handle);
void uv__fs_event_close(uv_fs_event_t* handle);
void uv__idle_close(uv_idle_t* handle);
void uv__pipe_close(uv_pipe_t* handle);
void uv__poll_close(uv_poll_t* handle);
void uv__prepare_close(uv_prepare_t* handle);
void uv__process_close(uv_process_t* handle);
void uv__stream_close(uv_stream_t* handle);
void uv__tcp_close(uv_tcp_t* handle);
void uv__timer_close(uv_timer_t* handle);
void uv__udp_close(uv_udp_t* handle);
void uv__udp_finish_close(uv_udp_t* handle);
uv_handle_type uv__handle_type(int fd);
FILE* uv__open_file(const char* path);
int uv__getpwuid_r(uv_passwd_t* pwd);


#if defined(__APPLE__)
int uv___stream_fd(const uv_stream_t* handle);
#define uv__stream_fd(handle) (uv___stream_fd((const uv_stream_t*) (handle)))
#else
#define uv__stream_fd(handle) ((handle)->io_watcher.fd)
#endif /* defined(__APPLE__) */

#ifdef UV__O_NONBLOCK
# define UV__F_NONBLOCK UV__O_NONBLOCK
#else
# define UV__F_NONBLOCK 1
#endif

int uv__make_socketpair(int fds[2], int flags);
int uv__make_pipe(int fds[2], int flags);

#if defined(__APPLE__)

int uv__fsevents_init(uv_fs_event_t* handle);
int uv__fsevents_close(uv_fs_event_t* handle);
void uv__fsevents_loop_delete(uv_loop_t* loop);

/* OSX < 10.7 has no file events, polyfill them */
#ifndef MAC_OS_X_VERSION_10_7

static const int kFSEventStreamCreateFlagFileEvents = 0x00000010;
static const int kFSEventStreamEventFlagItemCreated = 0x00000100;
static const int kFSEventStreamEventFlagItemRemoved = 0x00000200;
static const int kFSEventStreamEventFlagItemInodeMetaMod = 0x00000400;
static const int kFSEventStreamEventFlagItemRenamed = 0x00000800;
static const int kFSEventStreamEventFlagItemModified = 0x00001000;
static const int kFSEventStreamEventFlagItemFinderInfoMod = 0x00002000;
static const int kFSEventStreamEventFlagItemChangeOwner = 0x00004000;
static const int kFSEventStreamEventFlagItemXattrMod = 0x00008000;
static const int kFSEventStreamEventFlagItemIsFile = 0x00010000;
static const int kFSEventStreamEventFlagItemIsDir = 0x00020000;
static const int kFSEventStreamEventFlagItemIsSymlink = 0x00040000;

#endif /* __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1070 */

#endif /* defined(__APPLE__) */

UV_UNUSED(static void uv__update_time(uv_loop_t* loop)) {
  /* Use a fast time source if available.  We only need millisecond precision.
   */
  loop->time = uv__hrtime(UV_CLOCK_FAST) / 1000000;
}

UV_UNUSED(static char* uv__basename_r(const char* path)) {
  char* s;

  s = strrchr(path, '/');
  if (s == NULL)
    return (char*) path;

  return s + 1;
}

#if defined(__linux__)
int uv__inotify_fork(uv_loop_t* loop, void* old_watchers);
#endif

#endif /* UV_UNIX_INTERNAL_H_ */
