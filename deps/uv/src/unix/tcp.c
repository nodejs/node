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

#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>


int uv_tcp_init(uv_loop_t* loop, uv_tcp_t* tcp) {
  uv__stream_init(loop, (uv_stream_t*)tcp, UV_TCP);
  return 0;
}


static int maybe_new_socket(uv_tcp_t* handle, int domain, int flags) {
  int sockfd;

  if (uv__stream_fd(handle) != -1)
    return 0;

  sockfd = uv__socket(domain, SOCK_STREAM, 0);

  if (sockfd == -1)
    return uv__set_sys_error(handle->loop, errno);

  if (uv__stream_open((uv_stream_t*)handle, sockfd, flags)) {
    close(sockfd);
    return -1;
  }

  return 0;
}


static int uv__bind(uv_tcp_t* tcp,
                    int domain,
                    struct sockaddr* addr,
                    int addrsize) {
  int on;

  if (maybe_new_socket(tcp, domain, UV_STREAM_READABLE|UV_STREAM_WRITABLE))
    return -1;

  on = 1;
  if (setsockopt(tcp->io_watcher.fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)))
    return uv__set_sys_error(tcp->loop, errno);

  errno = 0;
  if (bind(tcp->io_watcher.fd, addr, addrsize) && errno != EADDRINUSE)
    return uv__set_sys_error(tcp->loop, errno);

  tcp->delayed_error = errno;
  return 0;
}


static int uv__connect(uv_connect_t* req,
                       uv_tcp_t* handle,
                       struct sockaddr* addr,
                       socklen_t addrlen,
                       uv_connect_cb cb) {
  int r;

  assert(handle->type == UV_TCP);

  if (handle->connect_req)
    return uv__set_sys_error(handle->loop, EALREADY);

  if (maybe_new_socket(handle,
                       addr->sa_family,
                       UV_STREAM_READABLE|UV_STREAM_WRITABLE)) {
    return -1;
  }

  handle->delayed_error = 0;

  do
    r = connect(uv__stream_fd(handle), addr, addrlen);
  while (r == -1 && errno == EINTR);

  if (r == -1) {
    if (errno == EINPROGRESS)
      ; /* not an error */
    else if (errno == ECONNREFUSED)
    /* If we get a ECONNREFUSED wait until the next tick to report the
     * error. Solaris wants to report immediately--other unixes want to
     * wait.
     */
      handle->delayed_error = errno;
    else
      return uv__set_sys_error(handle->loop, errno);
  }

  uv__req_init(handle->loop, req, UV_CONNECT);
  req->cb = cb;
  req->handle = (uv_stream_t*) handle;
  ngx_queue_init(&req->queue);
  handle->connect_req = req;

  uv__io_start(handle->loop, &handle->io_watcher, UV__POLLOUT);

  if (handle->delayed_error)
    uv__io_feed(handle->loop, &handle->io_watcher);

  return 0;
}


int uv__tcp_bind(uv_tcp_t* handle, struct sockaddr_in addr) {
  return uv__bind(handle,
                  AF_INET,
                  (struct sockaddr*)&addr,
                  sizeof(struct sockaddr_in));
}


int uv__tcp_bind6(uv_tcp_t* handle, struct sockaddr_in6 addr) {
  return uv__bind(handle,
                  AF_INET6,
                  (struct sockaddr*)&addr,
                  sizeof(struct sockaddr_in6));
}


int uv_tcp_open(uv_tcp_t* handle, uv_os_sock_t sock) {
  return uv__stream_open((uv_stream_t*)handle,
                         sock,
                         UV_STREAM_READABLE | UV_STREAM_WRITABLE);
}


int uv_tcp_getsockname(uv_tcp_t* handle, struct sockaddr* name,
    int* namelen) {
  socklen_t socklen;
  int saved_errno;
  int rv = 0;

  /* Don't clobber errno. */
  saved_errno = errno;

  if (handle->delayed_error) {
    uv__set_sys_error(handle->loop, handle->delayed_error);
    rv = -1;
    goto out;
  }

  if (uv__stream_fd(handle) < 0) {
    uv__set_sys_error(handle->loop, EINVAL);
    rv = -1;
    goto out;
  }

  /* sizeof(socklen_t) != sizeof(int) on some systems. */
  socklen = (socklen_t)*namelen;

  if (getsockname(uv__stream_fd(handle), name, &socklen) == -1) {
    uv__set_sys_error(handle->loop, errno);
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
    uv__set_sys_error(handle->loop, handle->delayed_error);
    rv = -1;
    goto out;
  }

  if (uv__stream_fd(handle) < 0) {
    uv__set_sys_error(handle->loop, EINVAL);
    rv = -1;
    goto out;
  }

  /* sizeof(socklen_t) != sizeof(int) on some systems. */
  socklen = (socklen_t)*namelen;

  if (getpeername(uv__stream_fd(handle), name, &socklen) == -1) {
    uv__set_sys_error(handle->loop, errno);
    rv = -1;
  } else {
    *namelen = (int)socklen;
  }

out:
  errno = saved_errno;
  return rv;
}


