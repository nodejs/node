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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* O_CLOEXEC, accept4(), etc. */
#endif

#include "uv.h"
#include "uv-common.h"
#include "uv-eio.h"

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
#include <poll.h>

#if defined(__linux__)

#include <linux/version.h>
#include <features.h>

#undef HAVE_PIPE2
#undef HAVE_ACCEPT4

/* pipe2() requires linux >= 2.6.27 and glibc >= 2.9 */
#if LINUX_VERSION_CODE >= 0x2061B && __GLIBC_PREREQ(2, 9)
#define HAVE_PIPE2
#endif

/* accept4() requires linux >= 2.6.28 and glib >= 2.10 */
#if LINUX_VERSION_CODE >= 0x2061C && __GLIBC_PREREQ(2, 10)
#define HAVE_ACCEPT4
#endif

#endif /* __linux__ */

#ifdef __sun
# include <sys/types.h>
# include <sys/wait.h>
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h> /* _NSGetExecutablePath */
#endif

#if defined(__FreeBSD__)
#include <sys/sysctl.h>
#include <sys/wait.h>
#endif


# ifdef __APPLE__
# include <crt_externs.h>
# define environ (*_NSGetEnviron())
# else
extern char **environ;
# endif

static uv_err_t last_err;

struct uv_ares_data_s {
  ares_channel channel;
  /*
   * While the channel is active this timer is called once per second to be sure
   * that we're always calling ares_process. See the warning above the
   * definition of ares_timeout().
   */
  ev_timer timer;
};

static struct uv_ares_data_s ares_data;

void uv__req_init(uv_req_t*);
void uv__next(EV_P_ ev_idle* watcher, int revents);
static int uv__stream_open(uv_stream_t*, int fd, int flags);
static void uv__finish_close(uv_handle_t* handle);
static uv_err_t uv_err_new(uv_handle_t* handle, int sys_error);

static int uv_tcp_listen(uv_tcp_t* tcp, int backlog, uv_connection_cb cb);
static int uv_pipe_listen(uv_pipe_t* handle, int backlog, uv_connection_cb cb);
static int uv_pipe_cleanup(uv_pipe_t* handle);
static uv_write_t* uv__write(uv_stream_t* stream);
static void uv__read(uv_stream_t* stream);
static void uv__stream_connect(uv_stream_t*);
static void uv__stream_io(EV_P_ ev_io* watcher, int revents);
static void uv__pipe_accept(EV_P_ ev_io* watcher, int revents);

static void uv__udp_watcher_start(uv_udp_t* handle, ev_io* w);
static void uv__udp_watcher_stop(uv_udp_t* handle, ev_io* w);
static void uv__udp_run_completed(uv_udp_t* handle);
static void uv__udp_run_pending(uv_udp_t* handle);
static void uv__udp_destroy(uv_udp_t* handle);
static void uv__udp_recvmsg(uv_udp_t* handle);
static void uv__udp_sendmsg(uv_udp_t* handle);
static void uv__udp_io(EV_P_ ev_io* w, int events);
static int uv__udp_bind(uv_udp_t* handle,
                        int domain,
                        struct sockaddr* addr,
                        socklen_t len,
                        unsigned flags);
static int uv__udp_maybe_deferred_bind(uv_udp_t* handle, int domain);
static int uv__udp_send(uv_udp_send_t* req,
                        uv_udp_t* handle,
                        uv_buf_t bufs[],
                        int bufcnt,
                        struct sockaddr* addr,
                        socklen_t addrlen,
                        uv_udp_send_cb send_cb);

#ifndef __GNUC__
#define __attribute__(a)
#endif

/* Unused on systems that support O_CLOEXEC, SOCK_CLOEXEC, etc. */
static int uv__cloexec(int fd, int set) __attribute__((unused));
static int uv__nonblock(int fd, int set) __attribute__((unused));

static int uv__socket(int domain, int type, int protocol);
static int uv__accept(int sockfd, struct sockaddr* saddr, socklen_t len);
static int uv__close(int fd);

size_t uv__strlcpy(char* dst, const char* src, size_t size);


/* flags */
enum {
  UV_CLOSING  = 0x00000001, /* uv_close() called but not finished. */
  UV_CLOSED   = 0x00000002, /* close(2) finished. */
  UV_READING  = 0x00000004, /* uv_read_start() called. */
  UV_SHUTTING = 0x00000008, /* uv_shutdown() called but not complete. */
  UV_SHUT     = 0x00000010, /* Write side closed. */
  UV_READABLE = 0x00000020, /* The stream is readable */
  UV_WRITABLE = 0x00000040  /* The stream is writable */
};


/* TODO Share this code with Windows. */
/* TODO Expose callback to user to handle fatal error like V8 does. */
static void uv_fatal_error(const int errorno, const char* syscall) {
  char* buf = NULL;
  const char* errmsg;

  if (buf) {
    errmsg = buf;
  } else {
    errmsg = "Unknown error";
  }

  if (syscall) {
    fprintf(stderr, "\nlibuv fatal error. %s: (%d) %s\n", syscall, errorno,
        errmsg);
  } else {
    fprintf(stderr, "\nlibuv fatal error. (%d) %s\n", errorno, errmsg);
  }

  abort();
}


uv_err_t uv_last_error() {
  return last_err;
}


char* uv_strerror(uv_err_t err) {
  return strerror(err.sys_errno_);
}


static uv_err_code uv_translate_sys_error(int sys_errno) {
  switch (sys_errno) {
    case 0: return UV_OK;
    case EACCES: return UV_EACCESS;
    case EBADF: return UV_EBADF;
    case EPIPE: return UV_EPIPE;
    case EAGAIN: return UV_EAGAIN;
    case ECONNRESET: return UV_ECONNRESET;
    case EFAULT: return UV_EFAULT;
    case EMFILE: return UV_EMFILE;
    case EMSGSIZE: return UV_EMSGSIZE;
    case EINVAL: return UV_EINVAL;
    case ECONNREFUSED: return UV_ECONNREFUSED;
    case EADDRINUSE: return UV_EADDRINUSE;
    case EADDRNOTAVAIL: return UV_EADDRNOTAVAIL;
    default: return UV_UNKNOWN;
  }
}


static uv_err_t uv_err_new_artificial(uv_handle_t* handle, int code) {
  uv_err_t err;
  err.sys_errno_ = 0;
  err.code = code;
  last_err = err;
  return err;
}


static uv_err_t uv_err_new(uv_handle_t* handle, int sys_error) {
  uv_err_t err;
  err.sys_errno_ = sys_error;
  err.code = uv_translate_sys_error(sys_error);
  last_err = err;
  return err;
}


void uv_close(uv_handle_t* handle, uv_close_cb close_cb) {
  uv_udp_t* udp;
  uv_async_t* async;
  uv_timer_t* timer;
  uv_stream_t* stream;
  uv_process_t* process;

  handle->close_cb = close_cb;

  switch (handle->type) {
    case UV_NAMED_PIPE:
      uv_pipe_cleanup((uv_pipe_t*)handle);
      /* Fall through. */

    case UV_TCP:
      stream = (uv_stream_t*)handle;

      uv_read_stop(stream);
      ev_io_stop(EV_DEFAULT_ &stream->write_watcher);

      uv__close(stream->fd);
      stream->fd = -1;

      if (stream->accepted_fd >= 0) {
        uv__close(stream->accepted_fd);
        stream->accepted_fd = -1;
      }

      assert(!ev_is_active(&stream->read_watcher));
      assert(!ev_is_active(&stream->write_watcher));
      break;

    case UV_UDP:
      udp = (uv_udp_t*)handle;
      uv__udp_watcher_stop(udp, &udp->read_watcher);
      uv__udp_watcher_stop(udp, &udp->write_watcher);
      uv__close(udp->fd);
      udp->fd = -1;
      break;

    case UV_PREPARE:
      uv_prepare_stop((uv_prepare_t*) handle);
      break;

    case UV_CHECK:
      uv_check_stop((uv_check_t*) handle);
      break;

    case UV_IDLE:
      uv_idle_stop((uv_idle_t*) handle);
      break;

    case UV_ASYNC:
      async = (uv_async_t*)handle;
      ev_async_stop(EV_DEFAULT_ &async->async_watcher);
      ev_ref(EV_DEFAULT_UC);
      break;

    case UV_TIMER:
      timer = (uv_timer_t*)handle;
      if (ev_is_active(&timer->timer_watcher)) {
        ev_ref(EV_DEFAULT_UC);
      }
      ev_timer_stop(EV_DEFAULT_ &timer->timer_watcher);
      break;

    case UV_PROCESS:
      process = (uv_process_t*)handle;
      ev_child_stop(EV_DEFAULT_UC_ &process->child_watcher);
      break;

    default:
      assert(0);
  }

  handle->flags |= UV_CLOSING;

  /* This is used to call the on_close callback in the next loop. */
  ev_idle_start(EV_DEFAULT_ &handle->next_watcher);
  ev_feed_event(EV_DEFAULT_ &handle->next_watcher, EV_IDLE);
  assert(ev_is_pending(&handle->next_watcher));
}


void uv_init() {
  /* Initialize the default ev loop. */
#if defined(__MAC_OS_X_VERSION_MIN_REQUIRED) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
  ev_default_loop(EVBACKEND_KQUEUE);
#else
  ev_default_loop(EVFLAG_AUTO);
#endif
}


int uv_run() {
  ev_run(EV_DEFAULT_ 0);
  return 0;
}


static void uv__handle_init(uv_handle_t* handle, uv_handle_type type) {
  uv_counters()->handle_init++;

  handle->type = type;
  handle->flags = 0;

  ev_init(&handle->next_watcher, uv__next);
  handle->next_watcher.data = handle;

  /* Ref the loop until this handle is closed. See uv__finish_close. */
  ev_ref(EV_DEFAULT_UC);
}


static void uv__udp_watcher_start(uv_udp_t* handle, ev_io* w) {
  int flags;

  assert(w == &handle->read_watcher
      || w == &handle->write_watcher);

  flags = (w == &handle->read_watcher ? EV_READ : EV_WRITE);

  w->data = handle;
  ev_set_cb(w, uv__udp_io);
  ev_io_set(w, handle->fd, flags);
  ev_io_start(EV_DEFAULT_UC_ w);
}


