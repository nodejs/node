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


static uv_err_t last_err;
static uv_alloc_cb alloc_cb;


void uv__tcp_io(EV_P_ ev_io* watcher, int revents);
void uv__next(EV_P_ ev_idle* watcher, int revents);
static void uv__tcp_connect(uv_handle_t* handle);
int uv_tcp_open(uv_handle_t*, int fd);
static void uv__finish_close(uv_handle_t* handle);


/* flags */
enum {
  UV_CLOSING  = 0x00000001, /* uv_close() called but not finished. */
  UV_CLOSED   = 0x00000002, /* close(2) finished. */
  UV_READING  = 0x00000004, /* uv_read_start() called. */
  UV_SHUTTING = 0x00000008, /* uv_shutdown() called but not complete. */
  UV_SHUT     = 0x00000010, /* Write side closed. */
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
  switch (handle->type) {
    case UV_TCP:
      ev_io_stop(EV_DEFAULT_ &handle->write_watcher);
      ev_io_stop(EV_DEFAULT_ &handle->read_watcher);
      break;

    case UV_PREPARE:
      uv_prepare_stop(handle);
      break;

    case UV_CHECK:
      uv_check_stop(handle);
      break;

    case UV_IDLE:
      uv_idle_stop(handle);
      break;

    case UV_ASYNC:
      ev_async_stop(EV_DEFAULT_ &handle->async_watcher);
      ev_ref(EV_DEFAULT_UC);
      break;

    case UV_TIMER:
      if (ev_is_active(&handle->timer_watcher)) {
        ev_ref(EV_DEFAULT_UC);
      }
      ev_timer_stop(EV_DEFAULT_ &handle->timer_watcher);
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


void uv_init(uv_alloc_cb cb) {
  assert(cb);
  alloc_cb = cb;

  // Initialize the default ev loop.
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


int uv_tcp_init(uv_handle_t* handle, uv_close_cb close_cb,
    void* data) {
  uv__handle_init(handle, UV_TCP, close_cb, data);

  handle->connect_req = NULL;
  handle->accepted_fd = -1;
  handle->fd = -1;
  handle->delayed_error = 0;
  ngx_queue_init(&handle->write_queue);
  handle->write_queue_size = 0;

  ev_init(&handle->read_watcher, uv__tcp_io);
  handle->read_watcher.data = handle;

  ev_init(&handle->write_watcher, uv__tcp_io);
  handle->write_watcher.data = handle;

  assert(ngx_queue_empty(&handle->write_queue));
  assert(handle->write_queue_size == 0);

  return 0;
}


int uv_bind(uv_handle_t* handle, struct sockaddr* addr) {
  int addrsize;
  int domain;
  int r;

  if (handle->fd <= 0) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      uv_err_new(handle, errno);
      return -1;
    }

    if (uv_tcp_open(handle, fd)) {
      close(fd);
      return -2;
    }
  }

  assert(handle->fd >= 0);

  if (addr->sa_family == AF_INET) {
    addrsize = sizeof(struct sockaddr_in);
    domain = AF_INET;
  } else if (addr->sa_family == AF_INET6) {
    addrsize = sizeof(struct sockaddr_in6);
    domain = AF_INET6;
  } else {
    uv_err_new(handle, EFAULT);
    return -1;
  }

  r = bind(handle->fd, addr, addrsize);
  handle->delayed_error = 0;

  if (r) {
    switch (errno) {
      case EADDRINUSE:
        handle->delayed_error = errno;
        return 0;

      default:
        uv_err_new(handle, errno);
        return -1;
    }
  }

  return 0;
}


int uv_tcp_open(uv_handle_t* handle, int fd) {
  assert(fd >= 0);
  handle->fd = fd;

  /* Set non-blocking. */
  int yes = 1;
  int r = fcntl(fd, F_SETFL, O_NONBLOCK);
  assert(r == 0);

  /* Reuse the port address. */
  r = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
  assert(r == 0);

  /* Associate the fd with each ev_io watcher. */
  ev_io_set(&handle->read_watcher, fd, EV_READ);
  ev_io_set(&handle->write_watcher, fd, EV_WRITE);

  /* These should have been set up by uv_tcp_init. */
  assert(handle->next_watcher.data == handle);
  assert(handle->write_watcher.data == handle);
  assert(handle->read_watcher.data == handle);
  assert(handle->read_watcher.cb == uv__tcp_io);
  assert(handle->write_watcher.cb == uv__tcp_io);

  return 0;
}


void uv__server_io(EV_P_ ev_io* watcher, int revents) {
  uv_handle_t* handle = watcher->data;
  assert(watcher == &handle->read_watcher ||
         watcher == &handle->write_watcher);
  assert(revents == EV_READ);

  assert(!uv_flag_is_set(handle, UV_CLOSING));

  if (handle->accepted_fd >= 0) {
    ev_io_stop(EV_DEFAULT_ &handle->read_watcher);
    return;
  }

  while (1) {
    struct sockaddr addr = { 0 };
    socklen_t addrlen = 0;

    assert(handle->accepted_fd < 0);
    int fd = accept(handle->fd, &addr, &addrlen);

    if (fd < 0) {
      if (errno == EAGAIN) {
        /* No problem. */
        return;
      } else if (errno == EMFILE) {
        /* TODO special trick. unlock reserved socket, accept, close. */
        return;
      } else {
        uv_err_new(handle, errno);
        uv_close(handle);
      }

    } else {
      handle->accepted_fd = fd;
      handle->accept_cb(handle);
      if (handle->accepted_fd >= 0) {
        /* The user hasn't yet accepted called uv_accept() */
        ev_io_stop(EV_DEFAULT_ &handle->read_watcher);
        return;
      }
    }
  }
}


int uv_accept(uv_handle_t* server, uv_handle_t* client,
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


int uv_listen(uv_handle_t* handle, int backlog, uv_accept_cb cb) {
  assert(handle->fd >= 0);

  if (handle->delayed_error) {
    uv_err_new(handle, handle->delayed_error);
    return -1;
  }

  int r = listen(handle->fd, backlog);
  if (r < 0) {
    uv_err_new(handle, errno);
    return -1;
  }

  handle->accept_cb = cb;

  /* Start listening for connections. */
  ev_io_set(&handle->read_watcher, handle->fd, EV_READ);
  ev_set_cb(&handle->read_watcher, uv__server_io);
  ev_io_start(EV_DEFAULT_ &handle->read_watcher);

  return 0;
}


void uv__finish_close(uv_handle_t* handle) {
  assert(uv_flag_is_set(handle, UV_CLOSING));
  assert(!uv_flag_is_set(handle, UV_CLOSED));
  uv_flag_set(handle, UV_CLOSED);

  switch (handle->type) {
    case UV_TCP:
      /* XXX Is it necessary to stop these watchers here? weren't they
       * supposed to be stopped in uv_close()?
       */
      ev_io_stop(EV_DEFAULT_ &handle->write_watcher);
      ev_io_stop(EV_DEFAULT_ &handle->read_watcher);

      assert(!ev_is_active(&handle->read_watcher));
      assert(!ev_is_active(&handle->write_watcher));

      close(handle->fd);
      handle->fd = -1;

      if (handle->accepted_fd >= 0) {
        close(handle->accepted_fd);
        handle->accepted_fd = -1;
      }
      break;

    case UV_PREPARE:
      assert(!ev_is_active(&handle->prepare_watcher));
      break;

    case UV_CHECK:
      assert(!ev_is_active(&handle->check_watcher));
      break;

    case UV_IDLE:
      assert(!ev_is_active(&handle->idle_watcher));
      break;

    case UV_ASYNC:
      assert(!ev_is_active(&handle->async_watcher));
      break;

    case UV_TIMER:
      assert(!ev_is_active(&handle->timer_watcher));
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


uv_req_t* uv_write_queue_head(uv_handle_t* handle) {
  if (ngx_queue_empty(&handle->write_queue)) {
    return NULL;
  }

  ngx_queue_t* q = ngx_queue_head(&handle->write_queue);
  if (!q) {
    return NULL;
  }

  uv_req_t* req = ngx_queue_data(q, struct uv_req_s, queue);
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


static void uv__drain(uv_handle_t* handle) {
  assert(!uv_write_queue_head(handle));
  assert(handle->write_queue_size == 0);

  ev_io_stop(EV_DEFAULT_ &handle->write_watcher);

  /* Shutdown? */
  if (uv_flag_is_set(handle, UV_SHUTTING) &&
      !uv_flag_is_set(handle, UV_CLOSING) &&
      !uv_flag_is_set(handle, UV_SHUT)) {
    assert(handle->shutdown_req);

    uv_req_t* req = handle->shutdown_req;
    uv_shutdown_cb cb = req->cb;

    if (shutdown(handle->fd, SHUT_WR)) {
      /* Error. Nothing we can do, close the handle. */
      uv_err_new(handle, errno);
      uv_close(handle);
      if (cb) cb(req, -1);
    } else {
      uv_err_new(handle, 0);
      uv_flag_set(handle, UV_SHUT);
      if (cb) cb(req, 0);
    }
  }
}


void uv__write(uv_handle_t* handle) {
  assert(handle->fd >= 0);

  /* TODO: should probably while(1) here until EAGAIN */

  /* Get the request at the head of the queue. */
  uv_req_t* req = uv_write_queue_head(handle);
  if (!req) {
    assert(handle->write_queue_size == 0);
    uv__drain(handle);
    return;
  }

  assert(req->handle == handle);

  /* Cast to iovec. We had to have our own uv_buf_t instead of iovec
   * because Windows's WSABUF is not an iovec.
   */
  assert(sizeof(uv_buf_t) == sizeof(struct iovec));
  struct iovec* iov = (struct iovec*) &(req->bufs[req->write_index]);
  int iovcnt = req->bufcnt - req->write_index;

  /* Now do the actual writev. Note that we've been updating the pointers
   * inside the iov each time we write. So there is no need to offset it.
   */

  ssize_t n = writev(handle->fd, iov, iovcnt);

  uv_write_cb cb = req->cb;

  if (n < 0) {
    if (errno != EAGAIN) {
      uv_err_t err = uv_err_new(handle, errno);

      /* XXX How do we handle the error? Need test coverage here. */
      uv_close(handle);

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
        handle->write_queue_size -= n;
        n = 0;

        /* There is more to write. Break and ensure the watcher is pending. */
        break;

      } else {
        /* Finished writing the buf at index req->write_index. */
        req->write_index++;

        assert(n >= len);
        n -= len;

        assert(handle->write_queue_size >= len);
        handle->write_queue_size -= len;

        if (req->write_index == req->bufcnt) {
          /* Then we're done! */
          assert(n == 0);

          /* Pop the req off handle->write_queue. */
          ngx_queue_remove(&req->queue);
          free(req->bufs); /* FIXME: we should not be allocing for each read */
          req->bufs = NULL;

          /* NOTE: call callback AFTER freeing the request data. */
          if (cb) {
            cb(req, 0);
          }

          if (!ngx_queue_empty(&handle->write_queue)) {
            assert(handle->write_queue_size > 0);
          } else {
            /* Write queue drained. */
            uv__drain(handle);
          }

          return;
        }
      }
    }
  }

  /* Either we've counted n down to zero or we've got EAGAIN. */
  assert(n == 0 || n == -1);

  /* We're not done yet. */
  assert(ev_is_active(&handle->write_watcher));
  ev_io_start(EV_DEFAULT_ &handle->write_watcher);
}


void uv__read(uv_handle_t* handle) {
  /* XXX: Maybe instead of having UV_READING we just test if
   * handle->read_cb is NULL or not?
   */
  while (handle->read_cb && uv_flag_is_set(handle, UV_READING)) {
    assert(alloc_cb);
    uv_buf_t buf = alloc_cb(handle, 64 * 1024);

    assert(buf.len > 0);
    assert(buf.base);

    struct iovec* iov = (struct iovec*) &buf;

    ssize_t nread = readv(handle->fd, iov, 1);

    if (nread < 0) {
      /* Error */
      if (errno == EAGAIN) {
        /* Wait for the next one. */
        if (uv_flag_is_set(handle, UV_READING)) {
          ev_io_start(EV_DEFAULT_UC_ &handle->read_watcher);
        }
        uv_err_new(handle, EAGAIN);
        handle->read_cb(handle, 0, buf);
        return;
      } else {
        uv_err_new(handle, errno);
        uv_close(handle);
        handle->read_cb(handle, -1, buf);
        assert(!ev_is_active(&handle->read_watcher));
        return;
      }
    } else if (nread == 0) {
      /* EOF */
      uv_err_new_artificial(handle, UV_EOF);
      ev_io_stop(EV_DEFAULT_UC_ &handle->read_watcher);
      handle->read_cb(handle, -1, buf);

      if (uv_flag_is_set(handle, UV_SHUT)) {
        uv_close(handle);
      }
      return;
    } else {
      /* Successful read */
      handle->read_cb(handle, nread, buf);
    }
  }
}


int uv_shutdown(uv_req_t* req) {
  uv_handle_t* handle = req->handle;
  assert(handle->fd >= 0);

  if (uv_flag_is_set(handle, UV_SHUT) ||
      uv_flag_is_set(handle, UV_CLOSED) ||
      uv_flag_is_set(handle, UV_CLOSING)) {
    return -1;
  }

  handle->shutdown_req = req;
  req->type = UV_SHUTDOWN;

  uv_flag_set(handle, UV_SHUTTING);

  ev_io_start(EV_DEFAULT_UC_ &handle->write_watcher);

  return 0;
}


void uv__tcp_io(EV_P_ ev_io* watcher, int revents) {
  uv_handle_t* handle = watcher->data;
  assert(watcher == &handle->read_watcher ||
         watcher == &handle->write_watcher);

  assert(handle->fd >= 0);

  assert(!uv_flag_is_set(handle, UV_CLOSING));

  if (handle->connect_req) {
    uv__tcp_connect(handle);
  } else {
    if (revents & EV_READ) {
      uv__read(handle);
    }

    if (revents & EV_WRITE) {
      uv__write(handle);
    }
  }
}


/**
 * We get called here from directly following a call to connect(2).
 * In order to determine if we've errored out or succeeded must call
 * getsockopt.
 */
static void uv__tcp_connect(uv_handle_t* handle) {
  int error;
  socklen_t errorsize = sizeof(int);

  assert(handle->fd >= 0);

  uv_req_t* req = handle->connect_req;
  assert(req);

  if (handle->delayed_error) {
    /* To smooth over the differences between unixes errors that
     * were reported synchronously on the first connect can be delayed
     * until the next tick--which is now.
     */
    error = handle->delayed_error;
    handle->delayed_error = 0;
  } else {
    /* Normal situation: we need to get the socket error from the kernel. */
    getsockopt(handle->fd, SOL_SOCKET, SO_ERROR, &error, &errorsize);
  }

  if (!error) {
    ev_io_start(EV_DEFAULT_ &handle->read_watcher);

    /* Successful connection */
    handle->connect_req = NULL;
    uv_connect_cb connect_cb = req->cb;
    if (connect_cb) {
      connect_cb(req, 0);
    }

  } else if (error == EINPROGRESS) {
    /* Still connecting. */
    return;
  } else {
    /* Error */
    uv_err_t err = uv_err_new(handle, error);

    handle->connect_req = NULL;

    uv_connect_cb connect_cb = req->cb;
    if (connect_cb) {
      connect_cb(req, -1);
    }

    uv_close(handle);
  }
}


int uv_connect(uv_req_t* req, struct sockaddr* addr) {
  uv_handle_t* handle = req->handle;

  if (handle->fd <= 0) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
      uv_err_new(handle, errno);
      return -1;
    }

    if (uv_tcp_open(handle, fd)) {
      close(fd);
      return -2;
    }
  }

  req->type = UV_CONNECT;
  ngx_queue_init(&req->queue);

  if (handle->connect_req) {
    uv_err_new(handle, EALREADY);
    return -1;
  }

  if (handle->type != UV_TCP) {
    uv_err_new(handle, ENOTSOCK);
    return -1;
  }

  handle->connect_req = req;

  int addrsize = sizeof(struct sockaddr_in);

  int r = connect(handle->fd, addr, addrsize);
  handle->delayed_error = 0;

  if (r != 0 && errno != EINPROGRESS) {
    switch (errno) {
      /* If we get a ECONNREFUSED wait until the next tick to report the
       * error. Solaris wants to report immediately--other unixes want to
       * wait.
       */
      case ECONNREFUSED:
        handle->delayed_error = errno;
        break;

      default:
        uv_err_new(handle, errno);
        return -1;
    }
  }

  assert(handle->write_watcher.data == handle);
  ev_io_start(EV_DEFAULT_ &handle->write_watcher);

  if (handle->delayed_error) {
    ev_feed_event(EV_DEFAULT_ &handle->write_watcher, EV_WRITE);
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
  uv_handle_t* handle = req->handle;
  assert(handle->fd >= 0);

  ngx_queue_init(&req->queue);
  req->type = UV_WRITE;

  /* TODO: Don't malloc for each write... */
  req->bufs = malloc(sizeof(uv_buf_t) * bufcnt);
  memcpy(req->bufs, bufs, bufcnt * sizeof(uv_buf_t));
  req->bufcnt = bufcnt;

  req->write_index = 0;
  handle->write_queue_size += uv__buf_count(bufs, bufcnt);

  /* Append the request to write_queue. */
  ngx_queue_insert_tail(&handle->write_queue, &req->queue);

  assert(!ngx_queue_empty(&handle->write_queue));
  assert(handle->write_watcher.cb == uv__tcp_io);
  assert(handle->write_watcher.data == handle);
  assert(handle->write_watcher.fd == handle->fd);

  ev_io_start(EV_DEFAULT_ &handle->write_watcher);

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


int uv_read_start(uv_handle_t* handle, uv_read_cb cb) {
  /* The UV_READING flag is irrelevant of the state of the handle - it just
   * expresses the desired state of the user.
   */
  uv_flag_set(handle, UV_READING);

  /* TODO: try to do the read inline? */
  /* TODO: keep track of handle state. If we've gotten a EOF then we should
   * not start the IO watcher.
   */
  assert(handle->fd >= 0);
  handle->read_cb = cb;

  /* These should have been set by uv_tcp_init. */
  assert(handle->read_watcher.data == handle);
  assert(handle->read_watcher.cb == uv__tcp_io);

  ev_io_start(EV_DEFAULT_UC_ &handle->read_watcher);
  return 0;
}


int uv_read_stop(uv_handle_t* handle) {
  uv_flag_unset(handle, UV_READING);

  ev_io_stop(EV_DEFAULT_UC_ &handle->read_watcher);
  handle->read_cb = NULL;
  return 0;
}


void uv_free(uv_handle_t* handle) {
  free(handle);
  /* lists? */
  return;
}


void uv_req_init(uv_req_t* req, uv_handle_t* handle, void* cb) {
  req->type = UV_UNKNOWN_REQ;
  req->cb = cb;
  req->handle = handle;
  ngx_queue_init(&req->queue);
}


static void uv__prepare(EV_P_ ev_prepare* w, int revents) {
  uv_handle_t* handle = (uv_handle_t*)(w->data);

  if (handle->prepare_cb) handle->prepare_cb(handle, 0);
}


int uv_prepare_init(uv_handle_t* handle, uv_close_cb close_cb, void* data) {
  uv__handle_init(handle, UV_PREPARE, close_cb, data);

  ev_prepare_init(&handle->prepare_watcher, uv__prepare);
  handle->prepare_watcher.data = handle;

  handle->prepare_cb = NULL;

  return 0;
}


int uv_prepare_start(uv_handle_t* handle, uv_loop_cb cb) {
  int was_active = ev_is_active(&handle->prepare_watcher);

  handle->prepare_cb = cb;

  ev_prepare_start(EV_DEFAULT_UC_ &handle->prepare_watcher);

  if (!was_active) {
    ev_unref(EV_DEFAULT_UC);
  }

  return 0;
}


int uv_prepare_stop(uv_handle_t* handle) {
  int was_active = ev_is_active(&handle->prepare_watcher);

  ev_prepare_stop(EV_DEFAULT_UC_ &handle->prepare_watcher);

  if (was_active) {
    ev_ref(EV_DEFAULT_UC);
  }
  return 0;
}



static void uv__check(EV_P_ ev_check* w, int revents) {
  uv_handle_t* handle = (uv_handle_t*)(w->data);

  if (handle->check_cb) handle->check_cb(handle, 0);
}


int uv_check_init(uv_handle_t* handle, uv_close_cb close_cb, void* data) {
  uv__handle_init(handle, UV_CHECK, close_cb, data);

  ev_check_init(&handle->check_watcher, uv__check);
  handle->check_watcher.data = handle;

  handle->check_cb = NULL;

  return 0;
}


int uv_check_start(uv_handle_t* handle, uv_loop_cb cb) {
  int was_active = ev_is_active(&handle->prepare_watcher);

  handle->check_cb = cb;

  ev_check_start(EV_DEFAULT_UC_ &handle->check_watcher);

  if (!was_active) {
    ev_unref(EV_DEFAULT_UC);
  }

  return 0;
}


int uv_check_stop(uv_handle_t* handle) {
  int was_active = ev_is_active(&handle->check_watcher);

  ev_check_stop(EV_DEFAULT_UC_ &handle->check_watcher);

  if (was_active) {
    ev_ref(EV_DEFAULT_UC);
  }

  return 0;
}


static void uv__idle(EV_P_ ev_idle* w, int revents) {
  uv_handle_t* handle = (uv_handle_t*)(w->data);

  if (handle->idle_cb) handle->idle_cb(handle, 0);
}



int uv_idle_init(uv_handle_t* handle, uv_close_cb close_cb, void* data) {
  uv__handle_init(handle, UV_IDLE, close_cb, data);

  ev_idle_init(&handle->idle_watcher, uv__idle);
  handle->idle_watcher.data = handle;

  handle->idle_cb = NULL;

  return 0;
}


int uv_idle_start(uv_handle_t* handle, uv_loop_cb cb) {
  int was_active = ev_is_active(&handle->idle_watcher);

  handle->idle_cb = cb;
  ev_idle_start(EV_DEFAULT_UC_ &handle->idle_watcher);

  if (!was_active) {
    ev_unref(EV_DEFAULT_UC);
  }

  return 0;
}


int uv_idle_stop(uv_handle_t* handle) {
  int was_active = ev_is_active(&handle->idle_watcher);

  ev_idle_stop(EV_DEFAULT_UC_ &handle->idle_watcher);

  if (was_active) {
    ev_ref(EV_DEFAULT_UC);
  }

  return 0;
}


int uv_is_active(uv_handle_t* handle) {
  switch (handle->type) {
    case UV_PREPARE:
    case UV_CHECK:
    case UV_IDLE:
      return ev_is_active(handle);

    default:
      return 1;
  }
}


static void uv__async(EV_P_ ev_async* w, int revents) {
  uv_handle_t* handle = (uv_handle_t*)(w->data);

  if (handle->async_cb) handle->async_cb(handle, 0);
}


int uv_async_init(uv_handle_t* handle, uv_async_cb async_cb,
    uv_close_cb close_cb, void* data) {
  uv__handle_init(handle, UV_ASYNC, close_cb, data);

  ev_async_init(&handle->async_watcher, uv__async);
  handle->async_watcher.data = handle;

  handle->async_cb = async_cb;

  /* Note: This does not have symmetry with the other libev wrappers. */
  ev_async_start(EV_DEFAULT_UC_ &handle->async_watcher);
  ev_unref(EV_DEFAULT_UC);

  return 0;
}


int uv_async_send(uv_handle_t* handle) {
  ev_async_send(EV_DEFAULT_UC_ &handle->async_watcher);
}


static void uv__timer_cb(EV_P_ ev_timer* w, int revents) {
  uv_handle_t* handle = (uv_handle_t*)(w->data);

  if (!ev_is_active(w)) {
    ev_ref(EV_DEFAULT_UC);
  }

  if (handle->timer_cb) handle->timer_cb(handle, 0);
}


int uv_timer_init(uv_handle_t* handle, uv_close_cb close_cb, void* data) {
  uv__handle_init(handle, UV_TIMER, close_cb, data);

  ev_init(&handle->timer_watcher, uv__timer_cb);
  handle->timer_watcher.data = handle;

  return 0;
}


int uv_timer_start(uv_handle_t* handle, uv_loop_cb cb, int64_t timeout,
    int64_t repeat) {
  if (ev_is_active(&handle->timer_watcher)) {
    return -1;
  }

  handle->timer_cb = cb;
  ev_timer_set(&handle->timer_watcher, timeout / 1000.0, repeat / 1000.0);
  ev_timer_start(EV_DEFAULT_UC_ &handle->timer_watcher);
  ev_unref(EV_DEFAULT_UC);
  return 0;
}


int uv_timer_stop(uv_handle_t* handle) {
  if (ev_is_active(&handle->timer_watcher)) {
    ev_ref(EV_DEFAULT_UC);
  }

  ev_timer_stop(EV_DEFAULT_UC_ &handle->timer_watcher);
  return 0;
}


int uv_timer_again(uv_handle_t* handle) {
  if (!ev_is_active(&handle->timer_watcher)) {
    uv_err_new(handle, EINVAL);
    return -1;
  }

  ev_timer_again(EV_DEFAULT_UC_ &handle->timer_watcher);
  return 0;
}

void uv_timer_set_repeat(uv_handle_t* handle, int64_t repeat) {
  assert(handle->type == UV_TIMER);
  handle->timer_watcher.repeat = repeat / 1000.0;
}

int64_t uv_timer_get_repeat(uv_handle_t* handle) {
  assert(handle->type == UV_TIMER);
  return (int64_t)(1000 * handle->timer_watcher.repeat);
}

