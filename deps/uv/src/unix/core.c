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
#include <limits.h> /* PATH_MAX */
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
#endif

#ifdef __FreeBSD__
# include <sys/sysctl.h>
# include <sys/wait.h>
#endif

static uv_loop_t default_loop_struct;
static uv_loop_t* default_loop_ptr;


void uv_close(uv_handle_t* handle, uv_close_cb close_cb) {
  handle->close_cb = close_cb;

  switch (handle->type) {
  case UV_NAMED_PIPE:
    uv__pipe_close((uv_pipe_t*)handle);
    break;

  case UV_TTY:
  case UV_TCP:
    uv__stream_close((uv_stream_t*)handle);
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

  default:
    assert(0);
  }

  handle->flags |= UV_CLOSING;

  handle->next_closing = handle->loop->closing_handles;
  handle->loop->closing_handles = handle;
}


static void uv__finish_close(uv_handle_t* handle) {
  assert(!uv__is_active(handle));
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
      break;

    case UV_NAMED_PIPE:
    case UV_TCP:
    case UV_TTY:
      assert(!uv__io_active(&((uv_stream_t*)handle)->read_watcher));
      assert(!uv__io_active(&((uv_stream_t*)handle)->write_watcher));
      assert(((uv_stream_t*)handle)->fd == -1);
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
  ngx_queue_remove(&handle->handle_queue);

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
  return handle->flags & (UV_CLOSING | UV_CLOSED);
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


static unsigned int uv__poll_timeout(uv_loop_t* loop) {
  if (!uv__has_active_handles(loop))
    return 0;

  if (!ngx_queue_empty(&loop->idle_handles))
    return 0;

  if (loop->closing_handles)
    return 0;

  return uv__next_timeout(loop);
}


static void uv__poll(uv_loop_t* loop) {
  void ev__run(EV_P_ ev_tstamp waittime);
  ev_invoke_pending(loop->ev);
  ev__run(loop->ev, uv__poll_timeout(loop) / 1000.);
  ev_invoke_pending(loop->ev);
}


static int uv__run(uv_loop_t* loop) {
  uv_update_time(loop);
  uv__run_timers(loop);
  uv__run_idle(loop);
  uv__run_prepare(loop);
  uv__poll(loop);
  uv__run_check(loop);
  uv__run_closing_handles(loop);
  return uv__has_active_handles(loop) || uv__has_active_reqs(loop);
}


int uv_run(uv_loop_t* loop) {
  while (uv__run(loop));
  return 0;
}


int uv_run_once(uv_loop_t* loop) {
  return uv__run(loop);
}


void uv_update_time(uv_loop_t* loop) {
  loop->time = uv_hrtime() / 1000000;
}


int64_t uv_now(uv_loop_t* loop) {
  return loop->time;
}


int uv_is_active(const uv_handle_t* handle) {
  return uv__is_active(handle);
}


static int uv_getaddrinfo_done(eio_req* req_) {
  uv_getaddrinfo_t* req = req_->data;
  struct addrinfo *res = req->res;
#if __sun
  size_t hostlen = strlen(req->hostname);
#endif

  req->res = NULL;

  uv__req_unregister(req->loop, req);

  /* see initialization in uv_getaddrinfo() */
  if (req->hints)
    free(req->hints);
  else if (req->service)
    free(req->service);
  else if (req->hostname)
    free(req->hostname);
  else
    assert(0);

  if (req->retcode == 0) {
    /* OK */
#if EAI_NODATA /* FreeBSD deprecated EAI_NODATA */
  } else if (req->retcode == EAI_NONAME || req->retcode == EAI_NODATA) {
#else
  } else if (req->retcode == EAI_NONAME) {
#endif
    uv__set_sys_error(req->loop, ENOENT); /* FIXME compatibility hack */
#if __sun
  } else if (req->retcode == EAI_MEMORY && hostlen >= MAXHOSTNAMELEN) {
    uv__set_sys_error(req->loop, ENOENT);
#endif
  } else {
    req->loop->last_err.code = UV_EADDRINFO;
    req->loop->last_err.sys_errno_ = req->retcode;
  }

  req->cb(req, req->retcode, res);

  return 0;
}


static void getaddrinfo_thread_proc(eio_req *req_) {
  uv_getaddrinfo_t* req = req_->data;

  req->retcode = getaddrinfo(req->hostname,
                             req->service,
                             req->hints,
                             &req->res);
}


int uv_getaddrinfo(uv_loop_t* loop,
                   uv_getaddrinfo_t* req,
                   uv_getaddrinfo_cb cb,
                   const char* hostname,
                   const char* service,
                   const struct addrinfo* hints) {
  size_t hostname_len;
  size_t service_len;
  size_t hints_len;
  eio_req* req_;
  size_t len;
  char* buf;

  if (req == NULL || cb == NULL || (hostname == NULL && service == NULL))
    return uv__set_artificial_error(loop, UV_EINVAL);

  uv_eio_init(loop);

  hostname_len = hostname ? strlen(hostname) + 1 : 0;
  service_len = service ? strlen(service) + 1 : 0;
  hints_len = hints ? sizeof(*hints) : 0;
  buf = malloc(hostname_len + service_len + hints_len);

  if (buf == NULL)
    return uv__set_artificial_error(loop, UV_ENOMEM);

  uv__req_init(loop, req, UV_GETADDRINFO);
  req->loop = loop;
  req->cb = cb;
  req->res = NULL;
  req->hints = NULL;
  req->service = NULL;
  req->hostname = NULL;
  req->retcode = 0;

  /* order matters, see uv_getaddrinfo_done() */
  len = 0;

  if (hints) {
    req->hints = memcpy(buf + len, hints, sizeof(*hints));
    len += sizeof(*hints);
  }

  if (service) {
    req->service = memcpy(buf + len, service, service_len);
    len += service_len;
  }

  if (hostname) {
    req->hostname = memcpy(buf + len, hostname, hostname_len);
    len += hostname_len;
  }

  req_ = eio_custom(getaddrinfo_thread_proc,
                    EIO_PRI_DEFAULT,
                    uv_getaddrinfo_done,
                    req,
                    &loop->uv_eio_channel);

  if (req_)
    return 0;

  free(buf);

  return uv__set_artificial_error(loop, UV_ENOMEM);
}


void uv_freeaddrinfo(struct addrinfo* ai) {
  if (ai)
    freeaddrinfo(ai);
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

out:
  return sockfd;
}


int uv__accept(int sockfd) {
  int peerfd;

  assert(sockfd >= 0);

  while (1) {
#if __linux__
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


#if __linux__

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

#else /* !__linux__ */

int uv__nonblock(int fd, int set) {
  int flags;
  int r;

  do
    r = fcntl(fd, F_GETFL);
  while (r == -1 && errno == EINTR);

  if (r == -1)
    return -1;

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

  if (set)
    flags = r | FD_CLOEXEC;
  else
    flags = r & ~FD_CLOEXEC;

  do
    r = fcntl(fd, F_SETFD, flags);
  while (r == -1 && errno == EINTR);

  return r;
}

#endif /* __linux__ */


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


/* TODO move to uv-common.c? */
size_t uv__strlcpy(char* dst, const char* src, size_t size) {
  const char *org;

  if (size == 0) {
    return 0;
  }

  org = src;
  while (--size && *src) {
    *dst++ = *src++;
  }
  *dst = '\0';

  return src - org;
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


static void uv__io_set_cb(uv__io_t* handle, uv__io_cb cb) {
  union { void* data; uv__io_cb cb; } u;
  u.cb = cb;
  handle->io_watcher.data = u.data;
}


static void uv__io_rw(struct ev_loop* ev, ev_io* w, int events) {
  union { void* data; uv__io_cb cb; } u;
  uv_loop_t* loop = ev_userdata(ev);
  uv__io_t* handle = container_of(w, uv__io_t, io_watcher);
  u.data = handle->io_watcher.data;
  u.cb(loop, handle, events & (EV_READ|EV_WRITE|EV_ERROR));
}


void uv__io_init(uv__io_t* handle, uv__io_cb cb, int fd, int events) {
  ev_io_init(&handle->io_watcher, uv__io_rw, fd, events & (EV_READ|EV_WRITE));
  uv__io_set_cb(handle, cb);
}


void uv__io_set(uv__io_t* handle, uv__io_cb cb, int fd, int events) {
  ev_io_set(&handle->io_watcher, fd, events);
  uv__io_set_cb(handle, cb);
}


void uv__io_start(uv_loop_t* loop, uv__io_t* handle) {
  ev_io_start(loop->ev, &handle->io_watcher);
}


void uv__io_stop(uv_loop_t* loop, uv__io_t* handle) {
  ev_io_stop(loop->ev, &handle->io_watcher);
}


void uv__io_feed(uv_loop_t* loop, uv__io_t* handle, int event) {
  ev_feed_event(loop->ev, &handle->io_watcher, event);
}


int uv__io_active(uv__io_t* handle) {
  return ev_is_active(&handle->io_watcher);
}
