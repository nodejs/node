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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>

#include <stdio.h>


static void uv__stream_connect(uv_stream_t*);
static void uv__write(uv_stream_t* stream);
static void uv__read(uv_stream_t* stream);


static size_t uv__buf_count(uv_buf_t bufs[], int bufcnt) {
  size_t total = 0;
  int i;

  for (i = 0; i < bufcnt; i++) {
    total += bufs[i].len;
  }

  return total;
}


void uv__stream_init(uv_loop_t* loop,
                     uv_stream_t* stream,
                     uv_handle_type type) {
  uv__handle_init(loop, (uv_handle_t*)stream, type);
  loop->counters.stream_init++;

  stream->alloc_cb = NULL;
  stream->close_cb = NULL;
  stream->connection_cb = NULL;
  stream->connect_req = NULL;
  stream->accepted_fd = -1;
  stream->fd = -1;
  stream->delayed_error = 0;
  stream->blocking = 0;
  ngx_queue_init(&stream->write_queue);
  ngx_queue_init(&stream->write_completed_queue);
  stream->write_queue_size = 0;

  ev_init(&stream->read_watcher, uv__stream_io);
  stream->read_watcher.data = stream;

  ev_init(&stream->write_watcher, uv__stream_io);
  stream->write_watcher.data = stream;

  assert(ngx_queue_empty(&stream->write_queue));
  assert(ngx_queue_empty(&stream->write_completed_queue));
  assert(stream->write_queue_size == 0);
}


int uv__stream_open(uv_stream_t* stream, int fd, int flags) {
  socklen_t yes;

  assert(fd >= 0);
  stream->fd = fd;

  stream->flags |= flags;

  if (stream->type == UV_TCP) {
    /* Reuse the port address if applicable. */
    yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes) == -1) {
      uv__set_sys_error(stream->loop, errno);
      return -1;
    }

    if ((stream->flags & UV_TCP_NODELAY) &&
        uv__tcp_nodelay((uv_tcp_t*)stream, 1)) {
      return -1;
    }

    /* TODO Use delay the user passed in. */
    if ((stream->flags & UV_TCP_KEEPALIVE) &&
        uv__tcp_keepalive((uv_tcp_t*)stream, 1, 60)) {
      return -1;
    }
  }

  /* Associate the fd with each ev_io watcher. */
  ev_io_set(&stream->read_watcher, fd, EV_READ);
  ev_io_set(&stream->write_watcher, fd, EV_WRITE);

  /* These should have been set up by uv_tcp_init or uv_pipe_init. */
  assert(stream->read_watcher.cb == uv__stream_io);
  assert(stream->write_watcher.cb == uv__stream_io);

  return 0;
}


