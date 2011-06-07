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

#include <stddef.h> /* NULL */
#include <stdio.h> /* printf */
#include <stdlib.h>
#include <string.h> /* strerror */
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h> /* PATH_MAX */

#if defined(__APPLE__)
#include <mach-o/dyld.h> /* _NSGetExecutablePath */
#endif

#if defined(__FreeBSD__)
#include <sys/sysctl.h>
#endif

static uv_err_t last_err;


void uv__tcp_io(EV_P_ ev_io* watcher, int revents);
void uv__next(EV_P_ ev_idle* watcher, int revents);
static void uv__tcp_connect(uv_tcp_t*);
int uv_tcp_open(uv_tcp_t*, int fd);
static void uv__finish_close(uv_handle_t* handle);


/* flags */
enum {
  UV_CLOSING  = 0x00000001, /* uv_close() called but not finished. */
  UV_CLOSED   = 0x00000002, /* close(2) finished. */
  UV_READING  = 0x00000004, /* uv_read_start() called. */
  UV_SHUTTING = 0x00000008, /* uv_shutdown() called but not complete. */
  UV_SHUT     = 0x00000010  /* Write side closed. */
};


void uv_flag_set(uv_handle_t* handle, int flag) {
  handle->flags |= flag;
}


uv_err_t uv_last_error() {
  return last_err;
}


char* uv_strerror(uv_err_t err) {
  return strerror(err.sys_errno_);
}


void uv_flag_unset(uv_handle_t* handle, int flag) {
  handle->flags = handle->flags & ~flag;
}


int uv_flag_is_set(uv_handle_t* handle, int flag) {
  return (handle->flags & flag) != 0;
}


