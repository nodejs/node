/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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

#include "uv.h"
#include "internal.h"

#include <stddef.h> /* NULL */
#include <stdio.h> /* printf */
#include <stdlib.h>
#include <string.h> /* strerror */
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>  /* O_CLOEXEC */
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h> /* INT_MAX, PATH_MAX, IOV_MAX */
#include <sys/uio.h> /* writev */
#include <sys/resource.h> /* getrusage */
#include <pwd.h>
#include <sys/utsname.h>
#include <sys/time.h>

#ifdef __sun
# include <sys/filio.h>
# include <sys/types.h>
# include <sys/wait.h>
#endif

#if defined(__APPLE__)
# include <sys/filio.h>
# endif /* defined(__APPLE__) */


#if defined(__APPLE__) && !TARGET_OS_IPHONE
# include <crt_externs.h>
# include <mach-o/dyld.h> /* _NSGetExecutablePath */
# define environ (*_NSGetEnviron())
#else /* defined(__APPLE__) && !TARGET_OS_IPHONE */
extern char** environ;
#endif /* !(defined(__APPLE__) && !TARGET_OS_IPHONE) */


#if defined(__DragonFly__)      || \
    defined(__FreeBSD__)        || \
    defined(__FreeBSD_kernel__) || \
    defined(__NetBSD__)         || \
    defined(__OpenBSD__)
# include <sys/sysctl.h>
# include <sys/filio.h>
# include <sys/wait.h>
# if defined(__FreeBSD__)
#  define uv__accept4 accept4
# endif
# if defined(__NetBSD__)
#  define uv__accept4(a, b, c, d) paccept((a), (b), (c), NULL, (d))
# endif
#endif

#if defined(__MVS__)
#include <sys/ioctl.h>
#endif

#if defined(__linux__)
# include <sys/syscall.h>
# define uv__accept4 accept4
#endif

static int uv__run_pending(uv_loop_t* loop);

/* Verify that uv_buf_t is ABI-compatible with struct iovec. */
STATIC_ASSERT(sizeof(uv_buf_t) == sizeof(struct iovec));
STATIC_ASSERT(sizeof(&((uv_buf_t*) 0)->base) ==
              sizeof(((struct iovec*) 0)->iov_base));
STATIC_ASSERT(sizeof(&((uv_buf_t*) 0)->len) ==
              sizeof(((struct iovec*) 0)->iov_len));
STATIC_ASSERT(offsetof(uv_buf_t, base) == offsetof(struct iovec, iov_base));
STATIC_ASSERT(offsetof(uv_buf_t, len) == offsetof(struct iovec, iov_len));


uint64_t uv_hrtime(void) {
  return uv__hrtime(UV_CLOCK_PRECISE);
}


void uv_close(uv_handle_t* handle, uv_close_cb close_cb) {
  assert(!uv__is_closing(handle));

  handle->flags |= UV_HANDLE_CLOSING;
  handle->close_cb = close_cb;

  switch (handle->type) {
  case UV_NAMED_PIPE:
    uv__pipe_close((uv_pipe_t*)handle);
    break;

  case UV_TTY:
    uv__stream_close((uv_stream_t*)handle);
    break;

  case UV_TCP:
    uv__tcp_close((uv_tcp_t*)handle);
    break;

  case UV_UDP:
    uv__udp_close((uv_udp_t*)handle);
    break;

  case UV_PREPARE:
    uv__prepare_close((uv_prepare_t*)handle);
    break;

  case UV_CHECK:
    uv__check_close((uv_check_t*)handle);
    break;

  case UV_IDLE:
    uv__idle_close((uv_idle_t*)handle);
    break;

  case UV_ASYNC:
    uv__async_close((uv_async_t*)handle);
    break;

  case UV_TIMER:
    uv__timer_close((uv_timer_t*)handle);
    break;

  case UV_PROCESS:
    uv__process_close((uv_process_t*)handle);
    break;

  case UV_FS_EVENT:
    uv__fs_event_close((uv_fs_event_t*)handle);
    break;

  case UV_POLL:
    uv__poll_close((uv_poll_t*)handle);
    break;

  case UV_FS_POLL:
    uv__fs_poll_close((uv_fs_poll_t*)handle);
    /* Poll handles use file system requests, and one of them may still be
     * running. The poll code will call uv__make_close_pending() for us. */
    return;

  case UV_SIGNAL:
    uv__signal_close((uv_signal_t*) handle);
    break;

  default:
    assert(0);
  }

  uv__make_close_pending(handle);
}

int uv__socket_sockopt(uv_handle_t* handle, int optname, int* value) {
  int r;
  int fd;
  socklen_t len;

  if (handle == NULL || value == NULL)
    return UV_EINVAL;

  if (handle->type == UV_TCP || handle->type == UV_NAMED_PIPE)
    fd = uv__stream_fd((uv_stream_t*) handle);
  else if (handle->type == UV_UDP)
    fd = ((uv_udp_t *) handle)->io_watcher.fd;
  else
    return UV_ENOTSUP;

  len = sizeof(*value);

  if (*value == 0)
    r = getsockopt(fd, SOL_SOCKET, optname, value, &len);
  else
    r = setsockopt(fd, SOL_SOCKET, optname, (const void*) value, len);

  if (r < 0)
    return UV__ERR(errno);

  return 0;
}