void uv__stream_destroy(uv_stream_t* stream) {
  uv_write_t* req;
  ngx_queue_t* q;

  assert(stream->flags & UV_CLOSED);

  while (!ngx_queue_empty(&stream->write_queue)) {
    q = ngx_queue_head(&stream->write_queue);
    ngx_queue_remove(q);

    req = ngx_queue_data(q, uv_write_t, queue);
    if (req->bufs != req->bufsml)
      free(req->bufs);

    if (req->cb) {
      uv__set_artificial_error(req->handle->loop, UV_EINTR);
      req->cb(req, -1);
    }
  }

  while (!ngx_queue_empty(&stream->write_completed_queue)) {
    q = ngx_queue_head(&stream->write_completed_queue);
    ngx_queue_remove(q);

    req = ngx_queue_data(q, uv_write_t, queue);
    if (req->cb) {
      uv__set_sys_error(stream->loop, req->error);
      req->cb(req, req->error ? -1 : 0);
    }
  }
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
    ev_io_stop(EV_A, &stream->read_watcher);
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
      } else if (errno == ECONNABORTED) {
        /* ignore */
        continue;
      } else {
        uv__set_sys_error(stream->loop, errno);
        stream->connection_cb((uv_stream_t*)stream, -1);
      }
    } else {
      stream->accepted_fd = fd;
      stream->connection_cb((uv_stream_t*)stream, 0);
      if (stream->accepted_fd >= 0) {
        /* The user hasn't yet accepted called uv_accept() */
        ev_io_stop(stream->loop->ev, &stream->read_watcher);
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

  /* TODO document this */
  assert(server->loop == client->loop);

  saved_errno = errno;
  status = -1;

  streamServer = (uv_stream_t*)server;
  streamClient = (uv_stream_t*)client;

  if (streamServer->accepted_fd < 0) {
    uv__set_sys_error(server->loop, EAGAIN);
    goto out;
  }

  if (uv__stream_open(streamClient, streamServer->accepted_fd,
        UV_READABLE | UV_WRITABLE)) {
    /* TODO handle error */
    uv__close(streamServer->accepted_fd);
    streamServer->accepted_fd = -1;
    goto out;
  }

  ev_io_start(streamServer->loop->ev, &streamServer->read_watcher);
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


static void uv__drain(uv_stream_t* stream) {
  uv_shutdown_t* req;

  assert(!uv_write_queue_head(stream));
  assert(stream->write_queue_size == 0);

  ev_io_stop(stream->loop->ev, &stream->write_watcher);

  /* Shutdown? */
  if ((stream->flags & UV_SHUTTING) &&
      !(stream->flags & UV_CLOSING) &&
      !(stream->flags & UV_SHUT)) {
    assert(stream->shutdown_req);

    req = stream->shutdown_req;

    if (shutdown(stream->fd, SHUT_WR)) {
      /* Error. Report it. User should call uv_close(). */
      uv__set_sys_error(stream->loop, errno);
      if (req->cb) {
        req->cb(req, -1);
      }
    } else {
      uv__set_sys_error(stream->loop, 0);
      ((uv_handle_t*) stream)->flags |= UV_SHUT;
      if (req->cb) {
        req->cb(req, 0);
      }
    }
  }
}


static size_t uv__write_req_size(uv_write_t* req) {
  size_t size;

  size = uv__buf_count(req->bufs + req->write_index,
                       req->bufcnt - req->write_index);
  assert(req->handle->write_queue_size >= size);

  return size;
}


static void uv__write_req_finish(uv_write_t* req) {
  uv_stream_t* stream = req->handle;

  /* Pop the req off tcp->write_queue. */
  ngx_queue_remove(&req->queue);
  if (req->bufs != req->bufsml) {
    free(req->bufs);
  }
  req->bufs = NULL;

  /* Add it to the write_completed_queue where it will have its
   * callback called in the near future.
   */
  ngx_queue_insert_tail(&stream->write_completed_queue, &req->queue);
  ev_feed_event(stream->loop->ev, &stream->write_watcher, EV_WRITE);
}


/* On success returns NULL. On error returns a pointer to the write request
 * which had the error.
 */
static void uv__write(uv_stream_t* stream) {
  uv_write_t* req;
  struct iovec* iov;
  int iovcnt;
  ssize_t n;

  if (stream->flags & UV_CLOSING) {
    /* Handle was closed this tick. We've received a stale
     * 'is writable' callback from the event loop, ignore.
     */
    return;
  }

start:

  assert(stream->fd >= 0);

  /* Get the request at the head of the queue. */
  req = uv_write_queue_head(stream);
  if (!req) {
    assert(stream->write_queue_size == 0);
    return;
  }

  assert(req->handle == stream);

  /*
   * Cast to iovec. We had to have our own uv_buf_t instead of iovec
   * because Windows's WSABUF is not an iovec.
   */
  assert(sizeof(uv_buf_t) == sizeof(struct iovec));
  iov = (struct iovec*) &(req->bufs[req->write_index]);
  iovcnt = req->bufcnt - req->write_index;

  /*
   * Now do the actual writev. Note that we've been updating the pointers
   * inside the iov each time we write. So there is no need to offset it.
   */

  if (req->send_handle) {
    struct msghdr msg;
    char scratch[64];
    struct cmsghdr *cmsg;
    int fd_to_send = req->send_handle->fd;

    assert(fd_to_send >= 0);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = iovcnt;
    msg.msg_flags = 0;

    msg.msg_control = (void*) scratch;
    msg.msg_controllen = CMSG_LEN(sizeof(fd_to_send));

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = msg.msg_controllen;
    *(int*) CMSG_DATA(cmsg) = fd_to_send;

    do {
      n = sendmsg(stream->fd, &msg, 0);
    }
    while (n == -1 && errno == EINTR);
  } else {
    do {
      if (iovcnt == 1) {
        n = write(stream->fd, iov[0].iov_base, iov[0].iov_len);
      } else {
        n = writev(stream->fd, iov, iovcnt);
      }
    }
    while (n == -1 && errno == EINTR);
  }

  if (n < 0) {
    if (errno != EAGAIN) {
      /* Error */
      req->error = errno;
      stream->write_queue_size -= uv__write_req_size(req);
      uv__write_req_finish(req);
      return;
    } else if (stream->blocking) {
      /* If this is a blocking stream, try again. */
      goto start;
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

        /* There is more to write. */
        if (stream->blocking) {
          /*
           * If we're blocking then we should not be enabling the write
           * watcher - instead we need to try again.
           */
          goto start;
        } else {
          /* Break loop and ensure the watcher is pending. */
          break;
        }

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
          uv__write_req_finish(req);
          /* TODO: start trying to write the next request. */
          return;
        }
      }
    }
  }

  /* Either we've counted n down to zero or we've got EAGAIN. */
  assert(n == 0 || n == -1);

  /* Only non-blocking streams should use the write_watcher. */
  assert(!stream->blocking);

  /* We're not done. */
  ev_io_start(stream->loop->ev, &stream->write_watcher);
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
      uv__set_sys_error(stream->loop, req->error);
      req->cb(req, req->error ? -1 : 0);
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
  struct msghdr msg;
  struct cmsghdr* cmsg;
  char cmsg_space[64];
  struct ev_loop* ev = stream->loop->ev;

  /* XXX: Maybe instead of having UV_READING we just test if
   * tcp->read_cb is NULL or not?
   */
  while ((stream->read_cb || stream->read2_cb) &&
         stream->flags & UV_READING) {
    assert(stream->alloc_cb);
    buf = stream->alloc_cb((uv_handle_t*)stream, 64 * 1024);

    assert(buf.len > 0);
    assert(buf.base);
    assert(stream->fd >= 0);

    if (stream->read_cb) {
      do {
        nread = read(stream->fd, buf.base, buf.len);
      }
      while (nread < 0 && errno == EINTR);
    } else {
      assert(stream->read2_cb);
      /* read2_cb uses recvmsg */
      msg.msg_flags = 0;
      msg.msg_iov = (struct iovec*) &buf;
      msg.msg_iovlen = 1;
      msg.msg_name = NULL;
      msg.msg_namelen = 0;
      /* Set up to receive a descriptor even if one isn't in the message */
      msg.msg_controllen = 64;
      msg.msg_control = (void *) cmsg_space;

      do {
        nread = recvmsg(stream->fd, &msg, 0);
      }
      while (nread < 0 && errno == EINTR);
    }


    if (nread < 0) {
      /* Error */
      if (errno == EAGAIN) {
        /* Wait for the next one. */
        if (stream->flags & UV_READING) {
          ev_io_start(ev, &stream->read_watcher);
        }
        uv__set_sys_error(stream->loop, EAGAIN);

        if (stream->read_cb) {
          stream->read_cb(stream, 0, buf);
        } else {
          stream->read2_cb((uv_pipe_t*)stream, 0, buf, UV_UNKNOWN_HANDLE);
        }

        return;
      } else {
        /* Error. User should call uv_close(). */
        uv__set_sys_error(stream->loop, errno);

        if (stream->read_cb) {
          stream->read_cb(stream, -1, buf);
        } else {
          stream->read2_cb((uv_pipe_t*)stream, -1, buf, UV_UNKNOWN_HANDLE);
        }

        assert(!ev_is_active(&stream->read_watcher));
        return;
      }

    } else if (nread == 0) {
      /* EOF */
      uv__set_artificial_error(stream->loop, UV_EOF);
      ev_io_stop(ev, &stream->read_watcher);

      if (stream->read_cb) {
        stream->read_cb(stream, -1, buf);
      } else {
        stream->read2_cb((uv_pipe_t*)stream, -1, buf, UV_UNKNOWN_HANDLE);
      }
      return;
    } else {
      /* Successful read */
      ssize_t buflen = buf.len;

      if (stream->read_cb) {
        stream->read_cb(stream, nread, buf);
      } else {
        assert(stream->read2_cb);

        /*
         * XXX: Some implementations can send multiple file descriptors in a
         * single message. We should be using CMSG_NXTHDR() to walk the
         * chain to get at them all. This would require changing the API to
         * hand these back up the caller, is a pain.
         */

        for (cmsg = CMSG_FIRSTHDR(&msg);
             msg.msg_controllen > 0 && cmsg != NULL;
             cmsg = CMSG_NXTHDR(&msg, cmsg)) {

          if (cmsg->cmsg_type == SCM_RIGHTS) {
            if (stream->accepted_fd != -1) {
              fprintf(stderr, "(libuv) ignoring extra FD received\n");
            }

            stream->accepted_fd = *(int *) CMSG_DATA(cmsg);

          } else {
            fprintf(stderr, "ignoring non-SCM_RIGHTS ancillary data: %d\n",
                cmsg->cmsg_type);
          }
        }


        if (stream->accepted_fd >= 0) {
          stream->read2_cb((uv_pipe_t*)stream, nread, buf, UV_TCP);
        } else {
          stream->read2_cb((uv_pipe_t*)stream, nread, buf, UV_UNKNOWN_HANDLE);
        }
      }

      /* Return if we didn't fill the buffer, there is no more data to read. */
      if (nread < buflen) {
        return;
      }
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
    uv__set_sys_error(stream->loop, EINVAL);
    return -1;
  }

  /* Initialize request */
  uv__req_init(stream->loop, (uv_req_t*)req);
  req->handle = stream;
  req->cb = cb;

  stream->shutdown_req = req;
  req->type = UV_SHUTDOWN;

  ((uv_handle_t*)stream)->flags |= UV_SHUTTING;


  ev_io_start(stream->loop->ev, &stream->write_watcher);

  return 0;
}


void uv__stream_io(EV_P_ ev_io* watcher, int revents) {
  uv_stream_t* stream = watcher->data;

  assert(stream->type == UV_TCP || stream->type == UV_NAMED_PIPE ||
      stream->type == UV_TTY);
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
      uv__write(stream);
      uv__write_callbacks(stream);
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
    ev_io_start(stream->loop->ev, &stream->read_watcher);

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
    uv__set_sys_error(stream->loop, error);

    stream->connect_req = NULL;
    if (req->cb) {
      req->cb(req, -1);
    }
  }
}