static uv_err_code uv_translate_sys_error(int sys_errno) {
  switch (sys_errno) {
    case 0: return UV_OK;
    case EACCES: return UV_EACCESS;
    case EAGAIN: return UV_EAGAIN;
    case ECONNRESET: return UV_ECONNRESET;
    case EFAULT: return UV_EFAULT;
    case EMFILE: return UV_EMFILE;
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


struct sockaddr_in uv_ip4_addr(char* ip, int port) {
  struct sockaddr_in addr;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = inet_addr(ip);

  return addr;
}


int uv_close(uv_handle_t* handle) {
  uv_tcp_t* tcp;
  uv_async_t* async;
  uv_timer_t* timer;

  switch (handle->type) {
    case UV_TCP:
      tcp = (uv_tcp_t*) handle;
      ev_io_stop(EV_DEFAULT_ &tcp->write_watcher);
      ev_io_stop(EV_DEFAULT_ &tcp->read_watcher);
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

    default:
      assert(0);
      return -1;
  }

  uv_flag_set(handle, UV_CLOSING);

  /* This is used to call the on_close callback in the next loop. */
  ev_idle_start(EV_DEFAULT_ &handle->next_watcher);
  ev_feed_event(EV_DEFAULT_ &handle->next_watcher, EV_IDLE);
  assert(ev_is_pending(&handle->next_watcher));

  return 0;
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


static void uv__handle_init(uv_handle_t* handle, uv_handle_type type,
    uv_close_cb close_cb, void* data) {
  handle->type = type;
  handle->close_cb = close_cb;
  handle->data = data;
  handle->flags = 0;

  ev_init(&handle->next_watcher, uv__next);
  handle->next_watcher.data = handle;

  /* Ref the loop until this handle is closed. See uv__finish_close. */
  ev_ref(EV_DEFAULT_UC);
}


int uv_tcp_init(uv_tcp_t* tcp, uv_close_cb close_cb, void* data) {
  uv__handle_init((uv_handle_t*)tcp, UV_TCP, close_cb, data);

  tcp->alloc_cb = NULL;
  tcp->connect_req = NULL;
  tcp->accepted_fd = -1;
  tcp->fd = -1;
  tcp->delayed_error = 0;
  ngx_queue_init(&tcp->write_queue);
  tcp->write_queue_size = 0;

  ev_init(&tcp->read_watcher, uv__tcp_io);
  tcp->read_watcher.data = tcp;

  ev_init(&tcp->write_watcher, uv__tcp_io);
  tcp->write_watcher.data = tcp;

  assert(ngx_queue_empty(&tcp->write_queue));
  assert(tcp->write_queue_size == 0);

  return 0;
}


int uv_bind(uv_tcp_t* tcp, struct sockaddr_in addr) {
  int addrsize = sizeof(struct sockaddr_in);
  int domain = AF_INET;
  int r;

  if (tcp->fd <= 0) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
      uv_err_new((uv_handle_t*)tcp, errno);
      return -1;
    }

    if (uv_tcp_open(tcp, fd)) {
      close(fd);
      return -2;
    }
  }

  assert(tcp->fd >= 0);

  if (addr.sin_family != AF_INET) {
    uv_err_new((uv_handle_t*)tcp, EFAULT);
    return -1;
  }

  r = bind(tcp->fd, (struct sockaddr*) &addr, addrsize);
  tcp->delayed_error = 0;

  if (r) {
    switch (errno) {
      case EADDRINUSE:
        tcp->delayed_error = errno;
        return 0;

      default:
        uv_err_new((uv_handle_t*)tcp, errno);
        return -1;
    }
  }

  return 0;
}


int uv_tcp_open(uv_tcp_t* tcp, int fd) {
  int yes;
  int r;

  assert(fd >= 0);
  tcp->fd = fd;

  /* Set non-blocking. */
  yes = 1;
  r = fcntl(fd, F_SETFL, O_NONBLOCK);
  assert(r == 0);

  /* Reuse the port address. */
  r = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  assert(r == 0);

  /* Associate the fd with each ev_io watcher. */
  ev_io_set(&tcp->read_watcher, fd, EV_READ);
  ev_io_set(&tcp->write_watcher, fd, EV_WRITE);

  /* These should have been set up by uv_tcp_init. */
  assert(tcp->next_watcher.data == tcp);
  assert(tcp->write_watcher.data == tcp);
  assert(tcp->read_watcher.data == tcp);
  assert(tcp->read_watcher.cb == uv__tcp_io);
  assert(tcp->write_watcher.cb == uv__tcp_io);

  return 0;
}


void uv__server_io(EV_P_ ev_io* watcher, int revents) {
  int fd;
  struct sockaddr addr;
  socklen_t addrlen;
  uv_tcp_t* tcp = watcher->data;

  assert(watcher == &tcp->read_watcher ||
         watcher == &tcp->write_watcher);
  assert(revents == EV_READ);

  assert(!uv_flag_is_set((uv_handle_t*)tcp, UV_CLOSING));

  if (tcp->accepted_fd >= 0) {
    ev_io_stop(EV_DEFAULT_ &tcp->read_watcher);
    return;
  }

  while (1) {
    assert(tcp->accepted_fd < 0);
    fd = accept(tcp->fd, &addr, &addrlen);

    if (fd < 0) {
      if (errno == EAGAIN) {
        /* No problem. */
        return;
      } else if (errno == EMFILE) {
        /* TODO special trick. unlock reserved socket, accept, close. */
        return;
      } else {
        uv_err_new((uv_handle_t*)tcp, errno);
        tcp->connection_cb(tcp, -1);
      }

    } else {
      tcp->accepted_fd = fd;
      tcp->connection_cb(tcp, 0);
      if (tcp->accepted_fd >= 0) {
        /* The user hasn't yet accepted called uv_accept() */
        ev_io_stop(EV_DEFAULT_ &tcp->read_watcher);
        return;
      }
    }
  }
}


int uv_accept(uv_tcp_t* server, uv_tcp_t* client,
    uv_close_cb close_cb, void* data) {
  if (server->accepted_fd < 0) {
    return -1;
  }

  if (uv_tcp_init(client, close_cb, data)) {
    return -1;
  }

  if (uv_tcp_open(client, server->accepted_fd)) {
    /* Ignore error for now */
    server->accepted_fd = -1;
    close(server->accepted_fd);
    return -1;
  } else {
    server->accepted_fd = -1;
    ev_io_start(EV_DEFAULT_ &server->read_watcher);
    return 0;
  }
}


int uv_listen(uv_tcp_t* tcp, int backlog, uv_connection_cb cb) {
  int r;

  assert(tcp->fd >= 0);

  if (tcp->delayed_error) {
    uv_err_new((uv_handle_t*)tcp, tcp->delayed_error);
    return -1;
  }

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
  uv_tcp_t* tcp;

  assert(uv_flag_is_set(handle, UV_CLOSING));
  assert(!uv_flag_is_set(handle, UV_CLOSED));
  uv_flag_set(handle, UV_CLOSED);

  switch (handle->type) {
    case UV_TCP:
      /* XXX Is it necessary to stop these watchers here? weren't they
       * supposed to be stopped in uv_close()?
       */
      tcp = (uv_tcp_t*)handle;
      ev_io_stop(EV_DEFAULT_ &tcp->write_watcher);
      ev_io_stop(EV_DEFAULT_ &tcp->read_watcher);

      assert(!ev_is_active(&tcp->read_watcher));
      assert(!ev_is_active(&tcp->write_watcher));

      close(tcp->fd);
      tcp->fd = -1;

      if (tcp->accepted_fd >= 0) {
        close(tcp->accepted_fd);
        tcp->accepted_fd = -1;
      }
      break;

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

    default:
      assert(0);
      break;
  }

  ev_idle_stop(EV_DEFAULT_ &handle->next_watcher);

  if (handle->close_cb) {
    handle->close_cb(handle, 0);
  }

  ev_unref(EV_DEFAULT_UC);
}


uv_req_t* uv_write_queue_head(uv_tcp_t* tcp) {
  ngx_queue_t* q;
  uv_req_t* req;

  if (ngx_queue_empty(&tcp->write_queue)) {
    return NULL;
  }

  q = ngx_queue_head(&tcp->write_queue);
  if (!q) {
    return NULL;
  }

  req = ngx_queue_data(q, struct uv_req_s, queue);
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
  assert(uv_flag_is_set(handle, UV_CLOSING));
  uv__finish_close(handle);
}


static void uv__drain(uv_tcp_t* tcp) {
  uv_req_t* req;
  uv_shutdown_cb cb;

  assert(!uv_write_queue_head(tcp));
  assert(tcp->write_queue_size == 0);

  ev_io_stop(EV_DEFAULT_ &tcp->write_watcher);

  /* Shutdown? */
  if (uv_flag_is_set((uv_handle_t*)tcp, UV_SHUTTING) &&
      !uv_flag_is_set((uv_handle_t*)tcp, UV_CLOSING) &&
      !uv_flag_is_set((uv_handle_t*)tcp, UV_SHUT)) {
    assert(tcp->shutdown_req);

    req = tcp->shutdown_req;
    cb = (uv_shutdown_cb)req->cb;

    if (shutdown(tcp->fd, SHUT_WR)) {
      /* Error. Report it. User should call uv_close(). */
      uv_err_new((uv_handle_t*)tcp, errno);
      if (cb) cb(req, -1);
    } else {
      uv_err_new((uv_handle_t*)tcp, 0);
      uv_flag_set((uv_handle_t*)tcp, UV_SHUT);
      if (cb) cb(req, 0);
    }
  }
}


void uv__write(uv_tcp_t* tcp) {
  uv_req_t* req;
  struct iovec* iov;
  int iovcnt;
  ssize_t n;
  uv_write_cb cb;

  assert(tcp->fd >= 0);

  /* TODO: should probably while(1) here until EAGAIN */

  /* Get the request at the head of the queue. */
  req = uv_write_queue_head(tcp);
  if (!req) {
    assert(tcp->write_queue_size == 0);
    uv__drain(tcp);
    return;
  }

  assert(req->handle == (uv_handle_t*)tcp);

  /* Cast to iovec. We had to have our own uv_buf_t instead of iovec
   * because Windows's WSABUF is not an iovec.
   */
  assert(sizeof(uv_buf_t) == sizeof(struct iovec));
  iov = (struct iovec*) &(req->bufs[req->write_index]);
  iovcnt = req->bufcnt - req->write_index;

  /* Now do the actual writev. Note that we've been updating the pointers
   * inside the iov each time we write. So there is no need to offset it.
   */

  n = writev(tcp->fd, iov, iovcnt);

  cb = (uv_write_cb)req->cb;

  if (n < 0) {
    if (errno != EAGAIN) {
      uv_err_t err = uv_err_new((uv_handle_t*)tcp, errno);

      if (cb) {
        cb(req, -1);
      }
      return;
    }
  } else {
    /* Successful write */

    /* The loop updates the counters. */
    while (n > 0) {
      uv_buf_t* buf = &(req->bufs[req->write_index]);
      size_t len = buf->len;

      assert(req->write_index < req->bufcnt);

      if (n < len) {
        buf->base += n;
        buf->len -= n;
        tcp->write_queue_size -= n;
        n = 0;

        /* There is more to write. Break and ensure the watcher is pending. */
        break;

      } else {
        /* Finished writing the buf at index req->write_index. */
        req->write_index++;

        assert(n >= len);
        n -= len;

        assert(tcp->write_queue_size >= len);
        tcp->write_queue_size -= len;

        if (req->write_index == req->bufcnt) {
          /* Then we're done! */
          assert(n == 0);

          /* Pop the req off tcp->write_queue. */
          ngx_queue_remove(&req->queue);
          free(req->bufs); /* FIXME: we should not be allocing for each read */
          req->bufs = NULL;

          /* NOTE: call callback AFTER freeing the request data. */
          if (cb) {
            cb(req, 0);
          }

          if (!ngx_queue_empty(&tcp->write_queue)) {
            assert(tcp->write_queue_size > 0);
          } else {
            /* Write queue drained. */
            uv__drain(tcp);
          }

          return;
        }
      }
    }
  }

  /* Either we've counted n down to zero or we've got EAGAIN. */
  assert(n == 0 || n == -1);

  /* We're not done yet. */
  assert(ev_is_active(&tcp->write_watcher));
  ev_io_start(EV_DEFAULT_ &tcp->write_watcher);
}


void uv__read(uv_tcp_t* tcp) {
  uv_buf_t buf;
  struct iovec* iov;
  ssize_t nread;

  /* XXX: Maybe instead of having UV_READING we just test if
   * tcp->read_cb is NULL or not?
   */
  while (tcp->read_cb && uv_flag_is_set((uv_handle_t*)tcp, UV_READING)) {
    assert(tcp->alloc_cb);
    buf = tcp->alloc_cb(tcp, 64 * 1024);

    assert(buf.len > 0);
    assert(buf.base);

    iov = (struct iovec*) &buf;

    nread = readv(tcp->fd, iov, 1);

    if (nread < 0) {
      /* Error */
      if (errno == EAGAIN) {
        /* Wait for the next one. */
        if (uv_flag_is_set((uv_handle_t*)tcp, UV_READING)) {
          ev_io_start(EV_DEFAULT_UC_ &tcp->read_watcher);
        }
        uv_err_new((uv_handle_t*)tcp, EAGAIN);
        tcp->read_cb(tcp, 0, buf);
        return;
      } else {
        /* Error. User should call uv_close(). */
        uv_err_new((uv_handle_t*)tcp, errno);
        tcp->read_cb(tcp, -1, buf);
        assert(!ev_is_active(&tcp->read_watcher));
        return;
      }
    } else if (nread == 0) {
      /* EOF */
      uv_err_new_artificial((uv_handle_t*)tcp, UV_EOF);
      ev_io_stop(EV_DEFAULT_UC_ &tcp->read_watcher);
      tcp->read_cb(tcp, -1, buf);
      return;
    } else {
      /* Successful read */
      tcp->read_cb(tcp, nread, buf);
    }
  }
}


int uv_shutdown(uv_req_t* req) {
  uv_tcp_t* tcp = (uv_tcp_t*)req->handle;
  assert(tcp->fd >= 0);
  assert(tcp->type == UV_TCP);

  if (uv_flag_is_set((uv_handle_t*)tcp, UV_SHUT) ||
      uv_flag_is_set((uv_handle_t*)tcp, UV_CLOSED) ||
      uv_flag_is_set((uv_handle_t*)tcp, UV_CLOSING)) {
    return -1;
  }

  tcp->shutdown_req = req;
  req->type = UV_SHUTDOWN;

  uv_flag_set((uv_handle_t*)tcp, UV_SHUTTING);

  ev_io_start(EV_DEFAULT_UC_ &tcp->write_watcher);

  return 0;
}


void uv__tcp_io(EV_P_ ev_io* watcher, int revents) {
  uv_tcp_t* tcp = watcher->data;
  assert(watcher == &tcp->read_watcher ||
         watcher == &tcp->write_watcher);

  assert(tcp->fd >= 0);

  assert(!uv_flag_is_set((uv_handle_t*)tcp, UV_CLOSING));

  if (tcp->connect_req) {
    uv__tcp_connect(tcp);
  } else {
    if (revents & EV_READ) {
      uv__read(tcp);
    }

    if (revents & EV_WRITE) {
      uv__write(tcp);
    }
  }
}


/**
 * We get called here from directly following a call to connect(2).
 * In order to determine if we've errored out or succeeded must call
 * getsockopt.
 */
static void uv__tcp_connect(uv_tcp_t* tcp) {
  int error;
  uv_req_t* req;
  uv_connect_cb connect_cb;
  socklen_t errorsize = sizeof(int);

  assert(tcp->fd >= 0);

  req = tcp->connect_req;
  assert(req);

  if (tcp->delayed_error) {
    /* To smooth over the differences between unixes errors that
     * were reported synchronously on the first connect can be delayed
     * until the next tick--which is now.
     */
    error = tcp->delayed_error;
    tcp->delayed_error = 0;
  } else {
    /* Normal situation: we need to get the socket error from the kernel. */
    getsockopt(tcp->fd, SOL_SOCKET, SO_ERROR, &error, &errorsize);
  }

  if (!error) {
    ev_io_start(EV_DEFAULT_ &tcp->read_watcher);

    /* Successful connection */
    tcp->connect_req = NULL;
    connect_cb = (uv_connect_cb) req->cb;
    if (connect_cb) {
      connect_cb(req, 0);
    }

  } else if (error == EINPROGRESS) {
    /* Still connecting. */
    return;
  } else {
    /* Error */
    uv_err_t err = uv_err_new((uv_handle_t*)tcp, error);

    tcp->connect_req = NULL;

    connect_cb = (uv_connect_cb) req->cb;
    if (connect_cb) {
      connect_cb(req, -1);
    }
  }
}


int uv_connect(uv_req_t* req, struct sockaddr_in addr) {
  uv_tcp_t* tcp = (uv_tcp_t*)req->handle;
  int addrsize;
  int r;

  if (tcp->fd <= 0) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
      uv_err_new((uv_handle_t*)tcp, errno);
      return -1;
    }

    if (uv_tcp_open(tcp, fd)) {
      close(fd);
      return -2;
    }
  }

  req->type = UV_CONNECT;
  ngx_queue_init(&req->queue);

  if (tcp->connect_req) {
    uv_err_new((uv_handle_t*)tcp, EALREADY);
    return -1;
  }

  if (tcp->type != UV_TCP) {
    uv_err_new((uv_handle_t*)tcp, ENOTSOCK);
    return -1;
  }

  tcp->connect_req = req;

  addrsize = sizeof(struct sockaddr_in);
  assert(addr.sin_family == AF_INET);

  r = connect(tcp->fd, (struct sockaddr*)&addr, addrsize);
  tcp->delayed_error = 0;

  if (r != 0 && errno != EINPROGRESS) {
    switch (errno) {
      /* If we get a ECONNREFUSED wait until the next tick to report the
       * error. Solaris wants to report immediately--other unixes want to
       * wait.
       */
      case ECONNREFUSED:
        tcp->delayed_error = errno;
        break;

      default:
        uv_err_new((uv_handle_t*)tcp, errno);
        return -1;
    }
  }

  assert(tcp->write_watcher.data == tcp);
  ev_io_start(EV_DEFAULT_ &tcp->write_watcher);

  if (tcp->delayed_error) {
    ev_feed_event(EV_DEFAULT_ &tcp->write_watcher, EV_WRITE);
  }

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
int uv_write(uv_req_t* req, uv_buf_t bufs[], int bufcnt) {
  uv_tcp_t* tcp = (uv_tcp_t*)req->handle;
  assert(tcp->fd >= 0);

  ngx_queue_init(&req->queue);
  req->type = UV_WRITE;

  /* TODO: Don't malloc for each write... */
  req->bufs = malloc(sizeof(uv_buf_t) * bufcnt);
  memcpy(req->bufs, bufs, bufcnt * sizeof(uv_buf_t));
  req->bufcnt = bufcnt;

  req->write_index = 0;
  tcp->write_queue_size += uv__buf_count(bufs, bufcnt);

  /* Append the request to write_queue. */
  ngx_queue_insert_tail(&tcp->write_queue, &req->queue);

  assert(!ngx_queue_empty(&tcp->write_queue));
  assert(tcp->write_watcher.cb == uv__tcp_io);
  assert(tcp->write_watcher.data == tcp);
  assert(tcp->write_watcher.fd == tcp->fd);

  ev_io_start(EV_DEFAULT_ &tcp->write_watcher);

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


int uv_read_start(uv_tcp_t* tcp, uv_alloc_cb alloc_cb, uv_read_cb read_cb) {
  /* The UV_READING flag is irrelevant of the state of the tcp - it just
   * expresses the desired state of the user.
   */
  uv_flag_set((uv_handle_t*)tcp, UV_READING);

  /* TODO: try to do the read inline? */
  /* TODO: keep track of tcp state. If we've gotten a EOF then we should
   * not start the IO watcher.
   */
  assert(tcp->fd >= 0);
  assert(alloc_cb);

  tcp->read_cb = read_cb;
  tcp->alloc_cb = alloc_cb;

  /* These should have been set by uv_tcp_init. */
  assert(tcp->read_watcher.data == tcp);
  assert(tcp->read_watcher.cb == uv__tcp_io);

  ev_io_start(EV_DEFAULT_UC_ &tcp->read_watcher);
  return 0;
}


int uv_read_stop(uv_tcp_t* tcp) {
  uv_flag_unset((uv_handle_t*)tcp, UV_READING);

  ev_io_stop(EV_DEFAULT_UC_ &tcp->read_watcher);
  tcp->read_cb = NULL;
  tcp->alloc_cb = NULL;
  return 0;
}


void uv_req_init(uv_req_t* req, uv_handle_t* handle, void* cb) {
  req->type = UV_UNKNOWN_REQ;
  req->cb = cb;
  req->handle = handle;
  ngx_queue_init(&req->queue);
}


static void uv__prepare(EV_P_ ev_prepare* w, int revents) {
  uv_prepare_t* prepare = w->data;

  if (prepare->prepare_cb) {
    prepare->prepare_cb((uv_handle_t*)prepare, 0);
  }
}


int uv_prepare_init(uv_prepare_t* prepare, uv_close_cb close_cb, void* data) {
  uv__handle_init((uv_handle_t*)prepare, UV_PREPARE, close_cb, data);

  ev_prepare_init(&prepare->prepare_watcher, uv__prepare);
  prepare->prepare_watcher.data = prepare;

  prepare->prepare_cb = NULL;

  return 0;
}


int uv_prepare_start(uv_prepare_t* prepare, uv_loop_cb cb) {
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
    check->check_cb((uv_handle_t*)check, 0);
  }
}