void uv__make_close_pending(uv_handle_t* handle) {
  assert(handle->flags & UV_HANDLE_CLOSING);
  assert(!(handle->flags & UV_HANDLE_CLOSED));
  handle->next_closing = handle->loop->closing_handles;
  handle->loop->closing_handles = handle;
}

int uv__getiovmax(void) {
#if defined(IOV_MAX)
  return IOV_MAX;
#elif defined(_SC_IOV_MAX)
  static int iovmax_cached = -1;
  int iovmax;

  iovmax = uv__load_relaxed(&iovmax_cached);
  if (iovmax != -1)
    return iovmax;

  /* On some embedded devices (arm-linux-uclibc based ip camera),
   * sysconf(_SC_IOV_MAX) can not get the correct value. The return
   * value is -1 and the errno is EINPROGRESS. Degrade the value to 1.
   */
  iovmax = sysconf(_SC_IOV_MAX);
  if (iovmax == -1)
    iovmax = 1;

  uv__store_relaxed(&iovmax_cached, iovmax);

  return iovmax;
#else
  return 1024;
#endif
}


static void uv__finish_close(uv_handle_t* handle) {
  uv_signal_t* sh;

  /* Note: while the handle is in the UV_HANDLE_CLOSING state now, it's still
   * possible for it to be active in the sense that uv__is_active() returns
   * true.
   *
   * A good example is when the user calls uv_shutdown(), immediately followed
   * by uv_close(). The handle is considered active at this point because the
   * completion of the shutdown req is still pending.
   */
  assert(handle->flags & UV_HANDLE_CLOSING);
  assert(!(handle->flags & UV_HANDLE_CLOSED));
  handle->flags |= UV_HANDLE_CLOSED;

  switch (handle->type) {
    case UV_PREPARE:
    case UV_CHECK:
    case UV_IDLE:
    case UV_ASYNC:
    case UV_TIMER:
    case UV_PROCESS:
    case UV_FS_EVENT:
    case UV_FS_POLL:
    case UV_POLL:
      break;

    case UV_SIGNAL:
      /* If there are any caught signals "trapped" in the signal pipe,
       * we can't call the close callback yet. Reinserting the handle
       * into the closing queue makes the event loop spin but that's
       * okay because we only need to deliver the pending events.
       */
      sh = (uv_signal_t*) handle;
      if (sh->caught_signals > sh->dispatched_signals) {
        handle->flags ^= UV_HANDLE_CLOSED;
        uv__make_close_pending(handle);  /* Back into the queue. */
        return;
      }
      break;

    case UV_NAMED_PIPE:
    case UV_TCP:
    case UV_TTY:
      uv__stream_destroy((uv_stream_t*)handle);
      break;

    case UV_UDP:
      uv__udp_finish_close((uv_udp_t*)handle);
      break;

    default:
      assert(0);
      break;
  }

  uv__handle_unref(handle);
  QUEUE_REMOVE(&handle->handle_queue);

  if (handle->close_cb) {
    handle->close_cb(handle);
  }
}


static void uv__run_closing_handles(uv_loop_t* loop) {
  uv_handle_t* p;
  uv_handle_t* q;

  p = loop->closing_handles;
  loop->closing_handles = NULL;

  while (p) {
    q = p->next_closing;
    uv__finish_close(p);
    p = q;
  }
}


int uv_is_closing(const uv_handle_t* handle) {
  return uv__is_closing(handle);
}


int uv_backend_fd(const uv_loop_t* loop) {
  return loop->backend_fd;
}


int uv_backend_timeout(const uv_loop_t* loop) {
  if (loop->stop_flag != 0)
    return 0;

  if (!uv__has_active_handles(loop) && !uv__has_active_reqs(loop))
    return 0;

  if (!QUEUE_EMPTY(&loop->idle_handles))
    return 0;

  if (!QUEUE_EMPTY(&loop->pending_queue))
    return 0;

  if (loop->closing_handles)
    return 0;

  return uv__next_timeout(loop);
}


static int uv__loop_alive(const uv_loop_t* loop) {
  return uv__has_active_handles(loop) ||
         uv__has_active_reqs(loop) ||
         loop->closing_handles != NULL;
}


int uv_loop_alive(const uv_loop_t* loop) {
    return uv__loop_alive(loop);
}


int uv_run(uv_loop_t* loop, uv_run_mode mode) {
  int timeout;
  int r;
  int ran_pending;

  r = uv__loop_alive(loop);
  if (!r)
    uv__update_time(loop);

  while (r != 0 && loop->stop_flag == 0) {
    uv__update_time(loop);
    uv__run_timers(loop);
    ran_pending = uv__run_pending(loop);
    uv__run_idle(loop);
    uv__run_prepare(loop);

    timeout = 0;
    if ((mode == UV_RUN_ONCE && !ran_pending) || mode == UV_RUN_DEFAULT)
      timeout = uv_backend_timeout(loop);

    uv__io_poll(loop, timeout);

    /* Run one final update on the provider_idle_time in case uv__io_poll
     * returned because the timeout expired, but no events were received. This
     * call will be ignored if the provider_entry_time was either never set (if
     * the timeout == 0) or was already updated b/c an event was received.
     */
    uv__metrics_update_idle_time(loop);

    uv__run_check(loop);
    uv__run_closing_handles(loop);

    if (mode == UV_RUN_ONCE) {
      /* UV_RUN_ONCE implies forward progress: at least one callback must have
       * been invoked when it returns. uv__io_poll() can return without doing
       * I/O (meaning: no callbacks) when its timeout expires - which means we
       * have pending timers that satisfy the forward progress constraint.
       *
       * UV_RUN_NOWAIT makes no guarantees about progress so it's omitted from
       * the check.
       */
      uv__update_time(loop);
      uv__run_timers(loop);
    }

    r = uv__loop_alive(loop);
    if (mode == UV_RUN_ONCE || mode == UV_RUN_NOWAIT)
      break;
  }

  /* The if statement lets gcc compile it to a conditional store. Avoids
   * dirtying a cache line.
   */
  if (loop->stop_flag != 0)
    loop->stop_flag = 0;

  return r;
}