int uv__connect(uv_connect_t* req, uv_stream_t* stream, struct sockaddr* addr,
    socklen_t addrlen, uv_connect_cb cb) { 
  int sockfd;
  int r;

  if (stream->fd <= 0) {
    if ((sockfd = uv__socket(addr->sa_family, SOCK_STREAM, 0)) == -1) {
      uv__set_sys_error(stream->loop, errno);
      return -1;
    }

    if (uv__stream_open(stream, sockfd, UV_READABLE | UV_WRITABLE)) {
      uv__close(sockfd);
      return -2;
    }
  }

  uv__req_init(stream->loop, (uv_req_t*)req);
  req->cb = cb;
  req->handle = stream;
  req->type = UV_CONNECT;
  ngx_queue_init(&req->queue);

  if (stream->connect_req) {
    uv__set_sys_error(stream->loop, EALREADY);
    return -1;
  }

  if (stream->type != UV_TCP) {
    uv__set_sys_error(stream->loop, ENOTSOCK);
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
       *
       * XXX: do the same for ECONNABORTED?
       */
      case ECONNREFUSED:
        stream->delayed_error = errno;
        break;

      default:
        uv__set_sys_error(stream->loop, errno);
        return -1;
    }
  }

  assert(stream->write_watcher.data == stream);
  ev_io_start(stream->loop->ev, &stream->write_watcher);

  if (stream->delayed_error) {
    ev_feed_event(stream->loop->ev, &stream->write_watcher, EV_WRITE);
  }

  return 0;
}