static void uv__udp_watcher_stop(uv_udp_t* handle, ev_io* w) {
  int flags;

  assert(w == &handle->read_watcher
      || w == &handle->write_watcher);

  flags = (w == &handle->read_watcher ? EV_READ : EV_WRITE);

  ev_io_stop(EV_DEFAULT_UC_ w);
  ev_io_set(w, -1, flags);
  ev_set_cb(w, NULL);
  w->data = (void*)0xDEADBABE;
}


static void uv__udp_destroy(uv_udp_t* handle) {
  uv_udp_send_t* req;
  ngx_queue_t* q;

  uv__udp_run_completed(handle);

  while (!ngx_queue_empty(&handle->write_queue)) {
    q = ngx_queue_head(&handle->write_queue);
    ngx_queue_remove(q);

    req = ngx_queue_data(q, uv_udp_send_t, queue);
    if (req->send_cb) {
      /* FIXME proper error code like UV_EABORTED */
      uv_err_new_artificial((uv_handle_t*)handle, UV_EINTR);
      req->send_cb(req, -1);
    }
  }

  /* Now tear down the handle. */
  handle->flags = 0;
  handle->recv_cb = NULL;
  handle->alloc_cb = NULL;
  /* but _do not_ touch close_cb */

  if (handle->fd != -1) {
    uv__close(handle->fd);
    handle->fd = -1;
  }

  uv__udp_watcher_stop(handle, &handle->read_watcher);
  uv__udp_watcher_stop(handle, &handle->write_watcher);
}


static void uv__udp_run_pending(uv_udp_t* handle) {
  uv_udp_send_t* req;
  ngx_queue_t* q;
  struct msghdr h;
  ssize_t size;

  while (!ngx_queue_empty(&handle->write_queue)) {
    q = ngx_queue_head(&handle->write_queue);
    assert(q != NULL);

    req = ngx_queue_data(q, uv_udp_send_t, queue);
    assert(req != NULL);

    memset(&h, 0, sizeof h);
    h.msg_name = &req->addr;
    h.msg_namelen = req->addrlen;
    h.msg_iov = (struct iovec*)req->bufs;
    h.msg_iovlen = req->bufcnt;

    do {
      size = sendmsg(handle->fd, &h, 0);
    }
    while (size == -1 && errno == EINTR);

    /* TODO try to write once or twice more in the
     * hope that the socket becomes readable again?
     */
    if (size == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
      break;

    req->status = (size == -1 ? -errno : size);

#ifndef NDEBUG
    /* Sanity check. */
    if (size != -1) {
      ssize_t nbytes;
      int i;

      for (nbytes = i = 0; i < req->bufcnt; i++)
        nbytes += req->bufs[i].len;

      assert(size == nbytes);
    }
#endif

    /* Sending a datagram is an atomic operation: either all data
     * is written or nothing is (and EMSGSIZE is raised). That is
     * why we don't handle partial writes. Just pop the request
     * off the write queue and onto the completed queue, done.
     */
    ngx_queue_remove(&req->queue);
    ngx_queue_insert_tail(&handle->write_completed_queue, &req->queue);
  }
}


static void uv__udp_run_completed(uv_udp_t* handle) {
  uv_udp_send_t* req;
  ngx_queue_t* q;

  while (!ngx_queue_empty(&handle->write_completed_queue)) {
    q = ngx_queue_head(&handle->write_completed_queue);
    assert(q != NULL);

    ngx_queue_remove(q);

    req = ngx_queue_data(q, uv_udp_send_t, queue);
    assert(req != NULL);

    if (req->bufs != req->bufsml)
      free(req->bufs);

    if (req->send_cb == NULL)
      continue;

    /* req->status >= 0 == bytes written
     * req->status <  0 == errno
     */
    if (req->status >= 0) {
      req->send_cb(req, 0);
    }
    else {
      uv_err_new((uv_handle_t*)handle, -req->status);
      req->send_cb(req, -1);
    }
  }
}


static void uv__udp_recvmsg(uv_udp_t* handle) {
  struct sockaddr_storage peer;
  struct msghdr h;
  ssize_t nread;
  uv_buf_t buf;
  int flags;

  assert(handle->recv_cb != NULL);
  assert(handle->alloc_cb != NULL);

  do {
    /* FIXME: hoist alloc_cb out the loop but for now follow uv__read() */
    buf = handle->alloc_cb((uv_handle_t*)handle, 64 * 1024);
    assert(buf.len > 0);
    assert(buf.base != NULL);

    memset(&h, 0, sizeof h);
    h.msg_name = &peer;
    h.msg_namelen = sizeof peer;
    h.msg_iov = (struct iovec*)&buf;
    h.msg_iovlen = 1;

    do {
      nread = recvmsg(handle->fd, &h, 0);
    }
    while (nread == -1 && errno == EINTR);

    if (nread == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        uv_err_new((uv_handle_t*)handle, EAGAIN);
        handle->recv_cb(handle, 0, buf, NULL, 0);
      }
      else {
        uv_err_new((uv_handle_t*)handle, errno);
        handle->recv_cb(handle, -1, buf, NULL, 0);
      }
    }
    else {
      flags = 0;

      if (h.msg_flags & MSG_TRUNC)
        flags |= UV_UDP_PARTIAL;

      handle->recv_cb(handle,
                      nread,
                      buf,
                      (struct sockaddr*)&peer,
                      flags);
    }
  }
  /* recv_cb callback may decide to pause or close the handle */
  while (nread != -1
      && handle->fd != -1
      && handle->recv_cb != NULL);
}


static void uv__udp_sendmsg(uv_udp_t* handle) {
  assert(!ngx_queue_empty(&handle->write_queue)
      || !ngx_queue_empty(&handle->write_completed_queue));

  /* Write out pending data first. */
  uv__udp_run_pending(handle);

  /* Drain 'request completed' queue. */
  uv__udp_run_completed(handle);

  if (!ngx_queue_empty(&handle->write_completed_queue)) {
    /* Schedule completion callbacks. */
    ev_feed_event(EV_DEFAULT_ &handle->write_watcher, EV_WRITE);
  }
  else if (ngx_queue_empty(&handle->write_queue)) {
    /* Pending queue and completion queue empty, stop watcher. */
    uv__udp_watcher_stop(handle, &handle->write_watcher);
  }
}


static void uv__udp_io(EV_P_ ev_io* w, int events) {
  uv_udp_t* handle;

  handle = w->data;
  assert(handle != NULL);
  assert(handle->type == UV_UDP);
  assert(handle->fd >= 0);
  assert(!(events & ~(EV_READ|EV_WRITE)));

  if (events & EV_READ)
    uv__udp_recvmsg(handle);

  if (events & EV_WRITE)
    uv__udp_sendmsg(handle);
}


static int uv__udp_bind(uv_udp_t* handle,
                        int domain,
                        struct sockaddr* addr,
                        socklen_t len,
                        unsigned flags) {
  int saved_errno;
  int status;
  int yes;
  int fd;

  saved_errno = errno;
  status = -1;

  /* Check for bad flags. */
  if (flags & ~UV_UDP_IPV6ONLY) {
    uv_err_new((uv_handle_t*)handle, EINVAL);
    goto out;
  }

  /* Cannot set IPv6-only mode on non-IPv6 socket. */
  if ((flags & UV_UDP_IPV6ONLY) && domain != AF_INET6) {
    uv_err_new((uv_handle_t*)handle, EINVAL);
    goto out;
  }

  /* Check for already active socket. */
  if (handle->fd != -1) {
    uv_err_new_artificial((uv_handle_t*)handle, UV_EALREADY);
    goto out;
  }

  if ((fd = uv__socket(domain, SOCK_DGRAM, 0)) == -1) {
    uv_err_new((uv_handle_t*)handle, errno);
    goto out;
  }

  if (flags & UV_UDP_IPV6ONLY) {
#ifdef IPV6_V6ONLY
    yes = 1;
    if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof yes) == -1) {
      uv_err_new((uv_handle_t*)handle, errno);
      goto out;
    }
#else
    uv_err_new((uv_handle_t*)handle, ENOTSUP);
    goto out;
#endif
  }

  if (bind(fd, addr, len) == -1) {
    uv_err_new((uv_handle_t*)handle, errno);
    goto out;
  }

  handle->fd = fd;
  status = 0;

out:
  if (status)
    uv__close(fd);

  errno = saved_errno;
  return status;
}


static int uv__udp_maybe_deferred_bind(uv_udp_t* handle, int domain) {
  struct sockaddr_storage taddr;
  socklen_t addrlen;

  assert(domain == AF_INET || domain == AF_INET6);

  if (handle->fd != -1)
    return 0;

  switch (domain) {
  case AF_INET:
  {
    struct sockaddr_in* addr = (void*)&taddr;
    memset(addr, 0, sizeof *addr);
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = INADDR_ANY;
    addrlen = sizeof *addr;
    break;
  }
  case AF_INET6:
  {
    struct sockaddr_in6* addr = (void*)&taddr;
    memset(addr, 0, sizeof *addr);
    addr->sin6_family = AF_INET6;
    addr->sin6_addr = in6addr_any;
    addrlen = sizeof *addr;
    break;
  }
  default:
    assert(0 && "unsupported address family");
    abort();
  }

  return uv__udp_bind(handle, domain, (struct sockaddr*)&taddr, addrlen, 0);
}


static int uv__udp_send(uv_udp_send_t* req,
                        uv_udp_t* handle,
                        uv_buf_t bufs[],
                        int bufcnt,
                        struct sockaddr* addr,
                        socklen_t addrlen,
                        uv_udp_send_cb send_cb) {
  if (uv__udp_maybe_deferred_bind(handle, addr->sa_family))
    return -1;

  /* Don't use uv__req_init(), it zeroes the data field. */
  uv_counters()->req_init++;

  memcpy(&req->addr, addr, addrlen);
  req->addrlen = addrlen;
  req->send_cb = send_cb;
  req->handle = handle;
  req->bufcnt = bufcnt;
  req->type = UV_UDP_SEND;

  if (bufcnt <= UV_REQ_BUFSML_SIZE) {
    req->bufs = req->bufsml;
  }
  else if ((req->bufs = malloc(bufcnt * sizeof(bufs[0]))) == NULL) {
    uv_err_new((uv_handle_t*)handle, ENOMEM);
    return -1;
  }
  memcpy(req->bufs, bufs, bufcnt * sizeof(bufs[0]));

  ngx_queue_insert_tail(&handle->write_queue, &req->queue);
  uv__udp_watcher_start(handle, &handle->write_watcher);

  return 0;
}