void uv_update_time(uv_loop_t* loop) {
  uv__update_time(loop);
}


int uv_is_active(const uv_handle_t* handle) {
  return uv__is_active(handle);
}


/* Open a socket in non-blocking close-on-exec mode, atomically if possible. */
int uv__socket(int domain, int type, int protocol) {
  int sockfd;
  int err;

#if defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
  sockfd = socket(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);
  if (sockfd != -1)
    return sockfd;

  if (errno != EINVAL)
    return UV__ERR(errno);
#endif

  sockfd = socket(domain, type, protocol);
  if (sockfd == -1)
    return UV__ERR(errno);

  err = uv__nonblock(sockfd, 1);
  if (err == 0)
    err = uv__cloexec(sockfd, 1);

  if (err) {
    uv__close(sockfd);
    return err;
  }

#if defined(SO_NOSIGPIPE)
  {
    int on = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
  }
#endif

  return sockfd;
}

/* get a file pointer to a file in read-only and close-on-exec mode */
FILE* uv__open_file(const char* path) {
  int fd;
  FILE* fp;

  fd = uv__open_cloexec(path, O_RDONLY);
  if (fd < 0)
    return NULL;

   fp = fdopen(fd, "r");
   if (fp == NULL)
     uv__close(fd);

   return fp;
}


int uv__accept(int sockfd) {
  int peerfd;
  int err;

  (void) &err;
  assert(sockfd >= 0);

  do
#ifdef uv__accept4
    peerfd = uv__accept4(sockfd, NULL, NULL, SOCK_NONBLOCK|SOCK_CLOEXEC);
#else
    peerfd = accept(sockfd, NULL, NULL);
#endif
  while (peerfd == -1 && errno == EINTR);

  if (peerfd == -1)
    return UV__ERR(errno);

#ifndef uv__accept4
  err = uv__cloexec(peerfd, 1);
  if (err == 0)
    err = uv__nonblock(peerfd, 1);

  if (err != 0) {
    uv__close(peerfd);
    return err;
  }
#endif

  return peerfd;
}


/* close() on macos has the "interesting" quirk that it fails with EINTR
 * without closing the file descriptor when a thread is in the cancel state.
 * That's why libuv calls close$NOCANCEL() instead.
 *
 * glibc on linux has a similar issue: close() is a cancellation point and
 * will unwind the thread when it's in the cancel state. Work around that
 * by making the system call directly. Musl libc is unaffected.
 */
int uv__close_nocancel(int fd) {
#if defined(__APPLE__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdollar-in-identifier-extension"
#if defined(__LP64__) || TARGET_OS_IPHONE
  extern int close$NOCANCEL(int);
  return close$NOCANCEL(fd);
#else
  extern int close$NOCANCEL$UNIX2003(int);
  return close$NOCANCEL$UNIX2003(fd);
#endif
#pragma GCC diagnostic pop
#elif defined(__linux__)
  return syscall(SYS_close, fd);
#else
  return close(fd);
#endif
}


int uv__close_nocheckstdio(int fd) {
  int saved_errno;
  int rc;

  assert(fd > -1);  /* Catch uninitialized io_watcher.fd bugs. */

  saved_errno = errno;
  rc = uv__close_nocancel(fd);
  if (rc == -1) {
    rc = UV__ERR(errno);
    if (rc == UV_EINTR || rc == UV__ERR(EINPROGRESS))
      rc = 0;    /* The close is in progress, not an error. */
    errno = saved_errno;
  }

  return rc;
}


int uv__close(int fd) {
  assert(fd > STDERR_FILENO);  /* Catch stdio close bugs. */
#if defined(__MVS__)
  SAVE_ERRNO(epoll_file_close(fd));
#endif
  return uv__close_nocheckstdio(fd);
}


int uv__nonblock_ioctl(int fd, int set) {
  int r;

  do
    r = ioctl(fd, FIONBIO, &set);
  while (r == -1 && errno == EINTR);

  if (r)
    return UV__ERR(errno);

  return 0;
}


#if !defined(__CYGWIN__) && !defined(__MSYS__) && !defined(__HAIKU__)
int uv__cloexec_ioctl(int fd, int set) {
  int r;

  do
    r = ioctl(fd, set ? FIOCLEX : FIONCLEX);
  while (r == -1 && errno == EINTR);

  if (r)
    return UV__ERR(errno);

  return 0;
}
#endif