int uv_tcp_listen(uv_tcp_t* tcp, int backlog, uv_connection_cb cb) {
  static int single_accept = -1;

  if (tcp->delayed_error)
    return uv__set_sys_error(tcp->loop, tcp->delayed_error);

  if (single_accept == -1) {
    const char* val = getenv("UV_TCP_SINGLE_ACCEPT");
    single_accept = (val != NULL && atoi(val) != 0);  /* Off by default. */
  }

  if (single_accept)
    tcp->flags |= UV_TCP_SINGLE_ACCEPT;

  if (maybe_new_socket(tcp, AF_INET, UV_STREAM_READABLE))
    return -1;

  if (listen(tcp->io_watcher.fd, backlog))
    return uv__set_sys_error(tcp->loop, errno);

  tcp->connection_cb = cb;

  /* Start listening for connections. */
  tcp->io_watcher.cb = uv__server_io;
  uv__io_start(tcp->loop, &tcp->io_watcher, UV__POLLIN);

  return 0;
}


int uv__tcp_connect(uv_connect_t* req,
                    uv_tcp_t* handle,
                    struct sockaddr_in addr,
                    uv_connect_cb cb) {
  int saved_errno;
  int status;

  saved_errno = errno;
  status = uv__connect(req, handle, (struct sockaddr*)&addr, sizeof addr, cb);
  errno = saved_errno;

  return status;
}


int uv__tcp_connect6(uv_connect_t* req,
                     uv_tcp_t* handle,
                     struct sockaddr_in6 addr,
                     uv_connect_cb cb) {
  int saved_errno;
  int status;

  saved_errno = errno;
  status = uv__connect(req, handle, (struct sockaddr*)&addr, sizeof addr, cb);
  errno = saved_errno;

  return status;
}


int uv__tcp_nodelay(int fd, int on) {
  return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
}


int uv__tcp_keepalive(int fd, int on, unsigned int delay) {
  if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)))
    return -1;

#ifdef TCP_KEEPIDLE
  if (on && setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &delay, sizeof(delay)))
    return -1;
#endif

  /* Solaris/SmartOS, if you don't support keep-alive,
   * then don't advertise it in your system headers...
   */
#if defined(TCP_KEEPALIVE) && !defined(__sun)
  if (on && setsockopt(fd, IPPROTO_TCP, TCP_KEEPALIVE, &delay, sizeof(delay)))
    return -1;
#endif

  return 0;
}


int uv_tcp_nodelay(uv_tcp_t* handle, int on) {
  if (uv__stream_fd(handle) != -1)
    if (uv__tcp_nodelay(uv__stream_fd(handle), on))
      return -1;

  if (on)
    handle->flags |= UV_TCP_NODELAY;
  else
    handle->flags &= ~UV_TCP_NODELAY;

  return 0;
}


int uv_tcp_keepalive(uv_tcp_t* handle, int on, unsigned int delay) {
  if (uv__stream_fd(handle) != -1)
    if (uv__tcp_keepalive(uv__stream_fd(handle), on, delay))
      return -1;

  if (on)
    handle->flags |= UV_TCP_KEEPALIVE;
  else
    handle->flags &= ~UV_TCP_KEEPALIVE;

  /* TODO Store delay if uv__stream_fd(handle) == -1 but don't want to enlarge
   *      uv_tcp_t with an int that's almost never used...
   */

  return 0;
}


int uv_tcp_simultaneous_accepts(uv_tcp_t* handle, int enable) {
  if (enable)
    handle->flags &= ~UV_TCP_SINGLE_ACCEPT;
  else
    handle->flags |= UV_TCP_SINGLE_ACCEPT;
  return 0;
}


void uv__tcp_close(uv_tcp_t* handle) {
  uv__stream_close((uv_stream_t*)handle);
}
