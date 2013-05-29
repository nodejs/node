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
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h> /* INT_MAX, PATH_MAX */
#include <sys/uio.h> /* writev */

#ifdef __linux__
# include <sys/ioctl.h>
#endif

#ifdef __sun
# include <sys/types.h>
# include <sys/wait.h>
#endif

#ifdef __APPLE__
# include <mach-o/dyld.h> /* _NSGetExecutablePath */
# include <sys/filio.h>
# include <sys/ioctl.h>
#endif

#ifdef __FreeBSD__
# include <sys/sysctl.h>
# include <sys/filio.h>
# include <sys/ioctl.h>
# include <sys/wait.h>
#endif

static void uv__run_pending(uv_loop_t* loop);

static uv_loop_t default_loop_struct;
static uv_loop_t* default_loop_ptr;

/* Verify that uv_buf_t is ABI-compatible with struct iovec. */
STATIC_ASSERT(sizeof(uv_buf_t) == sizeof(struct iovec));
STATIC_ASSERT(sizeof(&((uv_buf_t*) 0)->base) ==
              sizeof(((struct iovec*) 0)->iov_base));
STATIC_ASSERT(sizeof(&((uv_buf_t*) 0)->len) ==
              sizeof(((struct iovec*) 0)->iov_len));
STATIC_ASSERT(offsetof(uv_buf_t, base) == offsetof(struct iovec, iov_base));
STATIC_ASSERT(offsetof(uv_buf_t, len) == offsetof(struct iovec, iov_len));


uint64_t uv_hrtime(void) {
  return uv__hrtime();
}


void uv_close(uv_handle_t* handle, uv_close_cb close_cb) {
  assert(!(handle->flags & (UV_CLOSING | UV_CLOSED)));

  handle->flags |= UV_CLOSING;
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
    break;

  case UV_SIGNAL:
    uv__signal_close((uv_signal_t*) handle);
    /* Signal handles may not be closed immediately. The signal code will */
    /* itself close uv__make_close_pending whenever appropriate. */
    return;

  default:
    assert(0);
  }

  uv__make_close_pending(handle);
}


void uv__make_close_pending(uv_handle_t* handle) {
  assert(handle->flags & UV_CLOSING);
  assert(!(handle->flags & UV_CLOSED));
  handle->next_closing = handle->loop->closing_handles;
  handle->loop->closing_handles = handle;
}