int uv__nonblock_fcntl(int fd, int set) {
  int flags;
  int r;

  do
    r = fcntl(fd, F_GETFL);
  while (r == -1 && errno == EINTR);

  if (r == -1)
    return UV__ERR(errno);

  /* Bail out now if already set/clear. */
  if (!!(r & O_NONBLOCK) == !!set)
    return 0;

  if (set)
    flags = r | O_NONBLOCK;
  else
    flags = r & ~O_NONBLOCK;

  do
    r = fcntl(fd, F_SETFL, flags);
  while (r == -1 && errno == EINTR);

  if (r)
    return UV__ERR(errno);

  return 0;
}


int uv__cloexec_fcntl(int fd, int set) {
  int flags;
  int r;

  do
    r = fcntl(fd, F_GETFD);
  while (r == -1 && errno == EINTR);

  if (r == -1)
    return UV__ERR(errno);

  /* Bail out now if already set/clear. */
  if (!!(r & FD_CLOEXEC) == !!set)
    return 0;

  if (set)
    flags = r | FD_CLOEXEC;
  else
    flags = r & ~FD_CLOEXEC;

  do
    r = fcntl(fd, F_SETFD, flags);
  while (r == -1 && errno == EINTR);

  if (r)
    return UV__ERR(errno);

  return 0;
}


ssize_t uv__recvmsg(int fd, struct msghdr* msg, int flags) {
  struct cmsghdr* cmsg;
  ssize_t rc;
  int* pfd;
  int* end;
#if defined(__linux__)
  static int no_msg_cmsg_cloexec;
  if (0 == uv__load_relaxed(&no_msg_cmsg_cloexec)) {
    rc = recvmsg(fd, msg, flags | 0x40000000);  /* MSG_CMSG_CLOEXEC */
    if (rc != -1)
      return rc;
    if (errno != EINVAL)
      return UV__ERR(errno);
    rc = recvmsg(fd, msg, flags);
    if (rc == -1)
      return UV__ERR(errno);
    uv__store_relaxed(&no_msg_cmsg_cloexec, 1);
  } else {
    rc = recvmsg(fd, msg, flags);
  }
#else
  rc = recvmsg(fd, msg, flags);
#endif
  if (rc == -1)
    return UV__ERR(errno);
  if (msg->msg_controllen == 0)
    return rc;
  for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL; cmsg = CMSG_NXTHDR(msg, cmsg))
    if (cmsg->cmsg_type == SCM_RIGHTS)
      for (pfd = (int*) CMSG_DATA(cmsg),
           end = (int*) ((char*) cmsg + cmsg->cmsg_len);
           pfd < end;
           pfd += 1)
        uv__cloexec(*pfd, 1);
  return rc;
}


int uv_cwd(char* buffer, size_t* size) {
  char scratch[1 + UV__PATH_MAX];

  if (buffer == NULL || size == NULL)
    return UV_EINVAL;

  /* Try to read directly into the user's buffer first... */
  if (getcwd(buffer, *size) != NULL)
    goto fixup;

  if (errno != ERANGE)
    return UV__ERR(errno);

  /* ...or into scratch space if the user's buffer is too small
   * so we can report how much space to provide on the next try.
   */
  if (getcwd(scratch, sizeof(scratch)) == NULL)
    return UV__ERR(errno);

  buffer = scratch;

fixup:

  *size = strlen(buffer);

  if (*size > 1 && buffer[*size - 1] == '/') {
    *size -= 1;
    buffer[*size] = '\0';
  }

  if (buffer == scratch) {
    *size += 1;
    return UV_ENOBUFS;
  }

  return 0;
}


int uv_chdir(const char* dir) {
  if (chdir(dir))
    return UV__ERR(errno);

  return 0;
}


void uv_disable_stdio_inheritance(void) {
  int fd;

  /* Set the CLOEXEC flag on all open descriptors. Unconditionally try the
   * first 16 file descriptors. After that, bail out after the first error.
   */
  for (fd = 0; ; fd++)
    if (uv__cloexec(fd, 1) && fd > 15)
      break;
}


int uv_fileno(const uv_handle_t* handle, uv_os_fd_t* fd) {
  int fd_out;

  switch (handle->type) {
  case UV_TCP:
  case UV_NAMED_PIPE:
  case UV_TTY:
    fd_out = uv__stream_fd((uv_stream_t*) handle);
    break;

  case UV_UDP:
    fd_out = ((uv_udp_t *) handle)->io_watcher.fd;
    break;

  case UV_POLL:
    fd_out = ((uv_poll_t *) handle)->io_watcher.fd;
    break;

  default:
    return UV_EINVAL;
  }

  if (uv__is_closing(handle) || fd_out == -1)
    return UV_EBADF;

  *fd = fd_out;
  return 0;
}


static int uv__run_pending(uv_loop_t* loop) {
  QUEUE* q;
  QUEUE pq;
  uv__io_t* w;

  if (QUEUE_EMPTY(&loop->pending_queue))
    return 0;

  QUEUE_MOVE(&loop->pending_queue, &pq);

  while (!QUEUE_EMPTY(&pq)) {
    q = QUEUE_HEAD(&pq);
    QUEUE_REMOVE(q);
    QUEUE_INIT(q);
    w = QUEUE_DATA(q, uv__io_t, pending_queue);
    w->cb(loop, w, POLLOUT);
  }

  return 1;
}