int uv_udp_init(uv_udp_t* handle) {
  memset(handle, 0, sizeof *handle);

  uv__handle_init((uv_handle_t*)handle, UV_UDP);
  uv_counters()->udp_init++;

  handle->fd = -1;
  ngx_queue_init(&handle->write_queue);
  ngx_queue_init(&handle->write_completed_queue);

  return 0;
}


int uv_udp_bind(uv_udp_t* handle, struct sockaddr_in addr, unsigned flags) {
  return uv__udp_bind(handle,
                      AF_INET,
                      (struct sockaddr*)&addr,
                      sizeof addr,
                      flags);
}


int uv_udp_bind6(uv_udp_t* handle, struct sockaddr_in6 addr, unsigned flags) {
  return uv__udp_bind(handle,
                      AF_INET6,
                      (struct sockaddr*)&addr,
                      sizeof addr,
                      flags);
}


int uv_udp_send(uv_udp_send_t* req,
                uv_udp_t* handle,
                uv_buf_t bufs[],
                int bufcnt,
                struct sockaddr_in addr,
                uv_udp_send_cb send_cb) {
  return uv__udp_send(req,
                      handle,
                      bufs,
                      bufcnt,
                      (struct sockaddr*)&addr,
                      sizeof addr,
                      send_cb);
}


int uv_udp_send6(uv_udp_send_t* req,
                 uv_udp_t* handle,
                 uv_buf_t bufs[],
                 int bufcnt,
                 struct sockaddr_in6 addr,
                 uv_udp_send_cb send_cb) {
  return uv__udp_send(req,
                      handle,
                      bufs,
                      bufcnt,
                      (struct sockaddr*)&addr,
                      sizeof addr,
                      send_cb);
}


int uv_udp_recv_start(uv_udp_t* handle,
                      uv_alloc_cb alloc_cb,
                      uv_udp_recv_cb recv_cb) {
  if (alloc_cb == NULL || recv_cb == NULL) {
    uv_err_new_artificial((uv_handle_t*)handle, UV_EINVAL);
    return -1;
  }

  if (ev_is_active(&handle->read_watcher)) {
    uv_err_new_artificial((uv_handle_t*)handle, UV_EALREADY);
    return -1;
  }

  if (uv__udp_maybe_deferred_bind(handle, AF_INET))
    return -1;

  handle->alloc_cb = alloc_cb;
  handle->recv_cb = recv_cb;
  uv__udp_watcher_start(handle, &handle->read_watcher);

  return 0;
}


int uv_udp_recv_stop(uv_udp_t* handle) {
  uv__udp_watcher_stop(handle, &handle->read_watcher);
  handle->alloc_cb = NULL;
  handle->recv_cb = NULL;
  return 0;
}


int uv_tcp_init(uv_tcp_t* tcp) {
  uv__handle_init((uv_handle_t*)tcp, UV_TCP);
  uv_counters()->tcp_init++;

  tcp->alloc_cb = NULL;
  tcp->connect_req = NULL;
  tcp->accepted_fd = -1;
  tcp->fd = -1;
  tcp->delayed_error = 0;
  ngx_queue_init(&tcp->write_queue);
  ngx_queue_init(&tcp->write_completed_queue);
  tcp->write_queue_size = 0;

  ev_init(&tcp->read_watcher, uv__stream_io);
  tcp->read_watcher.data = tcp;

  ev_init(&tcp->write_watcher, uv__stream_io);
  tcp->write_watcher.data = tcp;

  assert(ngx_queue_empty(&tcp->write_queue));
  assert(ngx_queue_empty(&tcp->write_completed_queue));
  assert(tcp->write_queue_size == 0);

  return 0;
}


static int uv__tcp_bind(uv_tcp_t* tcp,
                        int domain,
                        struct sockaddr* addr,
                        int addrsize) {
  int saved_errno;
  int status;

  saved_errno = errno;
  status = -1;

  if (tcp->fd < 0) {
    if ((tcp->fd = uv__socket(domain, SOCK_STREAM, 0)) == -1) {
      uv_err_new((uv_handle_t*)tcp, errno);
      goto out;
    }

    if (uv__stream_open((uv_stream_t*)tcp, tcp->fd, UV_READABLE | UV_WRITABLE)) {
      uv__close(tcp->fd);
      tcp->fd = -1;
      status = -2;
      goto out;
    }
  }

  assert(tcp->fd >= 0);

  tcp->delayed_error = 0;
  if (bind(tcp->fd, addr, addrsize) == -1) {
    if (errno == EADDRINUSE) {
      tcp->delayed_error = errno;
    } else {
      uv_err_new((uv_handle_t*)tcp, errno);
      goto out;
    }
  }
  status = 0;

out:
  errno = saved_errno;
  return status;
}


int uv_tcp_bind(uv_tcp_t* tcp, struct sockaddr_in addr) {
  if (addr.sin_family != AF_INET) {
    uv_err_new((uv_handle_t*)tcp, EFAULT);
    return -1;
  }

  return uv__tcp_bind(tcp,
                      AF_INET,
                      (struct sockaddr*)&addr,
                      sizeof(struct sockaddr_in));
}


int uv_tcp_bind6(uv_tcp_t* tcp, struct sockaddr_in6 addr) {
  if (addr.sin6_family != AF_INET6) {
    uv_err_new((uv_handle_t*)tcp, EFAULT);
    return -1;
  }

  return uv__tcp_bind(tcp,
                      AF_INET6,
                      (struct sockaddr*)&addr,
                      sizeof(struct sockaddr_in6));
}


static int uv__stream_open(uv_stream_t* stream, int fd, int flags) {
  socklen_t yes;

  assert(fd >= 0);
  stream->fd = fd;

  ((uv_handle_t*)stream)->flags |= flags;

  /* Reuse the port address if applicable. */
  yes = 1;
  if (stream->type == UV_TCP
      && setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
    uv_err_new((uv_handle_t*)stream, errno);
    return -1;
  }

  /* Associate the fd with each ev_io watcher. */
  ev_io_set(&stream->read_watcher, fd, EV_READ);
  ev_io_set(&stream->write_watcher, fd, EV_WRITE);

  /* These should have been set up by uv_tcp_init or uv_pipe_init. */
  assert(stream->read_watcher.cb == uv__stream_io);
  assert(stream->write_watcher.cb == uv__stream_io);

  return 0;
}


void uv__server_io(EV_P_ ev_io* watcher, int revents) {
  int fd;
  struct sockaddr_storage addr;
  uv_stream_t* stream = watcher->data;

  assert(watcher == &stream->read_watcher ||
         watcher == &stream->write_watcher);
  assert(revents == EV_READ);

  assert(!(stream->flags & UV_CLOSING));

  if (stream->accepted_fd >= 0) {
    ev_io_stop(EV_DEFAULT_ &stream->read_watcher);
    return;
  }

  /* connection_cb can close the server socket while we're
   * in the loop so check it on each iteration.
   */
  while (stream->fd != -1) {
    assert(stream->accepted_fd < 0);
    fd = uv__accept(stream->fd, (struct sockaddr*)&addr, sizeof addr);

    if (fd < 0) {
      if (errno == EAGAIN) {
        /* No problem. */
        return;
      } else if (errno == EMFILE) {
        /* TODO special trick. unlock reserved socket, accept, close. */
        return;
      } else {
        uv_err_new((uv_handle_t*)stream, errno);
        stream->connection_cb((uv_stream_t*)stream, -1);
      }
    } else {
      stream->accepted_fd = fd;
      stream->connection_cb((uv_stream_t*)stream, 0);
      if (stream->accepted_fd >= 0) {
        /* The user hasn't yet accepted called uv_accept() */
        ev_io_stop(EV_DEFAULT_ &stream->read_watcher);
        return;
      }
    }
  }
}


int uv_accept(uv_stream_t* server, uv_stream_t* client) {
  uv_stream_t* streamServer;
  uv_stream_t* streamClient;
  int saved_errno;
  int status;

  saved_errno = errno;
  status = -1;

  streamServer = (uv_stream_t*)server;
  streamClient = (uv_stream_t*)client;

  if (streamServer->accepted_fd < 0) {
    uv_err_new((uv_handle_t*)server, EAGAIN);
    goto out;
  }

  if (uv__stream_open(streamClient, streamServer->accepted_fd,
        UV_READABLE | UV_WRITABLE)) {
    /* TODO handle error */
    streamServer->accepted_fd = -1;
    uv__close(streamServer->accepted_fd);
    goto out;
  }

  ev_io_start(EV_DEFAULT_ &streamServer->read_watcher);
  streamServer->accepted_fd = -1;
  status = 0;

out:
  errno = saved_errno;
  return status;
}


int uv_listen(uv_stream_t* stream, int backlog, uv_connection_cb cb) {
  switch (stream->type) {
    case UV_TCP:
      return uv_tcp_listen((uv_tcp_t*)stream, backlog, cb);
    case UV_NAMED_PIPE:
      return uv_pipe_listen((uv_pipe_t*)stream, backlog, cb);
    default:
      assert(0);
      return -1;
  }
}