int uv_write2(uv_write_t* req, uv_stream_t* stream, uv_buf_t bufs[], int bufcnt,
    uv_stream_t* send_handle, uv_write_cb cb) {
  int empty_queue;

  assert((stream->type == UV_TCP || stream->type == UV_NAMED_PIPE ||
      stream->type == UV_TTY) &&
      "uv_write (unix) does not yet support other types of streams");

  if (stream->fd < 0) {
    uv__set_sys_error(stream->loop, EBADF);
    return -1;
  }

  if (send_handle) {
    if (stream->type != UV_NAMED_PIPE || !((uv_pipe_t*)stream)->ipc) {
      uv__set_sys_error(stream->loop, EOPNOTSUPP);
      return -1;
    }
  }

  empty_queue = (stream->write_queue_size == 0);

  /* Initialize the req */
  uv__req_init(stream->loop, (uv_req_t*)req);
  req->cb = cb;
  req->handle = stream;
  req->error = 0;
  req->send_handle = send_handle;
  req->type = UV_WRITE;
  ngx_queue_init(&req->queue);

  if (bufcnt <= UV_REQ_BUFSML_SIZE) {
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
    uv__write(stream);
  } else {
    /*
     * blocking streams should never have anything in the queue.
     * if this assert fires then somehow the blocking stream isn't being
     * sufficently flushed in uv__write.
     */
    assert(!stream->blocking);

    ev_io_start(stream->loop->ev, &stream->write_watcher);
  }

  return 0;
}