static unsigned int next_power_of_two(unsigned int val) {
  val -= 1;
  val |= val >> 1;
  val |= val >> 2;
  val |= val >> 4;
  val |= val >> 8;
  val |= val >> 16;
  val += 1;
  return val;
}

static void maybe_resize(uv_loop_t* loop, unsigned int len) {
  uv__io_t** watchers;
  void* fake_watcher_list;
  void* fake_watcher_count;
  unsigned int nwatchers;
  unsigned int i;

  if (len <= loop->nwatchers)
    return;

  /* Preserve fake watcher list and count at the end of the watchers */
  if (loop->watchers != NULL) {
    fake_watcher_list = loop->watchers[loop->nwatchers];
    fake_watcher_count = loop->watchers[loop->nwatchers + 1];
  } else {
    fake_watcher_list = NULL;
    fake_watcher_count = NULL;
  }

  nwatchers = next_power_of_two(len + 2) - 2;
  watchers = uv__reallocf(loop->watchers,
                          (nwatchers + 2) * sizeof(loop->watchers[0]));

  if (watchers == NULL)
    abort();
  for (i = loop->nwatchers; i < nwatchers; i++)
    watchers[i] = NULL;
  watchers[nwatchers] = fake_watcher_list;
  watchers[nwatchers + 1] = fake_watcher_count;

  loop->watchers = watchers;
  loop->nwatchers = nwatchers;
}


void uv__io_init(uv__io_t* w, uv__io_cb cb, int fd) {
  assert(cb != NULL);
  assert(fd >= -1);
  QUEUE_INIT(&w->pending_queue);
  QUEUE_INIT(&w->watcher_queue);
  w->cb = cb;
  w->fd = fd;
  w->events = 0;
  w->pevents = 0;

#if defined(UV_HAVE_KQUEUE)
  w->rcount = 0;
  w->wcount = 0;
#endif /* defined(UV_HAVE_KQUEUE) */
}


void uv__io_start(uv_loop_t* loop, uv__io_t* w, unsigned int events) {
  assert(0 == (events & ~(POLLIN | POLLOUT | UV__POLLRDHUP | UV__POLLPRI)));
  assert(0 != events);
  assert(w->fd >= 0);
  assert(w->fd < INT_MAX);

  w->pevents |= events;
  maybe_resize(loop, w->fd + 1);

#if !defined(__sun)
  /* The event ports backend needs to rearm all file descriptors on each and
   * every tick of the event loop but the other backends allow us to
   * short-circuit here if the event mask is unchanged.
   */
  if (w->events == w->pevents)
    return;
#endif

  if (QUEUE_EMPTY(&w->watcher_queue))
    QUEUE_INSERT_TAIL(&loop->watcher_queue, &w->watcher_queue);

  if (loop->watchers[w->fd] == NULL) {
    loop->watchers[w->fd] = w;
    loop->nfds++;
  }
}


void uv__io_stop(uv_loop_t* loop, uv__io_t* w, unsigned int events) {
  assert(0 == (events & ~(POLLIN | POLLOUT | UV__POLLRDHUP | UV__POLLPRI)));
  assert(0 != events);

  if (w->fd == -1)
    return;

  assert(w->fd >= 0);

  /* Happens when uv__io_stop() is called on a handle that was never started. */
  if ((unsigned) w->fd >= loop->nwatchers)
    return;

  w->pevents &= ~events;

  if (w->pevents == 0) {
    QUEUE_REMOVE(&w->watcher_queue);
    QUEUE_INIT(&w->watcher_queue);

    if (loop->watchers[w->fd] != NULL) {
      assert(loop->watchers[w->fd] == w);
      assert(loop->nfds > 0);
      loop->watchers[w->fd] = NULL;
      loop->nfds--;
      w->events = 0;
    }
  }
  else if (QUEUE_EMPTY(&w->watcher_queue))
    QUEUE_INSERT_TAIL(&loop->watcher_queue, &w->watcher_queue);
}


void uv__io_close(uv_loop_t* loop, uv__io_t* w) {
  uv__io_stop(loop, w, POLLIN | POLLOUT | UV__POLLRDHUP | UV__POLLPRI);
  QUEUE_REMOVE(&w->pending_queue);

  /* Remove stale events for this file descriptor */
  if (w->fd != -1)
    uv__platform_invalidate_fd(loop, w->fd);
}


void uv__io_feed(uv_loop_t* loop, uv__io_t* w) {
  if (QUEUE_EMPTY(&w->pending_queue))
    QUEUE_INSERT_TAIL(&loop->pending_queue, &w->pending_queue);
}


int uv__io_active(const uv__io_t* w, unsigned int events) {
  assert(0 == (events & ~(POLLIN | POLLOUT | UV__POLLRDHUP | UV__POLLPRI)));
  assert(0 != events);
  return 0 != (w->pevents & events);
}


int uv__fd_exists(uv_loop_t* loop, int fd) {
  return (unsigned) fd < loop->nwatchers && loop->watchers[fd] != NULL;
}