static int uv_tcp_listen(uv_tcp_t* tcp, int backlog, uv_connection_cb cb) {
  int r;

  if (tcp->delayed_error) {
    uv_err_new((uv_handle_t*)tcp, tcp->delayed_error);
    return -1;
  }

  if (tcp->fd < 0) {
    if ((tcp->fd = uv__socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      uv_err_new((uv_handle_t*)tcp, errno);
      return -1;
    }

    if (uv__stream_open((uv_stream_t*)tcp, tcp->fd, UV_READABLE)) {
      uv__close(tcp->fd);
      tcp->fd = -1;
      return -1;
    }
  }

  assert(tcp->fd >= 0);

  r = listen(tcp->fd, backlog);
  if (r < 0) {
    uv_err_new((uv_handle_t*)tcp, errno);
    return -1;
  }

  tcp->connection_cb = cb;

  /* Start listening for connections. */
  ev_io_set(&tcp->read_watcher, tcp->fd, EV_READ);
  ev_set_cb(&tcp->read_watcher, uv__server_io);
  ev_io_start(EV_DEFAULT_ &tcp->read_watcher);

  return 0;
}


void uv__finish_close(uv_handle_t* handle) {
  assert(handle->flags & UV_CLOSING);
  assert(!(handle->flags & UV_CLOSED));
  handle->flags |= UV_CLOSED;

  switch (handle->type) {
    case UV_PREPARE:
      assert(!ev_is_active(&((uv_prepare_t*)handle)->prepare_watcher));
      break;

    case UV_CHECK:
      assert(!ev_is_active(&((uv_check_t*)handle)->check_watcher));
      break;

    case UV_IDLE:
      assert(!ev_is_active(&((uv_idle_t*)handle)->idle_watcher));
      break;

    case UV_ASYNC:
      assert(!ev_is_active(&((uv_async_t*)handle)->async_watcher));
      break;

    case UV_TIMER:
      assert(!ev_is_active(&((uv_timer_t*)handle)->timer_watcher));
      break;

    case UV_NAMED_PIPE:
    case UV_TCP:
      assert(!ev_is_active(&((uv_stream_t*)handle)->read_watcher));
      assert(!ev_is_active(&((uv_stream_t*)handle)->write_watcher));
      break;

    case UV_UDP:
      assert(!ev_is_active(&((uv_udp_t*)handle)->read_watcher));
      assert(!ev_is_active(&((uv_udp_t*)handle)->write_watcher));
      assert(((uv_udp_t*)handle)->fd == -1);
      uv__udp_destroy((uv_udp_t*)handle);
      break;

    case UV_PROCESS:
      assert(!ev_is_active(&((uv_process_t*)handle)->child_watcher));
      break;

    default:
      assert(0);
      break;
  }

  ev_idle_stop(EV_DEFAULT_ &handle->next_watcher);

  if (handle->close_cb) {
    handle->close_cb(handle);
  }

  ev_unref(EV_DEFAULT_UC);
}


uv_write_t* uv_write_queue_head(uv_stream_t* stream) {
  ngx_queue_t* q;
  uv_write_t* req;

  if (ngx_queue_empty(&stream->write_queue)) {
    return NULL;
  }

  q = ngx_queue_head(&stream->write_queue);
  if (!q) {
    return NULL;
  }

  req = ngx_queue_data(q, struct uv_write_s, queue);
  assert(req);

  return req;
}


void uv__next(EV_P_ ev_idle* watcher, int revents) {
  uv_handle_t* handle = watcher->data;
  assert(watcher == &handle->next_watcher);
  assert(revents == EV_IDLE);

  /* For now this function is only to handle the closing event, but we might
   * put more stuff here later.
   */
  assert(handle->flags & UV_CLOSING);
  uv__finish_close(handle);
}


static void uv__drain(uv_stream_t* stream) {
  uv_shutdown_t* req;

  assert(!uv_write_queue_head(stream));
  assert(stream->write_queue_size == 0);

  ev_io_stop(EV_DEFAULT_ &stream->write_watcher);

  /* Shutdown? */
  if ((stream->flags & UV_SHUTTING) &&
      !(stream->flags & UV_CLOSING) &&
      !(stream->flags & UV_SHUT)) {
    assert(stream->shutdown_req);

    req = stream->shutdown_req;

    if (shutdown(stream->fd, SHUT_WR)) {
      /* Error. Report it. User should call uv_close(). */
      uv_err_new((uv_handle_t*)stream, errno);
      if (req->cb) {
        req->cb(req, -1);
      }
    } else {
      uv_err_new((uv_handle_t*)stream, 0);
      ((uv_handle_t*) stream)->flags |= UV_SHUT;
      if (req->cb) {
        req->cb(req, 0);
      }
    }
  }
}


/* On success returns NULL. On error returns a pointer to the write request
 * which had the error.
 */
static uv_write_t* uv__write(uv_stream_t* stream) {
  uv_write_t* req;
  struct iovec* iov;
  int iovcnt;
  ssize_t n;

  assert(stream->fd >= 0);

  /* TODO: should probably while(1) here until EAGAIN */

  /* Get the request at the head of the queue. */
  req = uv_write_queue_head(stream);
  if (!req) {
    assert(stream->write_queue_size == 0);
    return NULL;
  }

  assert(req->handle == stream);

  /* Cast to iovec. We had to have our own uv_buf_t instead of iovec
   * because Windows's WSABUF is not an iovec.
   */
  assert(sizeof(uv_buf_t) == sizeof(struct iovec));
  iov = (struct iovec*) &(req->bufs[req->write_index]);
  iovcnt = req->bufcnt - req->write_index;

  /* Now do the actual writev. Note that we've been updating the pointers
   * inside the iov each time we write. So there is no need to offset it.
   */

  do {
    if (iovcnt == 1) {
      n = write(stream->fd, iov[0].iov_base, iov[0].iov_len);
    } else {
      n = writev(stream->fd, iov, iovcnt);
    }
  }
  while (n == -1 && errno == EINTR);

  if (n < 0) {
    if (errno != EAGAIN) {
      /* Error */
      uv_err_new((uv_handle_t*)stream, errno);
      return req;
    }
  } else {
    /* Successful write */

    /* Update the counters. */
    while (n >= 0) {
      uv_buf_t* buf = &(req->bufs[req->write_index]);
      size_t len = buf->len;

      assert(req->write_index < req->bufcnt);

      if ((size_t)n < len) {
        buf->base += n;
        buf->len -= n;
        stream->write_queue_size -= n;
        n = 0;

        /* There is more to write. Break and ensure the watcher is pending. */
        break;

      } else {
        /* Finished writing the buf at index req->write_index. */
        req->write_index++;

        assert((size_t)n >= len);
        n -= len;

        assert(stream->write_queue_size >= len);
        stream->write_queue_size -= len;

        if (req->write_index == req->bufcnt) {
          /* Then we're done! */
          assert(n == 0);

          /* Pop the req off tcp->write_queue. */
          ngx_queue_remove(&req->queue);
          if (req->bufs != req->bufsml) {
            free(req->bufs);
          }
          req->bufs = NULL;

          /* Add it to the write_completed_queue where it will have its
           * callback called in the near future.
           * TODO: start trying to write the next request.
           */
          ngx_queue_insert_tail(&stream->write_completed_queue, &req->queue);
          ev_feed_event(EV_DEFAULT_ &stream->write_watcher, EV_WRITE);
          return NULL;
        }
      }
    }
  }

  /* Either we've counted n down to zero or we've got EAGAIN. */
  assert(n == 0 || n == -1);

  /* We're not done. */
  ev_io_start(EV_DEFAULT_ &stream->write_watcher);

  return NULL;
}


static void uv__write_callbacks(uv_stream_t* stream) {
  int callbacks_made = 0;
  ngx_queue_t* q;
  uv_write_t* req;

  while (!ngx_queue_empty(&stream->write_completed_queue)) {
    /* Pop a req off write_completed_queue. */
    q = ngx_queue_head(&stream->write_completed_queue);
    assert(q);
    req = ngx_queue_data(q, struct uv_write_s, queue);
    ngx_queue_remove(q);

    /* NOTE: call callback AFTER freeing the request data. */
    if (req->cb) {
      req->cb(req, 0);
    }

    callbacks_made++;
  }

  assert(ngx_queue_empty(&stream->write_completed_queue));

  /* Write queue drained. */
  if (!uv_write_queue_head(stream)) {
    uv__drain(stream);
  }
}


static void uv__read(uv_stream_t* stream) {
  uv_buf_t buf;
  ssize_t nread;

  /* XXX: Maybe instead of having UV_READING we just test if
   * tcp->read_cb is NULL or not?
   */
  while (stream->read_cb && ((uv_handle_t*)stream)->flags & UV_READING) {
    assert(stream->alloc_cb);
    buf = stream->alloc_cb((uv_handle_t*)stream, 64 * 1024);

    assert(buf.len > 0);
    assert(buf.base);
    assert(stream->fd >= 0);

    do {
      nread = read(stream->fd, buf.base, buf.len);
    }
    while (nread < 0 && errno == EINTR);

    if (nread < 0) {
      /* Error */
      if (errno == EAGAIN) {
        /* Wait for the next one. */
        if (stream->flags & UV_READING) {
          ev_io_start(EV_DEFAULT_UC_ &stream->read_watcher);
        }
        uv_err_new((uv_handle_t*)stream, EAGAIN);
        stream->read_cb(stream, 0, buf);
        return;
      } else {
        /* Error. User should call uv_close(). */
        uv_err_new((uv_handle_t*)stream, errno);
        stream->read_cb(stream, -1, buf);
        assert(!ev_is_active(&stream->read_watcher));
        return;
      }
    } else if (nread == 0) {
      /* EOF */
      uv_err_new_artificial((uv_handle_t*)stream, UV_EOF);
      ev_io_stop(EV_DEFAULT_UC_ &stream->read_watcher);
      stream->read_cb(stream, -1, buf);
      return;
    } else {
      /* Successful read */
      stream->read_cb(stream, nread, buf);
    }
  }
}


int uv_shutdown(uv_shutdown_t* req, uv_stream_t* stream, uv_shutdown_cb cb) {
  assert((stream->type == UV_TCP || stream->type == UV_NAMED_PIPE) &&
         "uv_shutdown (unix) only supports uv_handle_t right now");
  assert(stream->fd >= 0);

  if (!(stream->flags & UV_WRITABLE) ||
      stream->flags & UV_SHUT ||
      stream->flags & UV_CLOSED ||
      stream->flags & UV_CLOSING) {
    uv_err_new((uv_handle_t*)stream, EINVAL);
    return -1;
  }

  /* Initialize request */
  uv__req_init((uv_req_t*)req);
  req->handle = stream;
  req->cb = cb;

  stream->shutdown_req = req;
  req->type = UV_SHUTDOWN;

  ((uv_handle_t*)stream)->flags |= UV_SHUTTING;


  ev_io_start(EV_DEFAULT_UC_ &stream->write_watcher);

  return 0;
}


static void uv__stream_io(EV_P_ ev_io* watcher, int revents) {
  uv_stream_t* stream = watcher->data;

  assert(stream->type == UV_TCP ||
         stream->type == UV_NAMED_PIPE);
  assert(watcher == &stream->read_watcher ||
         watcher == &stream->write_watcher);
  assert(!(stream->flags & UV_CLOSING));

  if (stream->connect_req) {
    uv__stream_connect(stream);
  } else {
    assert(revents & (EV_READ | EV_WRITE));
    assert(stream->fd >= 0);

    if (revents & EV_READ) {
      uv__read((uv_stream_t*)stream);
    }

    if (revents & EV_WRITE) {
      uv_write_t* req = uv__write(stream);
      if (req) {
        /* Error. Notify the user. */
        if (req->cb) {
          req->cb(req, -1);
        }
      } else {
        uv__write_callbacks(stream);
      }
    }
  }
}


/**
 * We get called here from directly following a call to connect(2).
 * In order to determine if we've errored out or succeeded must call
 * getsockopt.
 */
static void uv__stream_connect(uv_stream_t* stream) {
  int error;
  uv_connect_t* req = stream->connect_req;
  socklen_t errorsize = sizeof(int);

  assert(stream->type == UV_TCP || stream->type == UV_NAMED_PIPE);
  assert(req);

  if (stream->delayed_error) {
    /* To smooth over the differences between unixes errors that
     * were reported synchronously on the first connect can be delayed
     * until the next tick--which is now.
     */
    error = stream->delayed_error;
    stream->delayed_error = 0;
  } else {
    /* Normal situation: we need to get the socket error from the kernel. */
    assert(stream->fd >= 0);
    getsockopt(stream->fd, SOL_SOCKET, SO_ERROR, &error, &errorsize);
  }

  if (!error) {
    ev_io_start(EV_DEFAULT_ &stream->read_watcher);

    /* Successful connection */
    stream->connect_req = NULL;
    if (req->cb) {
      req->cb(req, 0);
    }

  } else if (error == EINPROGRESS) {
    /* Still connecting. */
    return;
  } else {
    /* Error */
    uv_err_new((uv_handle_t*)stream, error);

    stream->connect_req = NULL;
    if (req->cb) {
      req->cb(req, -1);
    }
  }
}


static int uv__connect(uv_connect_t* req,
                       uv_stream_t* stream,
                       struct sockaddr* addr,
                       socklen_t addrlen,
                       uv_connect_cb cb) {

  int sockfd;
  int r;

  if (stream->fd <= 0) {
    if ((sockfd = uv__socket(addr->sa_family, SOCK_STREAM, 0)) == -1) {
      uv_err_new((uv_handle_t*)stream, errno);
      return -1;
    }

    if (uv__stream_open(stream, sockfd, UV_READABLE | UV_WRITABLE)) {
      uv__close(sockfd);
      return -2;
    }
  }

  uv__req_init((uv_req_t*)req);
  req->cb = cb;
  req->handle = stream;
  req->type = UV_CONNECT;
  ngx_queue_init(&req->queue);

  if (stream->connect_req) {
    uv_err_new((uv_handle_t*)stream, EALREADY);
    return -1;
  }

  if (stream->type != UV_TCP) {
    uv_err_new((uv_handle_t*)stream, ENOTSOCK);
    return -1;
  }

  stream->connect_req = req;

  do {
    r = connect(stream->fd, addr, addrlen);
  }
  while (r == -1 && errno == EINTR);

  stream->delayed_error = 0;

  if (r != 0 && errno != EINPROGRESS) {
    switch (errno) {
      /* If we get a ECONNREFUSED wait until the next tick to report the
       * error. Solaris wants to report immediately--other unixes want to
       * wait.
       */
      case ECONNREFUSED:
        stream->delayed_error = errno;
        break;

      default:
        uv_err_new((uv_handle_t*)stream, errno);
        return -1;
    }
  }

  assert(stream->write_watcher.data == stream);
  ev_io_start(EV_DEFAULT_ &stream->write_watcher);

  if (stream->delayed_error) {
    ev_feed_event(EV_DEFAULT_ &stream->write_watcher, EV_WRITE);
  }

  return 0;
}


int uv_tcp_connect(uv_connect_t* req,
                   uv_tcp_t* handle,
                   struct sockaddr_in address,
                   uv_connect_cb cb) {
  int saved_errno;
  int status;

  saved_errno = errno;
  status = -1;

  if (handle->type != UV_TCP) {
    uv_err_new((uv_handle_t*)handle, EINVAL);
    goto out;
  }

  if (address.sin_family != AF_INET) {
    uv_err_new((uv_handle_t*)handle, EINVAL);
    goto out;
  }

  status = uv__connect(req,
                       (uv_stream_t*)handle,
                       (struct sockaddr*)&address,
                       sizeof address,
                       cb);

out:
  errno = saved_errno;
  return status;
}


int uv_tcp_connect6(uv_connect_t* req,
                    uv_tcp_t* handle,
                    struct sockaddr_in6 address,
                    uv_connect_cb cb) {
  int saved_errno;
  int status;

  saved_errno = errno;
  status = -1;

  if (handle->type != UV_TCP) {
    uv_err_new((uv_handle_t*)handle, EINVAL);
    goto out;
  }

  if (address.sin6_family != AF_INET6) {
    uv_err_new((uv_handle_t*)handle, EINVAL);
    goto out;
  }

  status = uv__connect(req,
                       (uv_stream_t*)handle,
                       (struct sockaddr*)&address,
                       sizeof address,
                       cb);

out:
  errno = saved_errno;
  return status;
}


int uv_getsockname(uv_handle_t* handle, struct sockaddr* name, int* namelen) {
  socklen_t socklen;
  int saved_errno;

  /* Don't clobber errno. */
  saved_errno = errno;

  /* sizeof(socklen_t) != sizeof(int) on some systems. */
  socklen = (socklen_t)*namelen;

  if (getsockname(handle->fd, name, &socklen) == -1) {
    uv_err_new((uv_handle_t*)handle, errno);
  } else {
    *namelen = (int)socklen;
  }

  errno = saved_errno;
  return 0;
}


static size_t uv__buf_count(uv_buf_t bufs[], int bufcnt) {
  size_t total = 0;
  int i;

  for (i = 0; i < bufcnt; i++) {
    total += bufs[i].len;
  }

  return total;
}


/* The buffers to be written must remain valid until the callback is called.
 * This is not required for the uv_buf_t array.
 */
int uv_write(uv_write_t* req, uv_stream_t* handle, uv_buf_t bufs[], int bufcnt,
    uv_write_cb cb) {
  uv_stream_t* stream;
  int empty_queue;

  stream = (uv_stream_t*)handle;

  /* Initialize the req */
  uv__req_init((uv_req_t*) req);
  req->cb = cb;
  req->handle = handle;
  ngx_queue_init(&req->queue);

  assert((handle->type == UV_TCP || handle->type == UV_NAMED_PIPE)
      && "uv_write (unix) does not yet support other types of streams");

  empty_queue = (stream->write_queue_size == 0);

  if (stream->fd < 0) {
    uv_err_new((uv_handle_t*)stream, EBADF);
    return -1;
  }

  ngx_queue_init(&req->queue);
  req->type = UV_WRITE;


  if (bufcnt < UV_REQ_BUFSML_SIZE) {
    req->bufs = req->bufsml;
  }
  else {
    req->bufs = malloc(sizeof(uv_buf_t) * bufcnt);
  }

  memcpy(req->bufs, bufs, bufcnt * sizeof(uv_buf_t));
  req->bufcnt = bufcnt;

  /*
   * fprintf(stderr, "cnt: %d bufs: %p bufsml: %p\n", bufcnt, req->bufs, req->bufsml);
   */

  req->write_index = 0;
  stream->write_queue_size += uv__buf_count(bufs, bufcnt);

  /* Append the request to write_queue. */
  ngx_queue_insert_tail(&stream->write_queue, &req->queue);

  assert(!ngx_queue_empty(&stream->write_queue));
  assert(stream->write_watcher.cb == uv__stream_io);
  assert(stream->write_watcher.data == stream);
  assert(stream->write_watcher.fd == stream->fd);

  /* If the queue was empty when this function began, we should attempt to
   * do the write immediately. Otherwise start the write_watcher and wait
   * for the fd to become writable.
   */
  if (empty_queue) {
    if (uv__write(stream)) {
      /* Error. uv_last_error has been set. */
      return -1;
    }
  }

  /* If the queue is now empty - we've flushed the request already. That
   * means we need to make the callback. The callback can only be done on a
   * fresh stack so we feed the event loop in order to service it.
   */
  if (ngx_queue_empty(&stream->write_queue)) {
    ev_feed_event(EV_DEFAULT_ &stream->write_watcher, EV_WRITE);
  } else {
    /* Otherwise there is data to write - so we should wait for the file
     * descriptor to become writable.
     */
    ev_io_start(EV_DEFAULT_ &stream->write_watcher);
  }

  return 0;
}


void uv_ref() {
  ev_ref(EV_DEFAULT_UC);
}


void uv_unref() {
  ev_unref(EV_DEFAULT_UC);
}


void uv_update_time() {
  ev_now_update(EV_DEFAULT_UC);
}


int64_t uv_now() {
  return (int64_t)(ev_now(EV_DEFAULT_UC) * 1000);
}


int uv_read_start(uv_stream_t* stream, uv_alloc_cb alloc_cb, uv_read_cb read_cb) {
  assert(stream->type == UV_TCP || stream->type == UV_NAMED_PIPE);

  if (stream->flags & UV_CLOSING) {
    uv_err_new((uv_handle_t*)stream, EINVAL);
    return -1;
  }

  /* The UV_READING flag is irrelevant of the state of the tcp - it just
   * expresses the desired state of the user.
   */
  ((uv_handle_t*)stream)->flags |= UV_READING;

  /* TODO: try to do the read inline? */
  /* TODO: keep track of tcp state. If we've gotten a EOF then we should
   * not start the IO watcher.
   */
  assert(stream->fd >= 0);
  assert(alloc_cb);

  stream->read_cb = read_cb;
  stream->alloc_cb = alloc_cb;

  /* These should have been set by uv_tcp_init. */
  assert(stream->read_watcher.cb == uv__stream_io);

  ev_io_start(EV_DEFAULT_UC_ &stream->read_watcher);
  return 0;
}


int uv_read_stop(uv_stream_t* stream) {
  uv_tcp_t* tcp = (uv_tcp_t*)stream;

  ((uv_handle_t*)tcp)->flags &= ~UV_READING;

  ev_io_stop(EV_DEFAULT_UC_ &tcp->read_watcher);
  tcp->read_cb = NULL;
  tcp->alloc_cb = NULL;
  return 0;
}


void uv__req_init(uv_req_t* req) {
  uv_counters()->req_init++;
  req->type = UV_UNKNOWN_REQ;
  req->data = NULL;
}


static void uv__prepare(EV_P_ ev_prepare* w, int revents) {
  uv_prepare_t* prepare = w->data;

  if (prepare->prepare_cb) {
    prepare->prepare_cb(prepare, 0);
  }
}


int uv_prepare_init(uv_prepare_t* prepare) {
  uv__handle_init((uv_handle_t*)prepare, UV_PREPARE);
  uv_counters()->prepare_init++;

  ev_prepare_init(&prepare->prepare_watcher, uv__prepare);
  prepare->prepare_watcher.data = prepare;

  prepare->prepare_cb = NULL;

  return 0;
}


int uv_prepare_start(uv_prepare_t* prepare, uv_prepare_cb cb) {
  int was_active = ev_is_active(&prepare->prepare_watcher);

  prepare->prepare_cb = cb;

  ev_prepare_start(EV_DEFAULT_UC_ &prepare->prepare_watcher);

  if (!was_active) {
    ev_unref(EV_DEFAULT_UC);
  }

  return 0;
}


int uv_prepare_stop(uv_prepare_t* prepare) {
  int was_active = ev_is_active(&prepare->prepare_watcher);

  ev_prepare_stop(EV_DEFAULT_UC_ &prepare->prepare_watcher);

  if (was_active) {
    ev_ref(EV_DEFAULT_UC);
  }
  return 0;
}



static void uv__check(EV_P_ ev_check* w, int revents) {
  uv_check_t* check = w->data;

  if (check->check_cb) {
    check->check_cb(check, 0);
  }
}


int uv_check_init(uv_check_t* check) {
  uv__handle_init((uv_handle_t*)check, UV_CHECK);
  uv_counters()->check_init++;

  ev_check_init(&check->check_watcher, uv__check);
  check->check_watcher.data = check;

  check->check_cb = NULL;

  return 0;
}


int uv_check_start(uv_check_t* check, uv_check_cb cb) {
  int was_active = ev_is_active(&check->check_watcher);

  check->check_cb = cb;

  ev_check_start(EV_DEFAULT_UC_ &check->check_watcher);

  if (!was_active) {
    ev_unref(EV_DEFAULT_UC);
  }

  return 0;
}


int uv_check_stop(uv_check_t* check) {
  int was_active = ev_is_active(&check->check_watcher);

  ev_check_stop(EV_DEFAULT_UC_ &check->check_watcher);

  if (was_active) {
    ev_ref(EV_DEFAULT_UC);
  }

  return 0;
}


static void uv__idle(EV_P_ ev_idle* w, int revents) {
  uv_idle_t* idle = (uv_idle_t*)(w->data);

  if (idle->idle_cb) {
    idle->idle_cb(idle, 0);
  }
}



int uv_idle_init(uv_idle_t* idle) {
  uv__handle_init((uv_handle_t*)idle, UV_IDLE);
  uv_counters()->idle_init++;

  ev_idle_init(&idle->idle_watcher, uv__idle);
  idle->idle_watcher.data = idle;

  idle->idle_cb = NULL;

  return 0;
}


int uv_idle_start(uv_idle_t* idle, uv_idle_cb cb) {
  int was_active = ev_is_active(&idle->idle_watcher);

  idle->idle_cb = cb;
  ev_idle_start(EV_DEFAULT_UC_ &idle->idle_watcher);

  if (!was_active) {
    ev_unref(EV_DEFAULT_UC);
  }

  return 0;
}


int uv_idle_stop(uv_idle_t* idle) {
  int was_active = ev_is_active(&idle->idle_watcher);

  ev_idle_stop(EV_DEFAULT_UC_ &idle->idle_watcher);

  if (was_active) {
    ev_ref(EV_DEFAULT_UC);
  }

  return 0;
}


int uv_is_active(uv_handle_t* handle) {
  switch (handle->type) {
    case UV_TIMER:
      return ev_is_active(&((uv_timer_t*)handle)->timer_watcher);

    case UV_PREPARE:
      return ev_is_active(&((uv_prepare_t*)handle)->prepare_watcher);

    case UV_CHECK:
      return ev_is_active(&((uv_check_t*)handle)->check_watcher);

    case UV_IDLE:
      return ev_is_active(&((uv_idle_t*)handle)->idle_watcher);

    default:
      return 1;
  }
}


static void uv__async(EV_P_ ev_async* w, int revents) {
  uv_async_t* async = w->data;

  if (async->async_cb) {
    async->async_cb(async, 0);
  }
}


int uv_async_init(uv_async_t* async, uv_async_cb async_cb) {
  uv__handle_init((uv_handle_t*)async, UV_ASYNC);
  uv_counters()->async_init++;

  ev_async_init(&async->async_watcher, uv__async);
  async->async_watcher.data = async;

  async->async_cb = async_cb;

  /* Note: This does not have symmetry with the other libev wrappers. */
  ev_async_start(EV_DEFAULT_UC_ &async->async_watcher);
  ev_unref(EV_DEFAULT_UC);

  return 0;
}


int uv_async_send(uv_async_t* async) {
  ev_async_send(EV_DEFAULT_UC_ &async->async_watcher);
  return 0;
}


static void uv__timer_cb(EV_P_ ev_timer* w, int revents) {
  uv_timer_t* timer = w->data;

  if (!ev_is_active(w)) {
    ev_ref(EV_DEFAULT_UC);
  }

  if (timer->timer_cb) {
    timer->timer_cb(timer, 0);
  }
}


int uv_timer_init(uv_timer_t* timer) {
  uv__handle_init((uv_handle_t*)timer, UV_TIMER);
  uv_counters()->timer_init++;

  ev_init(&timer->timer_watcher, uv__timer_cb);
  timer->timer_watcher.data = timer;

  return 0;
}


int uv_timer_start(uv_timer_t* timer, uv_timer_cb cb, int64_t timeout,
    int64_t repeat) {
  if (ev_is_active(&timer->timer_watcher)) {
    return -1;
  }

  timer->timer_cb = cb;
  ev_timer_set(&timer->timer_watcher, timeout / 1000.0, repeat / 1000.0);
  ev_timer_start(EV_DEFAULT_UC_ &timer->timer_watcher);
  ev_unref(EV_DEFAULT_UC);
  return 0;
}


int uv_timer_stop(uv_timer_t* timer) {
  if (ev_is_active(&timer->timer_watcher)) {
    ev_ref(EV_DEFAULT_UC);
  }

  ev_timer_stop(EV_DEFAULT_UC_ &timer->timer_watcher);
  return 0;
}


int uv_timer_again(uv_timer_t* timer) {
  if (!ev_is_active(&timer->timer_watcher)) {
    uv_err_new((uv_handle_t*)timer, EINVAL);
    return -1;
  }

  ev_timer_again(EV_DEFAULT_UC_ &timer->timer_watcher);
  return 0;
}

void uv_timer_set_repeat(uv_timer_t* timer, int64_t repeat) {
  assert(timer->type == UV_TIMER);
  timer->timer_watcher.repeat = repeat / 1000.0;
}

int64_t uv_timer_get_repeat(uv_timer_t* timer) {
  assert(timer->type == UV_TIMER);
  return (int64_t)(1000 * timer->timer_watcher.repeat);
}


/*
 * This is called once per second by ares_data.timer. It is used to
 * constantly callback into c-ares for possibly processing timeouts.
 */
static void uv__ares_timeout(EV_P_ struct ev_timer* watcher, int revents) {
  assert(watcher == &ares_data.timer);
  assert(revents == EV_TIMER);
  assert(!uv_ares_handles_empty());
  ares_process_fd(ares_data.channel, ARES_SOCKET_BAD, ARES_SOCKET_BAD);
}


static void uv__ares_io(EV_P_ struct ev_io* watcher, int revents) {
  /* Reset the idle timer */
  ev_timer_again(EV_A_ &ares_data.timer);

  /* Process DNS responses */
  ares_process_fd(ares_data.channel,
      revents & EV_READ ? watcher->fd : ARES_SOCKET_BAD,
      revents & EV_WRITE ? watcher->fd : ARES_SOCKET_BAD);
}


/* Allocates and returns a new uv_ares_task_t */
static uv_ares_task_t* uv__ares_task_create(int fd) {
  uv_ares_task_t* h = malloc(sizeof(uv_ares_task_t));

  if (h == NULL) {
    uv_fatal_error(ENOMEM, "malloc");
  }

  h->sock = fd;

  ev_io_init(&h->read_watcher, uv__ares_io, fd, EV_READ);
  ev_io_init(&h->write_watcher, uv__ares_io, fd, EV_WRITE);

  h->read_watcher.data = h;
  h->write_watcher.data = h;

  return h;
}


/* Callback from ares when socket operation is started */
static void uv__ares_sockstate_cb(void* data, ares_socket_t sock,
    int read, int write) {
  uv_ares_task_t* h = uv_find_ares_handle(sock);

  if (read || write) {
    if (!h) {
      /* New socket */

      /* If this is the first socket then start the timer. */
      if (!ev_is_active(&ares_data.timer)) {
        assert(uv_ares_handles_empty());
        ev_timer_again(EV_DEFAULT_UC_ &ares_data.timer);
      }

      h = uv__ares_task_create(sock);
      uv_add_ares_handle(h);
    }

    if (read) {
      ev_io_start(EV_DEFAULT_UC_ &h->read_watcher);
    } else {
      ev_io_stop(EV_DEFAULT_UC_ &h->read_watcher);
    }

    if (write) {
      ev_io_start(EV_DEFAULT_UC_ &h->write_watcher);
    } else {
      ev_io_stop(EV_DEFAULT_UC_ &h->write_watcher);
    }

  } else {
    /*
     * read == 0 and write == 0 this is c-ares's way of notifying us that
     * the socket is now closed. We must free the data associated with
     * socket.
     */
    assert(h && "When an ares socket is closed we should have a handle for it");

    ev_io_stop(EV_DEFAULT_UC_ &h->read_watcher);
    ev_io_stop(EV_DEFAULT_UC_ &h->write_watcher);

    uv_remove_ares_handle(h);
    free(h);

    if (uv_ares_handles_empty()) {
      ev_timer_stop(EV_DEFAULT_UC_ &ares_data.timer);
    }
  }
}


/* c-ares integration initialize and terminate */
/* TODO: share this with windows? */
int uv_ares_init_options(ares_channel *channelptr,
                         struct ares_options *options,
                         int optmask) {
  int rc;

  /* only allow single init at a time */
  if (ares_data.channel != NULL) {
    uv_err_new_artificial(NULL, UV_EALREADY);
    return -1;
  }

  /* set our callback as an option */
  options->sock_state_cb = uv__ares_sockstate_cb;
  options->sock_state_cb_data = &ares_data;
  optmask |= ARES_OPT_SOCK_STATE_CB;

  /* We do the call to ares_init_option for caller. */
  rc = ares_init_options(channelptr, options, optmask);

  /* if success, save channel */
  if (rc == ARES_SUCCESS) {
    ares_data.channel = *channelptr;
  }

  /*
   * Initialize the timeout timer. The timer won't be started until the
   * first socket is opened.
   */
  ev_init(&ares_data.timer, uv__ares_timeout);
  ares_data.timer.repeat = 1.0;

  return rc;
}


/* TODO share this with windows? */
void uv_ares_destroy(ares_channel channel) {
  /* only allow destroy if did init */
  if (ares_data.channel != NULL) {
    ev_timer_stop(EV_DEFAULT_UC_ &ares_data.timer);
    ares_destroy(channel);
    ares_data.channel = NULL;
  }
}


static int uv_getaddrinfo_done(eio_req* req) {
  uv_getaddrinfo_t* handle = req->data;

  uv_unref();

  free(handle->hints);
  free(handle->service);
  free(handle->hostname);

  if (handle->retcode != 0) {
    /* TODO how to display gai error strings? */
    uv_err_new(NULL, handle->retcode);
  }

  handle->cb(handle, handle->retcode, handle->res);

  freeaddrinfo(handle->res);
  handle->res = NULL;

  return 0;
}


static void getaddrinfo_thread_proc(eio_req *req) {
  uv_getaddrinfo_t* handle = req->data;

  handle->retcode = getaddrinfo(handle->hostname,
                                handle->service,
                                handle->hints,
                                &handle->res);
}


/* stub implementation of uv_getaddrinfo */
int uv_getaddrinfo(uv_getaddrinfo_t* handle,
                   uv_getaddrinfo_cb cb,
                   const char* hostname,
                   const char* service,
                   const struct addrinfo* hints) {
  eio_req* req;
  uv_eio_init();

  if (handle == NULL || cb == NULL ||
      (hostname == NULL && service == NULL)) {
    uv_err_new_artificial(NULL, UV_EINVAL);
    return -1;
  }

  memset(handle, 0, sizeof(uv_getaddrinfo_t));

  /* TODO don't alloc so much. */

  if (hints) {
    handle->hints = malloc(sizeof(struct addrinfo));
    memcpy(&handle->hints, hints, sizeof(struct addrinfo));
  }

  /* TODO security! check lengths, check return values. */

  handle->cb = cb;
  handle->hostname = hostname ? strdup(hostname) : NULL;
  handle->service = service ? strdup(service) : NULL;

  /* TODO check handle->hostname == NULL */
  /* TODO check handle->service == NULL */

  uv_ref();

  req = eio_custom(getaddrinfo_thread_proc, EIO_PRI_DEFAULT,
      uv_getaddrinfo_done, handle);
  assert(req);
  assert(req->data == handle);

  return 0;
}


int uv_pipe_init(uv_pipe_t* handle) {
  memset(handle, 0, sizeof *handle);

  uv__handle_init((uv_handle_t*)handle, UV_NAMED_PIPE);
  uv_counters()->pipe_init++;

  handle->type = UV_NAMED_PIPE;
  handle->pipe_fname = NULL; /* Only set by listener. */

  ev_init(&handle->write_watcher, uv__stream_io);
  ev_init(&handle->read_watcher, uv__stream_io);
  handle->write_watcher.data = handle;
  handle->read_watcher.data = handle;
  handle->accepted_fd = -1;
  handle->fd = -1;

  ngx_queue_init(&handle->write_completed_queue);
  ngx_queue_init(&handle->write_queue);

  return 0;
}


int uv_pipe_bind(uv_pipe_t* handle, const char* name) {
  struct sockaddr_un sun;
  const char* pipe_fname;
  int saved_errno;
  int sockfd;
  int status;
  int bound;

  saved_errno = errno;
  pipe_fname = NULL;
  sockfd = -1;
  status = -1;
  bound = 0;

  /* Already bound? */
  if (handle->fd >= 0) {
    uv_err_new_artificial((uv_handle_t*)handle, UV_EINVAL);
    goto out;
  }

  /* Make a copy of the file name, it outlives this function's scope. */
  if ((pipe_fname = strdup(name)) == NULL) {
    uv_err_new((uv_handle_t*)handle, ENOMEM);
    goto out;
  }

  /* We've got a copy, don't touch the original any more. */
  name = NULL;

  if ((sockfd = uv__socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    uv_err_new((uv_handle_t*)handle, errno);
    goto out;
  }

  memset(&sun, 0, sizeof sun);
  uv__strlcpy(sun.sun_path, pipe_fname, sizeof(sun.sun_path));
  sun.sun_family = AF_UNIX;

  if (bind(sockfd, (struct sockaddr*)&sun, sizeof sun) == -1) {
    /* On EADDRINUSE:
     *
     * We hold the file lock so there is no other process listening
     * on the socket. Ergo, it's stale - remove it.
     *
     * This assumes that the other process uses locking too
     * but that's a good enough assumption for now.
     */
    if (errno != EADDRINUSE
        || unlink(pipe_fname) == -1
        || bind(sockfd, (struct sockaddr*)&sun, sizeof sun) == -1) {
      /* Convert ENOENT to EACCES for compatibility with Windows. */
      uv_err_new((uv_handle_t*)handle, (errno == ENOENT) ? EACCES : errno);
      goto out;
    }
  }
  bound = 1;

  /* Success. */
  handle->pipe_fname = pipe_fname; /* Is a strdup'ed copy. */
  handle->fd = sockfd;
  status = 0;

out:
  /* Clean up on error. */
  if (status) {
    if (bound) {
      /* unlink() before close() to avoid races. */
      assert(pipe_fname != NULL);
      unlink(pipe_fname);
    }
    uv__close(sockfd);

    free((void*)pipe_fname);
  }

  errno = saved_errno;
  return status;
}


static int uv_pipe_listen(uv_pipe_t* handle, int backlog, uv_connection_cb cb) {
  int saved_errno;
  int status;

  saved_errno = errno;
  status = -1;

  if (handle->fd == -1) {
    uv_err_new_artificial((uv_handle_t*)handle, UV_EINVAL);
    goto out;
  }
  assert(handle->fd >= 0);

  if ((status = listen(handle->fd, backlog)) == -1) {
    uv_err_new((uv_handle_t*)handle, errno);
  } else {
    handle->connection_cb = cb;
    ev_io_init(&handle->read_watcher, uv__pipe_accept, handle->fd, EV_READ);
    ev_io_start(EV_DEFAULT_ &handle->read_watcher);
  }

out:
  errno = saved_errno;
  return status;
}


static int uv_pipe_cleanup(uv_pipe_t* handle) {
  int saved_errno;
  int status;

  saved_errno = errno;
  status = -1;

  if (handle->pipe_fname) {
    /*
     * Unlink the file system entity before closing the file descriptor.
     * Doing it the other way around introduces a race where our process
     * unlinks a socket with the same name that's just been created by
     * another thread or process.
     *
     * This is less of an issue now that we attach a file lock
     * to the socket but it's still a best practice.
     */
    unlink(handle->pipe_fname);
    free((void*)handle->pipe_fname);
  }

  errno = saved_errno;
  return status;
}


int uv_pipe_connect(uv_connect_t* req,
                    uv_pipe_t* handle,
                    const char* name,
                    uv_connect_cb cb) {
  struct sockaddr_un sun;
  int saved_errno;
  int sockfd;
  int status;
  int r;

  saved_errno = errno;
  sockfd = -1;
  status = -1;

  if ((sockfd = uv__socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    uv_err_new((uv_handle_t*)handle, errno);
    goto out;
  }

  memset(&sun, 0, sizeof sun);
  uv__strlcpy(sun.sun_path, name, sizeof(sun.sun_path));
  sun.sun_family = AF_UNIX;

  /* We don't check for EINPROGRESS. Think about it: the socket
   * is either there or not.
   */
  do {
    r = connect(sockfd, (struct sockaddr*)&sun, sizeof sun);
  }
  while (r == -1 && errno == EINTR);

  if (r == -1) {
    uv_err_new((uv_handle_t*)handle, errno);
    uv__close(sockfd);
    goto out;
  }

  uv__stream_open((uv_stream_t*)handle, sockfd, UV_READABLE | UV_WRITABLE);

  ev_io_start(EV_DEFAULT_ &handle->read_watcher);
  ev_io_start(EV_DEFAULT_ &handle->write_watcher);

  status = 0;

out:
  handle->delayed_error = status; /* Passed to callback. */
  handle->connect_req = req;
  req->handle = (uv_stream_t*)handle;
  req->type = UV_CONNECT;
  req->cb = cb;
  ngx_queue_init(&req->queue);

  /* Run callback on next tick. */
  ev_feed_event(EV_DEFAULT_ &handle->read_watcher, EV_CUSTOM);
  assert(ev_is_pending(&handle->read_watcher));

  /* Mimic the Windows pipe implementation, always
   * return 0 and let the callback handle errors.
   */
  errno = saved_errno;
  return 0;
}


/* TODO merge with uv__server_io()? */
static void uv__pipe_accept(EV_P_ ev_io* watcher, int revents) {
  struct sockaddr_un sun;
  uv_pipe_t* pipe;
  int saved_errno;
  int sockfd;

  saved_errno = errno;
  pipe = watcher->data;

  assert(pipe->type == UV_NAMED_PIPE);
  assert(pipe->pipe_fname != NULL);

  sockfd = uv__accept(pipe->fd, (struct sockaddr *)&sun, sizeof sun);
  if (sockfd == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      assert(0 && "EAGAIN on uv__accept(pipefd)");
    } else {
      uv_err_new((uv_handle_t*)pipe, errno);
    }
  } else {
    pipe->accepted_fd = sockfd;
    pipe->connection_cb((uv_stream_t*)pipe, 0);
    if (pipe->accepted_fd == sockfd) {
      /* The user hasn't yet accepted called uv_accept() */
      ev_io_stop(EV_DEFAULT_ &pipe->read_watcher);
    }
  }

  errno = saved_errno;
}


/* Open a socket in non-blocking close-on-exec mode, atomically if possible. */
static int uv__socket(int domain, int type, int protocol) {
#if defined(SOCK_NONBLOCK) && defined(SOCK_CLOEXEC)
  return socket(domain, type | SOCK_NONBLOCK | SOCK_CLOEXEC, protocol);
#else
  int sockfd;

  if ((sockfd = socket(domain, type, protocol)) == -1) {
    return -1;
  }

  if (uv__nonblock(sockfd, 1) == -1 || uv__cloexec(sockfd, 1) == -1) {
    uv__close(sockfd);
    return -1;
  }

  return sockfd;
#endif
}


static int uv__accept(int sockfd, struct sockaddr* saddr, socklen_t slen) {
  int peerfd;

  assert(sockfd >= 0);

  do {
#if defined(HAVE_ACCEPT4)
    peerfd = accept4(sockfd, saddr, &slen, SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
    if ((peerfd = accept(sockfd, saddr, &slen)) != -1) {
      if (uv__cloexec(peerfd, 1) == -1 || uv__nonblock(peerfd, 1) == -1) {
        uv__close(peerfd);
        return -1;
      }
    }
#endif
  }
  while (peerfd == -1 && errno == EINTR);

  return peerfd;
}


static int uv__close(int fd) {
  int status;

  /*
   * Retry on EINTR. You may think this is academic but on linux
   * and probably other Unices too, close(2) is interruptible.
   * Failing to handle EINTR is a common source of fd leaks.
   */
  do {
    status = close(fd);
  }
  while (status == -1 && errno == EINTR);

  return status;
}


static int uv__nonblock(int fd, int set) {
  int flags;

  if ((flags = fcntl(fd, F_GETFL)) == -1) {
    return -1;
  }

  if (set) {
    flags |= O_NONBLOCK;
  } else {
    flags &= ~O_NONBLOCK;
  }

  if (fcntl(fd, F_SETFL, flags) == -1) {
    return -1;
  }

  return 0;
}


static int uv__cloexec(int fd, int set) {
  int flags;

  if ((flags = fcntl(fd, F_GETFD)) == -1) {
    return -1;
  }

  if (set) {
    flags |= FD_CLOEXEC;
  } else {
    flags &= ~FD_CLOEXEC;
  }

  if (fcntl(fd, F_SETFD, flags) == -1) {
    return -1;
  }

  return 0;
}


/* TODO move to uv-common.c? */
size_t uv__strlcpy(char* dst, const char* src, size_t size) {
  const char *org;

  if (size == 0) {
    return 0;
  }

  org = src;
  while (size > 1) {
    if ((*dst++ = *src++) == '\0') {
      return org - src;
    }
  }
  *dst = '\0';

  return src - org;
}


uv_stream_t* uv_std_handle(uv_std_type type) {
  assert(0 && "implement me");
  return NULL;
}


static void uv__chld(EV_P_ ev_child* watcher, int revents) {
  int status = watcher->rstatus;
  int exit_status = 0;
  int term_signal = 0;
  uv_process_t *process = watcher->data;

  assert(&process->child_watcher == watcher);
  assert(revents & EV_CHILD);

  ev_child_stop(EV_A_ &process->child_watcher);

  if (WIFEXITED(status)) {
    exit_status = WEXITSTATUS(status);
  }

  if (WIFSIGNALED(status)) {
    term_signal = WTERMSIG(status);
  }

  if (process->exit_cb) {
    process->exit_cb(process, exit_status, term_signal);
  }
}

#ifndef SPAWN_WAIT_EXEC
# define SPAWN_WAIT_EXEC 1
#endif

int uv_spawn(uv_process_t* process, uv_process_options_t options) {
  /*
   * Save environ in the case that we get it clobbered
   * by the child process.
   */
  char** save_our_env = environ;
  int stdin_pipe[2] = { -1, -1 };
  int stdout_pipe[2] = { -1, -1 };
  int stderr_pipe[2] = { -1, -1 };
#if SPAWN_WAIT_EXEC
  int signal_pipe[2] = { -1, -1 };
  struct pollfd pfd;
#endif
  int status;
  pid_t pid;

  uv__handle_init((uv_handle_t*)process, UV_PROCESS);
  uv_counters()->process_init++;

  process->exit_cb = options.exit_cb;

  if (options.stdin_stream) {
    if (options.stdin_stream->type != UV_NAMED_PIPE) {
      errno = EINVAL;
      goto error;
    }

    if (pipe(stdin_pipe) < 0) {
      goto error;
    }
    uv__cloexec(stdin_pipe[0], 1);
    uv__cloexec(stdin_pipe[1], 1);
  }

  if (options.stdout_stream) {
    if (options.stdout_stream->type != UV_NAMED_PIPE) {
      errno = EINVAL;
      goto error;
    }

    if (pipe(stdout_pipe) < 0) {
      goto error;
    }
    uv__cloexec(stdout_pipe[0], 1);
    uv__cloexec(stdout_pipe[1], 1);
  }

  if (options.stderr_stream) {
    if (options.stderr_stream->type != UV_NAMED_PIPE) {
      errno = EINVAL;
      goto error;
    }

    if (pipe(stderr_pipe) < 0) {
      goto error;
    }
    uv__cloexec(stderr_pipe[0], 1);
    uv__cloexec(stderr_pipe[1], 1);
  }

  /* This pipe is used by the parent to wait until
   * the child has called `execve()`. We need this
   * to avoid the following race condition:
   *
   *    if ((pid = fork()) > 0) {
   *      kill(pid, SIGTERM);
   *    }
   *    else if (pid == 0) {
   *      execve("/bin/cat", argp, envp);
   *    }
   *
   * The parent sends a signal immediately after forking.
   * Since the child may not have called `execve()` yet,
   * there is no telling what process receives the signal,
   * our fork or /bin/cat.
   *
   * To avoid ambiguity, we create a pipe with both ends
   * marked close-on-exec. Then, after the call to `fork()`,
   * the parent polls the read end until it sees POLLHUP.
   */
#if SPAWN_WAIT_EXEC
# ifdef HAVE_PIPE2
  if (pipe2(signal_pipe, O_CLOEXEC | O_NONBLOCK) < 0) {
    goto error;
  }
# else
  if (pipe(signal_pipe) < 0) {
    goto error;
  }
  uv__cloexec(signal_pipe[0], 1);
  uv__cloexec(signal_pipe[1], 1);
  uv__nonblock(signal_pipe[0], 1);
  uv__nonblock(signal_pipe[1], 1);
# endif
#endif

  pid = fork();

  if (pid == -1) {
#if SPAWN_WAIT_EXEC
    uv__close(signal_pipe[0]);
    uv__close(signal_pipe[1]);
#endif
    environ = save_our_env;
    goto error;
  }

  if (pid == 0) {
    if (stdin_pipe[0] >= 0) {
      uv__close(stdin_pipe[1]);
      dup2(stdin_pipe[0],  STDIN_FILENO);
    }

    if (stdout_pipe[1] >= 0) {
      uv__close(stdout_pipe[0]);
      dup2(stdout_pipe[1], STDOUT_FILENO);
    }

    if (stderr_pipe[1] >= 0) {
      uv__close(stderr_pipe[0]);
      dup2(stderr_pipe[1], STDERR_FILENO);
    }

    if (options.cwd && chdir(options.cwd)) {
      perror("chdir()");
      _exit(127);
    }

    environ = options.env;

    execvp(options.file, options.args);
    perror("execvp()");
    _exit(127);
    /* Execution never reaches here. */
  }

  /* Parent. */

  /* Restore environment. */
  environ = save_our_env;

#if SPAWN_WAIT_EXEC
  /* POLLHUP signals child has exited or execve()'d. */
  uv__close(signal_pipe[1]);
  do {
    pfd.fd = signal_pipe[0];
    pfd.events = POLLIN|POLLHUP;
    pfd.revents = 0;
    errno = 0, status = poll(&pfd, 1, -1);
  }
  while (status == -1 && (errno == EINTR || errno == ENOMEM));

  uv__close(signal_pipe[0]);
  uv__close(signal_pipe[1]);

  assert((status == 1)
      && "poll() on pipe read end failed");
  assert((pfd.revents & POLLHUP) == POLLHUP
      && "no POLLHUP on pipe read end");
#endif

  process->pid = pid;

  ev_child_init(&process->child_watcher, uv__chld, pid, 0);
  ev_child_start(EV_DEFAULT_UC_ &process->child_watcher);
  process->child_watcher.data = process;

  if (stdin_pipe[1] >= 0) {
    assert(options.stdin_stream);
    assert(stdin_pipe[0] >= 0);
    uv__close(stdin_pipe[0]);
    uv__nonblock(stdin_pipe[1], 1);
    uv__stream_open((uv_stream_t*)options.stdin_stream, stdin_pipe[1],
        UV_WRITABLE);
  }

  if (stdout_pipe[0] >= 0) {
    assert(options.stdout_stream);
    assert(stdout_pipe[1] >= 0);
    uv__close(stdout_pipe[1]);
    uv__nonblock(stdout_pipe[0], 1);
    uv__stream_open((uv_stream_t*)options.stdout_stream, stdout_pipe[0],
        UV_READABLE);
  }

  if (stderr_pipe[0] >= 0) {
    assert(options.stderr_stream);
    assert(stderr_pipe[1] >= 0);
    uv__close(stderr_pipe[1]);
    uv__nonblock(stderr_pipe[0], 1);
    uv__stream_open((uv_stream_t*)options.stderr_stream, stderr_pipe[0],
        UV_READABLE);
  }

  return 0;

error:
  uv_err_new((uv_handle_t*)process, errno);
  uv__close(stdin_pipe[0]);
  uv__close(stdin_pipe[1]);
  uv__close(stdout_pipe[0]);
  uv__close(stdout_pipe[1]);
  uv__close(stderr_pipe[0]);
  uv__close(stderr_pipe[1]);
  return -1;
}


int uv_process_kill(uv_process_t* process, int signum) {
  int r = kill(process->pid, signum);

  if (r) {
    uv_err_new((uv_handle_t*)process, errno);
    return -1;
  } else {
    return 0;
  }
}