static void uv__finish_close(uv_handle_t* handle) {
  /* Note: while the handle is in the UV_CLOSING state now, it's still possible
   * for it to be active in the sense that uv__is_active() returns true.
   * A good example is when the user calls uv_shutdown(), immediately followed
   * by uv_close(). The handle is considered active at this point because the
   * completion of the shutdown req is still pending.
   */
  assert(handle->flags & UV_CLOSING);
  assert(!(handle->flags & UV_CLOSED));
  handle->flags |= UV_CLOSED;

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
    case UV_SIGNAL:
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


uv_loop_t* uv_default_loop(void) {
  if (default_loop_ptr)
    return default_loop_ptr;

  if (uv__loop_init(&default_loop_struct, /* default_loop? */ 1))
    return NULL;

  return (default_loop_ptr = &default_loop_struct);
}


uv_loop_t* uv_loop_new(void) {
  uv_loop_t* loop;

  if ((loop = malloc(sizeof(*loop))) == NULL)
    return NULL;

  if (uv__loop_init(loop, /* default_loop? */ 0)) {
    free(loop);
    return NULL;
  }

  return loop;
}


void uv_loop_delete(uv_loop_t* loop) {
  uv__loop_delete(loop);
#ifndef NDEBUG
  memset(loop, -1, sizeof *loop);
#endif
  if (loop == default_loop_ptr)
    default_loop_ptr = NULL;
  else
    free(loop);
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

  if (loop->closing_handles)
    return 0;

  return uv__next_timeout(loop);
}


static int uv__loop_alive(uv_loop_t* loop) {
  return uv__has_active_handles(loop) ||
         uv__has_active_reqs(loop) ||
         loop->closing_handles != NULL;
}


int uv_run(uv_loop_t* loop, uv_run_mode mode) {
  int timeout;
  int r;

  r = uv__loop_alive(loop);
  while (r != 0 && loop->stop_flag == 0) {
    UV_TICK_START(loop, mode);

    uv__update_time(loop);
    uv__run_timers(loop);
    uv__run_idle(loop);
    uv__run_prepare(loop);
    uv__run_pending(loop);

    timeout = 0;
    if ((mode & UV_RUN_NOWAIT) == 0)
      timeout = uv_backend_timeout(loop);

    uv__io_poll(loop, timeout);
    uv__run_check(loop);
    uv__run_closing_handles(loop);

    if (mode == UV_RUN_ONCE) {
      /* UV_RUN_ONCE implies forward progess: at least one callback must have
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
    UV_TICK_STOP(loop, mode);

    if (mode & (UV_RUN_ONCE | UV_RUN_NOWAIT))
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

#if defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
  sockfd = socket(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);

  if (sockfd != -1)
    goto out;

  if (errno != EINVAL)
    goto out;
#endif

  sockfd = socket(domain, type, protocol);

  if (sockfd == -1)
    goto out;

  if (uv__nonblock(sockfd, 1) || uv__cloexec(sockfd, 1)) {
    close(sockfd);
    sockfd = -1;
  }

#if defined(SO_NOSIGPIPE)
  {
    int on = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on));
  }
#endif

out:
  return sockfd;
}


int uv__accept(int sockfd) {
  int peerfd;

  assert(sockfd >= 0);

  while (1) {
#if defined(__linux__)
    static int no_accept4;

    if (no_accept4)
      goto skip;

    peerfd = uv__accept4(sockfd,
                         NULL,
                         NULL,
                         UV__SOCK_NONBLOCK|UV__SOCK_CLOEXEC);

    if (peerfd != -1)
      break;

    if (errno == EINTR)
      continue;

    if (errno != ENOSYS)
      break;

    no_accept4 = 1;
skip:
#endif

    peerfd = accept(sockfd, NULL, NULL);

    if (peerfd == -1) {
      if (errno == EINTR)
        continue;
      else
        break;
    }

    if (uv__cloexec(peerfd, 1) || uv__nonblock(peerfd, 1)) {
      close(peerfd);
      peerfd = -1;
    }

    break;
  }

  return peerfd;
}


#if defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)

int uv__nonblock(int fd, int set) {
  int r;

  do
    r = ioctl(fd, FIONBIO, &set);
  while (r == -1 && errno == EINTR);

  return r;
}


int uv__cloexec(int fd, int set) {
  int r;

  do
    r = ioctl(fd, set ? FIOCLEX : FIONCLEX);
  while (r == -1 && errno == EINTR);

  return r;
}

#else /* !(defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__)) */

int uv__nonblock(int fd, int set) {
  int flags;
  int r;

  do
    r = fcntl(fd, F_GETFL);
  while (r == -1 && errno == EINTR);

  if (r == -1)
    return -1;

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

  return r;
}


int uv__cloexec(int fd, int set) {
  int flags;
  int r;

  do
    r = fcntl(fd, F_GETFD);
  while (r == -1 && errno == EINTR);

  if (r == -1)
    return -1;

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

  return r;
}

#endif /* defined(__linux__) || defined(__FreeBSD__) || defined(__APPLE__) */


/* This function is not execve-safe, there is a race window
 * between the call to dup() and fcntl(FD_CLOEXEC).
 */
int uv__dup(int fd) {
  fd = dup(fd);

  if (fd == -1)
    return -1;

  if (uv__cloexec(fd, 1)) {
    SAVE_ERRNO(close(fd));
    return -1;
  }

  return fd;
}


uv_err_t uv_cwd(char* buffer, size_t size) {
  if (!buffer || !size) {
    return uv__new_artificial_error(UV_EINVAL);
  }

  if (getcwd(buffer, size)) {
    return uv_ok_;
  } else {
    return uv__new_sys_error(errno);
  }
}


uv_err_t uv_chdir(const char* dir) {
  if (chdir(dir) == 0) {
    return uv_ok_;
  } else {
    return uv__new_sys_error(errno);
  }
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


static void uv__run_pending(uv_loop_t* loop) {
  QUEUE* q;
  uv__io_t* w;

  while (!QUEUE_EMPTY(&loop->pending_queue)) {
    q = QUEUE_HEAD(&loop->pending_queue);
    QUEUE_REMOVE(q);
    QUEUE_INIT(q);

    w = QUEUE_DATA(q, uv__io_t, pending_queue);
    w->cb(loop, w, UV__POLLOUT);
  }
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
  unsigned int nwatchers;
  unsigned int i;

  if (len <= loop->nwatchers)
    return;

  nwatchers = next_power_of_two(len);
  watchers = realloc(loop->watchers, nwatchers * sizeof(loop->watchers[0]));

  if (watchers == NULL)
    abort();

  for (i = loop->nwatchers; i < nwatchers; i++)
    watchers[i] = NULL;

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
  assert(0 == (events & ~(UV__POLLIN | UV__POLLOUT)));
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
  if (w->events == w->pevents) {
    if (w->events == 0 && !QUEUE_EMPTY(&w->watcher_queue)) {
      QUEUE_REMOVE(&w->watcher_queue);
      QUEUE_INIT(&w->watcher_queue);
    }
    return;
  }
#endif

  if (QUEUE_EMPTY(&w->watcher_queue))
    QUEUE_INSERT_TAIL(&loop->watcher_queue, &w->watcher_queue);

  if (loop->watchers[w->fd] == NULL) {
    loop->watchers[w->fd] = w;
    loop->nfds++;
  }
}


void uv__io_stop(uv_loop_t* loop, uv__io_t* w, unsigned int events) {
  assert(0 == (events & ~(UV__POLLIN | UV__POLLOUT)));
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
  uv__io_stop(loop, w, UV__POLLIN | UV__POLLOUT);
  QUEUE_REMOVE(&w->pending_queue);
}


void uv__io_feed(uv_loop_t* loop, uv__io_t* w) {
  if (QUEUE_EMPTY(&w->pending_queue))
    QUEUE_INSERT_TAIL(&loop->pending_queue, &w->pending_queue);
}


int uv__io_active(const uv__io_t* w, unsigned int events) {
  assert(0 == (events & ~(UV__POLLIN | UV__POLLOUT)));
  assert(0 != events);
  return 0 != (w->pevents & events);
}