int uv_getrusage(uv_rusage_t* rusage) {
  struct rusage usage;

  if (getrusage(RUSAGE_SELF, &usage))
    return UV__ERR(errno);

  rusage->ru_utime.tv_sec = usage.ru_utime.tv_sec;
  rusage->ru_utime.tv_usec = usage.ru_utime.tv_usec;

  rusage->ru_stime.tv_sec = usage.ru_stime.tv_sec;
  rusage->ru_stime.tv_usec = usage.ru_stime.tv_usec;

#if !defined(__MVS__) && !defined(__HAIKU__)
  rusage->ru_maxrss = usage.ru_maxrss;
  rusage->ru_ixrss = usage.ru_ixrss;
  rusage->ru_idrss = usage.ru_idrss;
  rusage->ru_isrss = usage.ru_isrss;
  rusage->ru_minflt = usage.ru_minflt;
  rusage->ru_majflt = usage.ru_majflt;
  rusage->ru_nswap = usage.ru_nswap;
  rusage->ru_inblock = usage.ru_inblock;
  rusage->ru_oublock = usage.ru_oublock;
  rusage->ru_msgsnd = usage.ru_msgsnd;
  rusage->ru_msgrcv = usage.ru_msgrcv;
  rusage->ru_nsignals = usage.ru_nsignals;
  rusage->ru_nvcsw = usage.ru_nvcsw;
  rusage->ru_nivcsw = usage.ru_nivcsw;
#endif

  return 0;
}


int uv__open_cloexec(const char* path, int flags) {
#if defined(O_CLOEXEC)
  int fd;

  fd = open(path, flags | O_CLOEXEC);
  if (fd == -1)
    return UV__ERR(errno);

  return fd;
#else  /* O_CLOEXEC */
  int err;
  int fd;

  fd = open(path, flags);
  if (fd == -1)
    return UV__ERR(errno);

  err = uv__cloexec(fd, 1);
  if (err) {
    uv__close(fd);
    return err;
  }

  return fd;
#endif  /* O_CLOEXEC */
}


int uv__dup2_cloexec(int oldfd, int newfd) {
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__linux__)
  int r;

  r = dup3(oldfd, newfd, O_CLOEXEC);
  if (r == -1)
    return UV__ERR(errno);

  return r;
#else
  int err;
  int r;

  r = dup2(oldfd, newfd);  /* Never retry. */
  if (r == -1)
    return UV__ERR(errno);

  err = uv__cloexec(newfd, 1);
  if (err != 0) {
    uv__close(newfd);
    return err;
  }

  return r;
#endif
}


int uv_os_homedir(char* buffer, size_t* size) {
  uv_passwd_t pwd;
  size_t len;
  int r;

  /* Check if the HOME environment variable is set first. The task of
     performing input validation on buffer and size is taken care of by
     uv_os_getenv(). */
  r = uv_os_getenv("HOME", buffer, size);

  if (r != UV_ENOENT)
    return r;

  /* HOME is not set, so call uv__getpwuid_r() */
  r = uv__getpwuid_r(&pwd);

  if (r != 0) {
    return r;
  }

  len = strlen(pwd.homedir);

  if (len >= *size) {
    *size = len + 1;
    uv_os_free_passwd(&pwd);
    return UV_ENOBUFS;
  }

  memcpy(buffer, pwd.homedir, len + 1);
  *size = len;
  uv_os_free_passwd(&pwd);

  return 0;
}


int uv_os_tmpdir(char* buffer, size_t* size) {
  const char* buf;
  size_t len;

  if (buffer == NULL || size == NULL || *size == 0)
    return UV_EINVAL;

#define CHECK_ENV_VAR(name)                                                   \
  do {                                                                        \
    buf = getenv(name);                                                       \
    if (buf != NULL)                                                          \
      goto return_buffer;                                                     \
  }                                                                           \
  while (0)

  /* Check the TMPDIR, TMP, TEMP, and TEMPDIR environment variables in order */
  CHECK_ENV_VAR("TMPDIR");
  CHECK_ENV_VAR("TMP");
  CHECK_ENV_VAR("TEMP");
  CHECK_ENV_VAR("TEMPDIR");

#undef CHECK_ENV_VAR

  /* No temp environment variables defined */
  #if defined(__ANDROID__)
    buf = "/data/local/tmp";
  #else
    buf = "/tmp";
  #endif

return_buffer:
  len = strlen(buf);

  if (len >= *size) {
    *size = len + 1;
    return UV_ENOBUFS;
  }

  /* The returned directory should not have a trailing slash. */
  if (len > 1 && buf[len - 1] == '/') {
    len--;
  }

  memcpy(buffer, buf, len + 1);
  buffer[len] = '\0';
  *size = len;

  return 0;
}