/* The buffers to be written must remain valid until the callback is called.
 * This is not required for the uv_buf_t array.
 */
int uv_write(uv_write_t* req, uv_stream_t* stream, uv_buf_t bufs[], int bufcnt,
    uv_write_cb cb) {
  return uv_write2(req, stream, bufs, bufcnt, NULL, cb);
}


int uv__read_start_common(uv_stream_t* stream, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb, uv_read2_cb read2_cb) {
  assert(stream->type == UV_TCP || stream->type == UV_NAMED_PIPE ||
      stream->type == UV_TTY);

  if (stream->flags & UV_CLOSING) {
    uv__set_sys_error(stream->loop, EINVAL);
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
  stream->read2_cb = read2_cb;
  stream->alloc_cb = alloc_cb;

  /* These should have been set by uv_tcp_init. */
  assert(stream->read_watcher.cb == uv__stream_io);

  ev_io_start(stream->loop->ev, &stream->read_watcher);
  return 0;
}


int uv_read_start(uv_stream_t* stream, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb) {
  return uv__read_start_common(stream, alloc_cb, read_cb, NULL);
}


int uv_read2_start(uv_stream_t* stream, uv_alloc_cb alloc_cb,
    uv_read2_cb read_cb) {
  return uv__read_start_common(stream, alloc_cb, NULL, read_cb);
}


int uv_read_stop(uv_stream_t* stream) {
  ev_io_stop(stream->loop->ev, &stream->read_watcher);
  stream->flags &= ~UV_READING;
  stream->read_cb = NULL;
  stream->read2_cb = NULL;
  stream->alloc_cb = NULL;
  return 0;
}


int uv_is_readable(uv_stream_t* stream) {
  return stream->flags & UV_READABLE;
}


int uv_is_writable(uv_stream_t* stream) {
  return stream->flags & UV_WRITABLE;
}
