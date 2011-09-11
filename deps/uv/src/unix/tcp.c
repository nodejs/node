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

#include "uv.h"
#include "internal.h"

#include <assert.h>
#include <errno.h>


int uv_tcp_init(uv_loop_t* loop, uv_tcp_t* tcp) {
  uv__stream_init(loop, (uv_stream_t*)tcp, UV_TCP);
  loop->counters.tcp_init++;
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
      uv_err_new(tcp->loop, errno);
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
      uv_err_new(tcp->loop, errno);
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
    uv_err_new(tcp->loop, EFAULT);
    return -1;
  }

  return uv__tcp_bind(tcp,
                      AF_INET,
                      (struct sockaddr*)&addr,
                      sizeof(struct sockaddr_in));
}


int uv_tcp_bind6(uv_tcp_t* tcp, struct sockaddr_in6 addr) {
  if (addr.sin6_family != AF_INET6) {
    uv_err_new(tcp->loop, EFAULT);
    return -1;
  }

  return uv__tcp_bind(tcp,
                      AF_INET6,
                      (struct sockaddr*)&addr,
                      sizeof(struct sockaddr_in6));
}


int uv_tcp_getsockname(uv_tcp_t* handle, struct sockaddr* name,
    int* namelen) {
  socklen_t socklen;
  int saved_errno;
  int rv = 0;

  /* Don't clobber errno. */
  saved_errno = errno;

  if (handle->delayed_error) {
    uv_err_new(handle->loop, handle->delayed_error);
    rv = -1;
    goto out;
  }

  if (handle->fd < 0) {
    uv_err_new(handle->loop, EINVAL);
    rv = -1;
    goto out;
  }

  /* sizeof(socklen_t) != sizeof(int) on some systems. */
  socklen = (socklen_t)*namelen;

  if (getsockname(handle->fd, name, &socklen) == -1) {
    uv_err_new(handle->loop, errno);
    rv = -1;
  } else {
    *namelen = (int)socklen;
  }

out:
  errno = saved_errno;
  return rv;
}


int uv_tcp_getpeername(uv_tcp_t* handle, struct sockaddr* name,
    int* namelen) {
  socklen_t socklen;
  int saved_errno;
  int rv = 0;

  /* Don't clobber errno. */
  saved_errno = errno;

  if (handle->delayed_error) {
    uv_err_new(handle->loop, handle->delayed_error);
    rv = -1;
    goto out;
  }

  if (handle->fd < 0) {
    uv_err_new(handle->loop, EINVAL);
    rv = -1;
    goto out;
  }

  /* sizeof(socklen_t) != sizeof(int) on some systems. */
  socklen = (socklen_t)*namelen;

  if (getpeername(handle->fd, name, &socklen) == -1) {
    uv_err_new(handle->loop, errno);
    rv = -1;
  } else {
    *namelen = (int)socklen;
  }

out:
  errno = saved_errno;
  return rv;
}


int uv_tcp_listen(uv_tcp_t* tcp, int backlog, uv_connection_cb cb) {
  int r;

  if (tcp->delayed_error) {
    uv_err_new(tcp->loop, tcp->delayed_error);
    return -1;
  }

  if (tcp->fd < 0) {
    if ((tcp->fd = uv__socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      uv_err_new(tcp->loop, errno);
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
    uv_err_new(tcp->loop, errno);
    return -1;
  }

  tcp->connection_cb = cb;

  /* Start listening for connections. */
  ev_io_set(&tcp->read_watcher, tcp->fd, EV_READ);
  ev_set_cb(&tcp->read_watcher, uv__server_io);
  ev_io_start(tcp->loop->ev, &tcp->read_watcher);

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
    uv_err_new(handle->loop, EINVAL);
    goto out;
  }

  if (address.sin_family != AF_INET) {
    uv_err_new(handle->loop, EINVAL);
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
    uv_err_new(handle->loop, EINVAL);
    goto out;
  }

  if (address.sin6_family != AF_INET6) {
    uv_err_new(handle->loop, EINVAL);
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