int uv__getpwuid_r(uv_passwd_t* pwd) {
  struct passwd pw;
  struct passwd* result;
  char* buf;
  uid_t uid;
  size_t bufsize;
  size_t name_size;
  size_t homedir_size;
  size_t shell_size;
  long initsize;
  int r;

  if (pwd == NULL)
    return UV_EINVAL;

  initsize = sysconf(_SC_GETPW_R_SIZE_MAX);

  if (initsize <= 0)
    bufsize = 4096;
  else
    bufsize = (size_t) initsize;

  uid = geteuid();
  buf = NULL;

  for (;;) {
    uv__free(buf);
    buf = uv__malloc(bufsize);

    if (buf == NULL)
      return UV_ENOMEM;

    r = getpwuid_r(uid, &pw, buf, bufsize, &result);

    if (r != ERANGE)
      break;

    bufsize *= 2;
  }

  if (r != 0) {
    uv__free(buf);
    return -r;
  }

  if (result == NULL) {
    uv__free(buf);
    return UV_ENOENT;
  }

  /* Allocate memory for the username, shell, and home directory */
  name_size = strlen(pw.pw_name) + 1;
  homedir_size = strlen(pw.pw_dir) + 1;
  shell_size = strlen(pw.pw_shell) + 1;
  pwd->username = uv__malloc(name_size + homedir_size + shell_size);

  if (pwd->username == NULL) {
    uv__free(buf);
    return UV_ENOMEM;
  }

  /* Copy the username */
  memcpy(pwd->username, pw.pw_name, name_size);

  /* Copy the home directory */
  pwd->homedir = pwd->username + name_size;
  memcpy(pwd->homedir, pw.pw_dir, homedir_size);

  /* Copy the shell */
  pwd->shell = pwd->homedir + homedir_size;
  memcpy(pwd->shell, pw.pw_shell, shell_size);

  /* Copy the uid and gid */
  pwd->uid = pw.pw_uid;
  pwd->gid = pw.pw_gid;

  uv__free(buf);

  return 0;
}


void uv_os_free_passwd(uv_passwd_t* pwd) {
  if (pwd == NULL)
    return;

  /*
    The memory for name, shell, and homedir are allocated in a single
    uv__malloc() call. The base of the pointer is stored in pwd->username, so
    that is the field that needs to be freed.
  */
  uv__free(pwd->username);
  pwd->username = NULL;
  pwd->shell = NULL;
  pwd->homedir = NULL;
}


int uv_os_get_passwd(uv_passwd_t* pwd) {
  return uv__getpwuid_r(pwd);
}


int uv_translate_sys_error(int sys_errno) {
  /* If < 0 then it's already a libuv error. */
  return sys_errno <= 0 ? sys_errno : -sys_errno;
}


int uv_os_environ(uv_env_item_t** envitems, int* count) {
  int i, j, cnt;
  uv_env_item_t* envitem;

  *envitems = NULL;
  *count = 0;

  for (i = 0; environ[i] != NULL; i++);

  *envitems = uv__calloc(i, sizeof(**envitems));

  if (*envitems == NULL)
    return UV_ENOMEM;

  for (j = 0, cnt = 0; j < i; j++) {
    char* buf;
    char* ptr;

    if (environ[j] == NULL)
      break;

    buf = uv__strdup(environ[j]);
    if (buf == NULL)
      goto fail;

    ptr = strchr(buf, '=');
    if (ptr == NULL) {
      uv__free(buf);
      continue;
    }

    *ptr = '\0';

    envitem = &(*envitems)[cnt];
    envitem->name = buf;
    envitem->value = ptr + 1;

    cnt++;
  }

  *count = cnt;
  return 0;

fail:
  for (i = 0; i < cnt; i++) {
    envitem = &(*envitems)[cnt];
    uv__free(envitem->name);
  }
  uv__free(*envitems);

  *envitems = NULL;
  *count = 0;
  return UV_ENOMEM;
}


int uv_os_getenv(const char* name, char* buffer, size_t* size) {
  char* var;
  size_t len;

  if (name == NULL || buffer == NULL || size == NULL || *size == 0)
    return UV_EINVAL;

  var = getenv(name);

  if (var == NULL)
    return UV_ENOENT;

  len = strlen(var);

  if (len >= *size) {
    *size = len + 1;
    return UV_ENOBUFS;
  }

  memcpy(buffer, var, len + 1);
  *size = len;

  return 0;
}


int uv_os_setenv(const char* name, const char* value) {
  if (name == NULL || value == NULL)
    return UV_EINVAL;

  if (setenv(name, value, 1) != 0)
    return UV__ERR(errno);

  return 0;
}


int uv_os_unsetenv(const char* name) {
  if (name == NULL)
    return UV_EINVAL;

  if (unsetenv(name) != 0)
    return UV__ERR(errno);

  return 0;
}


int uv_os_gethostname(char* buffer, size_t* size) {
  /*
    On some platforms, if the input buffer is not large enough, gethostname()
    succeeds, but truncates the result. libuv can detect this and return ENOBUFS
    instead by creating a large enough buffer and comparing the hostname length
    to the size input.
  */
  char buf[UV_MAXHOSTNAMESIZE];
  size_t len;

  if (buffer == NULL || size == NULL || *size == 0)
    return UV_EINVAL;

  if (gethostname(buf, sizeof(buf)) != 0)
    return UV__ERR(errno);

  buf[sizeof(buf) - 1] = '\0'; /* Null terminate, just to be safe. */
  len = strlen(buf);

  if (len >= *size) {
    *size = len + 1;
    return UV_ENOBUFS;
  }

  memcpy(buffer, buf, len + 1);
  *size = len;
  return 0;
}


uv_os_fd_t uv_get_osfhandle(int fd) {
  return fd;
}

int uv_open_osfhandle(uv_os_fd_t os_fd) {
  return os_fd;
}

uv_pid_t uv_os_getpid(void) {
  return getpid();
}


uv_pid_t uv_os_getppid(void) {
  return getppid();
}


int uv_os_getpriority(uv_pid_t pid, int* priority) {
  int r;

  if (priority == NULL)
    return UV_EINVAL;

  errno = 0;
  r = getpriority(PRIO_PROCESS, (int) pid);

  if (r == -1 && errno != 0)
    return UV__ERR(errno);

  *priority = r;
  return 0;
}