int uv_check_init(uv_check_t* check, uv_close_cb close_cb, void* data) {
  uv__handle_init((uv_handle_t*)check, UV_CHECK, close_cb, data);

  ev_check_init(&check->check_watcher, uv__check);
  check->check_watcher.data = check;

  check->check_cb = NULL;

  return 0;
}


int uv_check_start(uv_check_t* check, uv_loop_cb cb) {
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
    idle->idle_cb((uv_handle_t*)idle, 0);
  }
}



int uv_idle_init(uv_idle_t* idle, uv_close_cb close_cb, void* data) {
  uv__handle_init((uv_handle_t*)idle, UV_IDLE, close_cb, data);

  ev_idle_init(&idle->idle_watcher, uv__idle);
  idle->idle_watcher.data = idle;

  idle->idle_cb = NULL;

  return 0;
}


int uv_idle_start(uv_idle_t* idle, uv_loop_cb cb) {
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
    async->async_cb((uv_handle_t*)async, 0);
  }
}


int uv_async_init(uv_async_t* async, uv_async_cb async_cb,
    uv_close_cb close_cb, void* data) {
  uv__handle_init((uv_handle_t*)async, UV_ASYNC, close_cb, data);

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
}


static void uv__timer_cb(EV_P_ ev_timer* w, int revents) {
  uv_timer_t* timer = w->data;

  if (!ev_is_active(w)) {
    ev_ref(EV_DEFAULT_UC);
  }

  if (timer->timer_cb) {
    timer->timer_cb((uv_handle_t*)timer, 0);
  }
}