int uv_os_setpriority(uv_pid_t pid, int priority) {
  if (priority < UV_PRIORITY_HIGHEST || priority > UV_PRIORITY_LOW)
    return UV_EINVAL;

  if (setpriority(PRIO_PROCESS, (int) pid, priority) != 0)
    return UV__ERR(errno);

  return 0;
}


int uv_os_uname(uv_utsname_t* buffer) {
  struct utsname buf;
  int r;

  if (buffer == NULL)
    return UV_EINVAL;

  if (uname(&buf) == -1) {
    r = UV__ERR(errno);
    goto error;
  }

  r = uv__strscpy(buffer->sysname, buf.sysname, sizeof(buffer->sysname));
  if (r == UV_E2BIG)
    goto error;

#ifdef _AIX
  r = snprintf(buffer->release,
               sizeof(buffer->release),
               "%s.%s",
               buf.version,
               buf.release);
  if (r >= sizeof(buffer->release)) {
    r = UV_E2BIG;
    goto error;
  }
#else
  r = uv__strscpy(buffer->release, buf.release, sizeof(buffer->release));
  if (r == UV_E2BIG)
    goto error;
#endif

  r = uv__strscpy(buffer->version, buf.version, sizeof(buffer->version));
  if (r == UV_E2BIG)
    goto error;

#if defined(_AIX) || defined(__PASE__)
  r = uv__strscpy(buffer->machine, "ppc64", sizeof(buffer->machine));
#else
  r = uv__strscpy(buffer->machine, buf.machine, sizeof(buffer->machine));
#endif

  if (r == UV_E2BIG)
    goto error;

  return 0;

error:
  buffer->sysname[0] = '\0';
  buffer->release[0] = '\0';
  buffer->version[0] = '\0';
  buffer->machine[0] = '\0';
  return r;
}

int uv__getsockpeername(const uv_handle_t* handle,
                        uv__peersockfunc func,
                        struct sockaddr* name,
                        int* namelen) {
  socklen_t socklen;
  uv_os_fd_t fd;
  int r;

  r = uv_fileno(handle, &fd);
  if (r < 0)
    return r;

  /* sizeof(socklen_t) != sizeof(int) on some systems. */
  socklen = (socklen_t) *namelen;

  if (func(fd, name, &socklen))
    return UV__ERR(errno);

  *namelen = (int) socklen;
  return 0;
}

int uv_gettimeofday(uv_timeval64_t* tv) {
  struct timeval time;

  if (tv == NULL)
    return UV_EINVAL;

  if (gettimeofday(&time, NULL) != 0)
    return UV__ERR(errno);

  tv->tv_sec = (int64_t) time.tv_sec;
  tv->tv_usec = (int32_t) time.tv_usec;
  return 0;
}

void uv_sleep(unsigned int msec) {
  struct timespec timeout;
  int rc;

  timeout.tv_sec = msec / 1000;
  timeout.tv_nsec = (msec % 1000) * 1000 * 1000;

  do
    rc = nanosleep(&timeout, &timeout);
  while (rc == -1 && errno == EINTR);

  assert(rc == 0);
}

int uv__search_path(const char* prog, char* buf, size_t* buflen) {
  char abspath[UV__PATH_MAX];
  size_t abspath_size;
  char trypath[UV__PATH_MAX];
  char* cloned_path;
  char* path_env;
  char* token;

  if (buf == NULL || buflen == NULL || *buflen == 0)
    return UV_EINVAL;

  /*
   * Possibilities for prog:
   * i) an absolute path such as: /home/user/myprojects/nodejs/node
   * ii) a relative path such as: ./node or ../myprojects/nodejs/node
   * iii) a bare filename such as "node", after exporting PATH variable
   *     to its location.
   */

  /* Case i) and ii) absolute or relative paths */
  if (strchr(prog, '/') != NULL) {
    if (realpath(prog, abspath) != abspath)
      return UV__ERR(errno);

    abspath_size = strlen(abspath);

    *buflen -= 1;
    if (*buflen > abspath_size)
      *buflen = abspath_size;

    memcpy(buf, abspath, *buflen);
    buf[*buflen] = '\0';

    return 0;
  } 

  /* Case iii). Search PATH environment variable */
  cloned_path = NULL;
  token = NULL;
  path_env = getenv("PATH");

  if (path_env == NULL)
    return UV_EINVAL;

  cloned_path = uv__strdup(path_env);
  if (cloned_path == NULL)
    return UV_ENOMEM;

  token = strtok(cloned_path, ":");
  while (token != NULL) {
    snprintf(trypath, sizeof(trypath) - 1, "%s/%s", token, prog);
    if (realpath(trypath, abspath) == abspath) {
      /* Check the match is executable */
      if (access(abspath, X_OK) == 0) {
        abspath_size = strlen(abspath);

        *buflen -= 1;
        if (*buflen > abspath_size)
          *buflen = abspath_size;

        memcpy(buf, abspath, *buflen);
        buf[*buflen] = '\0';

        uv__free(cloned_path);
        return 0;
      }
    }
    token = strtok(NULL, ":");
  }
  uv__free(cloned_path);

  /* Out of tokens (path entries), and no match found */
  return UV_EINVAL;
}