int uv_timer_init(uv_timer_t* timer, uv_close_cb close_cb, void* data) {
  uv__handle_init((uv_handle_t*)timer, UV_TIMER, close_cb, data);

  ev_init(&timer->timer_watcher, uv__timer_cb);
  timer->timer_watcher.data = timer;

  return 0;
}


int uv_timer_start(uv_timer_t* timer, uv_loop_cb cb, int64_t timeout,
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


int uv_get_exepath(char* buffer, size_t* size) {
  uint32_t usize;
  int result;
  char* path;
  char* fullpath;

  if (!buffer || !size) {
    return -1;
  }

#if defined(__APPLE__)
  usize = *size;
  result = _NSGetExecutablePath(buffer, &usize);
  if (result) return result;

  path = (char*)malloc(2 * PATH_MAX);
  fullpath = realpath(buffer, path);

  if (fullpath == NULL) {
    free(path);
    return -1;
  }

  strncpy(buffer, fullpath, *size);
  free(fullpath);
  *size = strlen(buffer);
  return 0;
#elif defined(__linux__)
  *size = readlink("/proc/self/exe", buffer, *size - 1);
  if (*size <= 0) return -1;
  buffer[*size] = '\0';
  return 0;
#elif defined(__FreeBSD__)
  int mib[4];

  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = -1;

  size_t cb = *size;
  if (sysctl(mib, 4, buffer, &cb, NULL, 0) < 0) {
	  *size = 0;
	  return -1;
  }
  *size = strlen(buffer);

  return 0;
#else
  assert(0 && "implement me");
  /* Need to return argv[0] */
#endif
}


