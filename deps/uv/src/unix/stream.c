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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>
#include <limits.h> /* IOV_MAX */

#if defined(__APPLE__)
# include <sys/event.h>
# include <sys/time.h>
# include <sys/select.h>

/* Forward declaration */
typedef struct uv__stream_select_s uv__stream_select_t;

struct uv__stream_select_s {
  uv_stream_t* stream;
  uv_thread_t thread;
  uv_sem_t close_sem;
  uv_sem_t async_sem;
  uv_async_t async;
  int events;
  int fake_fd;
  int int_fd;
  int fd;
  fd_set* sread;
  size_t sread_sz;
  fd_set* swrite;
  size_t swrite_sz;
};
#endif /* defined(__APPLE__) */

static void uv__stream_connect(uv_stream_t*);
static void uv__write(uv_stream_t* stream);
static void uv__read(uv_stream_t* stream);
static void uv__stream_io(uv_loop_t* loop, uv__io_t* w, unsigned int events);
static void uv__write_callbacks(uv_stream_t* stream);
static size_t uv__write_req_size(uv_write_t* req);


void uv__stream_init(uv_loop_t* loop,
                     uv_stream_t* stream,
                     uv_handle_type type) {
  int err;

  uv__handle_init(loop, (uv_handle_t*)stream, type);
  stream->read_cb = NULL;
  stream->alloc_cb = NULL;
  stream->close_cb = NULL;
  stream->connection_cb = NULL;
  stream->connect_req = NULL;
  stream->shutdown_req = NULL;
  stream->accepted_fd = -1;
  stream->queued_fds = NULL;
  stream->delayed_error = 0;
  QUEUE_INIT(&stream->write_queue);
  QUEUE_INIT(&stream->write_completed_queue);
  stream->write_queue_size = 0;

  if (loop->emfile_fd == -1) {
    err = uv__open_cloexec("/dev/null", O_RDONLY);
    if (err < 0)
        /* In the rare case that "/dev/null" isn't mounted open "/"
         * instead.
         */
        err = uv__open_cloexec("/", O_RDONLY);
    if (err >= 0)
      loop->emfile_fd = err;
  }

#if defined(__APPLE__)
  stream->select = NULL;
#endif /* defined(__APPLE_) */

  uv__io_init(&stream->io_watcher, uv__stream_io, -1);
}


static void uv__stream_osx_interrupt_select(uv_stream_t* stream) {
#if defined(__APPLE__)
  /* Notify select() thread about state change */
  uv__stream_select_t* s;
  int r;

  s = stream->select;
  if (s == NULL)
    return;

  /* Interrupt select() loop
   * NOTE: fake_fd and int_fd are socketpair(), thus writing to one will
   * emit read event on other side
   */
  do
    r = write(s->fake_fd, "x", 1);
  while (r == -1 && errno == EINTR);

  assert(r == 1);
#else  /* !defined(__APPLE__) */
  /* No-op on any other platform */
#endif  /* !defined(__APPLE__) */
}


#if defined(__APPLE__)
static void uv__stream_osx_select(void* arg) {
  uv_stream_t* stream;
  uv__stream_select_t* s;
  char buf[1024];
  int events;
  int fd;
  int r;
  int max_fd;

  stream = arg;
  s = stream->select;
  fd = s->fd;

  if (fd > s->int_fd)
    max_fd = fd;
  else
    max_fd = s->int_fd;

  while (1) {
    /* Terminate on semaphore */
    if (uv_sem_trywait(&s->close_sem) == 0)
      break;

    /* Watch fd using select(2) */
    memset(s->sread, 0, s->sread_sz);
    memset(s->swrite, 0, s->swrite_sz);

    if (uv__io_active(&stream->io_watcher, POLLIN))
      FD_SET(fd, s->sread);
    if (uv__io_active(&stream->io_watcher, POLLOUT))
      FD_SET(fd, s->swrite);
    FD_SET(s->int_fd, s->sread);

    /* Wait indefinitely for fd events */
    r = select(max_fd + 1, s->sread, s->swrite, NULL, NULL);
    if (r == -1) {
      if (errno == EINTR)
        continue;

      /* XXX: Possible?! */
      abort();
    }

    /* Ignore timeouts */
    if (r == 0)
      continue;

    /* Empty socketpair's buffer in case of interruption */
    if (FD_ISSET(s->int_fd, s->sread))
      while (1) {
        r = read(s->int_fd, buf, sizeof(buf));

        if (r == sizeof(buf))
          continue;

        if (r != -1)
          break;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
          break;

        if (errno == EINTR)
          continue;

        abort();
      }

    /* Handle events */
    events = 0;
    if (FD_ISSET(fd, s->sread))
      events |= POLLIN;
    if (FD_ISSET(fd, s->swrite))
      events |= POLLOUT;

    assert(events != 0 || FD_ISSET(s->int_fd, s->sread));
    if (events != 0) {
      ACCESS_ONCE(int, s->events) = events;

      uv_async_send(&s->async);
      uv_sem_wait(&s->async_sem);

      /* Should be processed at this stage */
      assert((s->events == 0) || (stream->flags & UV_CLOSING));
    }
  }
}


static void uv__stream_osx_select_cb(uv_async_t* handle) {
  uv__stream_select_t* s;
  uv_stream_t* stream;
  int events;

  s = container_of(handle, uv__stream_select_t, async);
  stream = s->stream;

  /* Get and reset stream's events */
  events = s->events;
  ACCESS_ONCE(int, s->events) = 0;

  assert(events != 0);
  assert(events == (events & (POLLIN | POLLOUT)));

  /* Invoke callback on event-loop */
  if ((events & POLLIN) && uv__io_active(&stream->io_watcher, POLLIN))
    uv__stream_io(stream->loop, &stream->io_watcher, POLLIN);

  if ((events & POLLOUT) && uv__io_active(&stream->io_watcher, POLLOUT))
    uv__stream_io(stream->loop, &stream->io_watcher, POLLOUT);

  if (stream->flags & UV_CLOSING)
    return;

  /* NOTE: It is important to do it here, otherwise `select()` might be called
   * before the actual `uv__read()`, leading to the blocking syscall
   */
  uv_sem_post(&s->async_sem);
}


static void uv__stream_osx_cb_close(uv_handle_t* async) {
  uv__stream_select_t* s;

  s = container_of(async, uv__stream_select_t, async);
  uv__free(s);
}


int uv__stream_try_select(uv_stream_t* stream, int* fd) {
  /*
   * kqueue doesn't work with some files from /dev mount on osx.
   * select(2) in separate thread for those fds
   */

  struct kevent filter[1];
  struct kevent events[1];
  struct timespec timeout;
  uv__stream_select_t* s;
  int fds[2];
  int err;
  int ret;
  int kq;
  int old_fd;
  int max_fd;
  size_t sread_sz;
  size_t swrite_sz;

  kq = kqueue();
  if (kq == -1) {
    perror("(libuv) kqueue()");
    return -errno;
  }

  EV_SET(&filter[0], *fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, 0);

  /* Use small timeout, because we only want to capture EINVALs */
  timeout.tv_sec = 0;
  timeout.tv_nsec = 1;

  do
    ret = kevent(kq, filter, 1, events, 1, &timeout);
  while (ret == -1 && errno == EINTR);

  uv__close(kq);

  if (ret == -1)
    return -errno;

  if (ret == 0 || (events[0].flags & EV_ERROR) == 0 || events[0].data != EINVAL)
    return 0;

  /* At this point we definitely know that this fd won't work with kqueue */

  /*
   * Create fds for io watcher and to interrupt the select() loop.
   * NOTE: do it ahead of malloc below to allocate enough space for fd_sets
   */
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds))
    return -errno;

  max_fd = *fd;
  if (fds[1] > max_fd)
    max_fd = fds[1];

  sread_sz = ROUND_UP(max_fd + 1, sizeof(uint32_t) * NBBY) / NBBY;
  swrite_sz = sread_sz;

  s = uv__malloc(sizeof(*s) + sread_sz + swrite_sz);
  if (s == NULL) {
    err = -ENOMEM;
    goto failed_malloc;
  }

  s->events = 0;
  s->fd = *fd;
  s->sread = (fd_set*) ((char*) s + sizeof(*s));
  s->sread_sz = sread_sz;
  s->swrite = (fd_set*) ((char*) s->sread + sread_sz);
  s->swrite_sz = swrite_sz;

  err = uv_async_init(stream->loop, &s->async, uv__stream_osx_select_cb);
  if (err)
    goto failed_async_init;

  s->async.flags |= UV__HANDLE_INTERNAL;
  uv__handle_unref(&s->async);

  err = uv_sem_init(&s->close_sem, 0);
  if (err != 0)
    goto failed_close_sem_init;

  err = uv_sem_init(&s->async_sem, 0);
  if (err != 0)
    goto failed_async_sem_init;

  s->fake_fd = fds[0];
  s->int_fd = fds[1];

  old_fd = *fd;
  s->stream = stream;
  stream->select = s;
  *fd = s->fake_fd;

  err = uv_thread_create(&s->thread, uv__stream_osx_select, stream);
  if (err != 0)
    goto failed_thread_create;

  return 0;

failed_thread_create:
  s->stream = NULL;
  stream->select = NULL;
  *fd = old_fd;

  uv_sem_destroy(&s->async_sem);

failed_async_sem_init:
  uv_sem_destroy(&s->close_sem);

failed_close_sem_init:
  uv__close(fds[0]);
  uv__close(fds[1]);
  uv_close((uv_handle_t*) &s->async, uv__stream_osx_cb_close);
  return err;

failed_async_init:
  uv__free(s);

failed_malloc:
  uv__close(fds[0]);
  uv__close(fds[1]);

  return err;
}
#endif /* defined(__APPLE__) */


int uv__stream_open(uv_stream_t* stream, int fd, int flags) {
#if defined(__APPLE__)
  int enable;
#endif

  if (!(stream->io_watcher.fd == -1 || stream->io_watcher.fd == fd))
    return -EBUSY;

  assert(fd >= 0);
  stream->flags |= flags;

  if (stream->type == UV_TCP) {
    if ((stream->flags & UV_TCP_NODELAY) && uv__tcp_nodelay(fd, 1))
      return -errno;

    /* TODO Use delay the user passed in. */
    if ((stream->flags & UV_TCP_KEEPALIVE) && uv__tcp_keepalive(fd, 1, 60))
      return -errno;
  }

#if defined(__APPLE__)
  enable = 1;
  if (setsockopt(fd, SOL_SOCKET, SO_OOBINLINE, &enable, sizeof(enable)) &&
      errno != ENOTSOCK &&
      errno != EINVAL) {
    return -errno;
  }
#endif

  stream->io_watcher.fd = fd;

  return 0;
}


void uv__stream_flush_write_queue(uv_stream_t* stream, int error) {
  uv_write_t* req;
  QUEUE* q;
  while (!QUEUE_EMPTY(&stream->write_queue)) {
    q = QUEUE_HEAD(&stream->write_queue);
    QUEUE_REMOVE(q);

    req = QUEUE_DATA(q, uv_write_t, queue);
    req->error = error;

    QUEUE_INSERT_TAIL(&stream->write_completed_queue, &req->queue);
  }
}


void uv__stream_destroy(uv_stream_t* stream) {
  assert(!uv__io_active(&stream->io_watcher, POLLIN | POLLOUT));
  assert(stream->flags & UV_CLOSED);

  if (stream->connect_req) {
    uv__req_unregister(stream->loop, stream->connect_req);
    stream->connect_req->cb(stream->connect_req, -ECANCELED);
    stream->connect_req = NULL;
  }

  uv__stream_flush_write_queue(stream, -ECANCELED);
  uv__write_callbacks(stream);

  if (stream->shutdown_req) {
    /* The ECANCELED error code is a lie, the shutdown(2) syscall is a
     * fait accompli at this point. Maybe we should revisit this in v0.11.
     * A possible reason for leaving it unchanged is that it informs the
     * callee that the handle has been destroyed.
     */
    uv__req_unregister(stream->loop, stream->shutdown_req);
    stream->shutdown_req->cb(stream->shutdown_req, -ECANCELED);
    stream->shutdown_req = NULL;
  }

  assert(stream->write_queue_size == 0);
}


/* Implements a best effort approach to mitigating accept() EMFILE errors.
 * We have a spare file descriptor stashed away that we close to get below
 * the EMFILE limit. Next, we accept all pending connections and close them
 * immediately to signal the clients that we're overloaded - and we are, but
 * we still keep on trucking.
 *
 * There is one caveat: it's not reliable in a multi-threaded environment.
 * The file descriptor limit is per process. Our party trick fails if another
 * thread opens a file or creates a socket in the time window between us
 * calling close() and accept().
 */
static int uv__emfile_trick(uv_loop_t* loop, int accept_fd) {
  int err;
  int emfile_fd;

  if (loop->emfile_fd == -1)
    return -EMFILE;

  uv__close(loop->emfile_fd);
  loop->emfile_fd = -1;

  do {
    err = uv__accept(accept_fd);
    if (err >= 0)
      uv__close(err);
  } while (err >= 0 || err == -EINTR);

  emfile_fd = uv__open_cloexec("/", O_RDONLY);
  if (emfile_fd >= 0)
    loop->emfile_fd = emfile_fd;

  return err;
}


#if defined(UV_HAVE_KQUEUE)
# define UV_DEC_BACKLOG(w) w->rcount--;
#else
# define UV_DEC_BACKLOG(w) /* no-op */
#endif /* defined(UV_HAVE_KQUEUE) */


void uv__server_io(uv_loop_t* loop, uv__io_t* w, unsigned int events) {
  uv_stream_t* stream;
  int err;

  stream = container_of(w, uv_stream_t, io_watcher);
  assert(events & POLLIN);
  assert(stream->accepted_fd == -1);
  assert(!(stream->flags & UV_CLOSING));

  uv__io_start(stream->loop, &stream->io_watcher, POLLIN);

  /* connection_cb can close the server socket while we're
   * in the loop so check it on each iteration.
   */
  while (uv__stream_fd(stream) != -1) {
    assert(stream->accepted_fd == -1);

#if defined(UV_HAVE_KQUEUE)
    if (w->rcount <= 0)
      return;
#endif /* defined(UV_HAVE_KQUEUE) */

    err = uv__accept(uv__stream_fd(stream));
    if (err < 0) {
      if (err == -EAGAIN || err == -EWOULDBLOCK)
        return;  /* Not an error. */

      if (err == -ECONNABORTED)
        continue;  /* Ignore. Nothing we can do about that. */

      if (err == -EMFILE || err == -ENFILE) {
        err = uv__emfile_trick(loop, uv__stream_fd(stream));
        if (err == -EAGAIN || err == -EWOULDBLOCK)
          break;
      }

      stream->connection_cb(stream, err);
      continue;
    }

    UV_DEC_BACKLOG(w)
    stream->accepted_fd = err;
    stream->connection_cb(stream, 0);

    if (stream->accepted_fd != -1) {
      /* The user hasn't yet accepted called uv_accept() */
      uv__io_stop(loop, &stream->io_watcher, POLLIN);
      return;
    }

    if (stream->type == UV_TCP && (stream->flags & UV_TCP_SINGLE_ACCEPT)) {
      /* Give other processes a chance to accept connections. */
      struct timespec timeout = { 0, 1 };
      nanosleep(&timeout, NULL);
    }
  }
}


#undef UV_DEC_BACKLOG


int uv_accept(uv_stream_t* server, uv_stream_t* client) {
  int err;

  assert(server->loop == client->loop);

  if (server->accepted_fd == -1)
    return -EAGAIN;

  switch (client->type) {
    case UV_NAMED_PIPE:
    case UV_TCP:
      err = uv__stream_open(client,
                            server->accepted_fd,
                            UV_STREAM_READABLE | UV_STREAM_WRITABLE);
      if (err) {
        /* TODO handle error */
        uv__close(server->accepted_fd);
        goto done;
      }
      break;

    case UV_UDP:
      err = uv_udp_open((uv_udp_t*) client, server->accepted_fd);
      if (err) {
        uv__close(server->accepted_fd);
        goto done;
      }
      break;

    default:
      return -EINVAL;
  }

  client->flags |= UV_HANDLE_BOUND;

done:
  /* Process queued fds */
  if (server->queued_fds != NULL) {
    uv__stream_queued_fds_t* queued_fds;

    queued_fds = server->queued_fds;

    /* Read first */
    server->accepted_fd = queued_fds->fds[0];

    /* All read, free */
    assert(queued_fds->offset > 0);
    if (--queued_fds->offset == 0) {
      uv__free(queued_fds);
      server->queued_fds = NULL;
    } else {
      /* Shift rest */
      memmove(queued_fds->fds,
              queued_fds->fds + 1,
              queued_fds->offset * sizeof(*queued_fds->fds));
    }
  } else {
    server->accepted_fd = -1;
    if (err == 0)
      uv__io_start(server->loop, &server->io_watcher, POLLIN);
  }
  return err;
}


int uv_listen(uv_stream_t* stream, int backlog, uv_connection_cb cb) {
  int err;

  switch (stream->type) {
  case UV_TCP:
    err = uv_tcp_listen((uv_tcp_t*)stream, backlog, cb);
    break;

  case UV_NAMED_PIPE:
    err = uv_pipe_listen((uv_pipe_t*)stream, backlog, cb);
    break;

  default:
    err = -EINVAL;
  }

  if (err == 0)
    uv__handle_start(stream);

  return err;
}


static void uv__drain(uv_stream_t* stream) {
  uv_shutdown_t* req;
  int err;

  assert(QUEUE_EMPTY(&stream->write_queue));
  uv__io_stop(stream->loop, &stream->io_watcher, POLLOUT);
  uv__stream_osx_interrupt_select(stream);

  /* Shutdown? */
  if ((stream->flags & UV_STREAM_SHUTTING) &&
      !(stream->flags & UV_CLOSING) &&
      !(stream->flags & UV_STREAM_SHUT)) {
    assert(stream->shutdown_req);

    req = stream->shutdown_req;
    stream->shutdown_req = NULL;
    stream->flags &= ~UV_STREAM_SHUTTING;
    uv__req_unregister(stream->loop, req);

    err = 0;
    if (shutdown(uv__stream_fd(stream), SHUT_WR))
      err = -errno;

    if (err == 0)
      stream->flags |= UV_STREAM_SHUT;

    if (req->cb != NULL)
      req->cb(req, err);
  }
}


static size_t uv__write_req_size(uv_write_t* req) {
  size_t size;

  assert(req->bufs != NULL);
  size = uv__count_bufs(req->bufs + req->write_index,
                        req->nbufs - req->write_index);
  assert(req->handle->write_queue_size >= size);

  return size;
}


static void uv__write_req_finish(uv_write_t* req) {
  uv_stream_t* stream = req->handle;

  /* Pop the req off tcp->write_queue. */
  QUEUE_REMOVE(&req->queue);

  /* Only free when there was no error. On error, we touch up write_queue_size
   * right before making the callback. The reason we don't do that right away
   * is that a write_queue_size > 0 is our only way to signal to the user that
   * they should stop writing - which they should if we got an error. Something
   * to revisit in future revisions of the libuv API.
   */
  if (req->error == 0) {
    if (req->bufs != req->bufsml)
      uv__free(req->bufs);
    req->bufs = NULL;
  }

  /* Add it to the write_completed_queue where it will have its
   * callback called in the near future.
   */
  QUEUE_INSERT_TAIL(&stream->write_completed_queue, &req->queue);
  uv__io_feed(stream->loop, &stream->io_watcher);
}


static int uv__handle_fd(uv_handle_t* handle) {
  switch (handle->type) {
    case UV_NAMED_PIPE:
    case UV_TCP:
      return ((uv_stream_t*) handle)->io_watcher.fd;

    case UV_UDP:
      return ((uv_udp_t*) handle)->io_watcher.fd;

    default:
      return -1;
  }
}

static void uv__write(uv_stream_t* stream) {
  struct iovec* iov;
  QUEUE* q;
  uv_write_t* req;
  int iovmax;
  int iovcnt;
  ssize_t n;
  int err;

start:

  assert(uv__stream_fd(stream) >= 0);

  if (QUEUE_EMPTY(&stream->write_queue))
    return;

  q = QUEUE_HEAD(&stream->write_queue);
  req = QUEUE_DATA(q, uv_write_t, queue);
  assert(req->handle == stream);

  /*
   * Cast to iovec. We had to have our own uv_buf_t instead of iovec
   * because Windows's WSABUF is not an iovec.
   */
  assert(sizeof(uv_buf_t) == sizeof(struct iovec));
  iov = (struct iovec*) &(req->bufs[req->write_index]);
  iovcnt = req->nbufs - req->write_index;

  iovmax = uv__getiovmax();

  /* Limit iov count to avoid EINVALs from writev() */
  if (iovcnt > iovmax)
    iovcnt = iovmax;

  /*
   * Now do the actual writev. Note that we've been updating the pointers
   * inside the iov each time we write. So there is no need to offset it.
   */

  if (req->send_handle) {
    int fd_to_send;
    struct msghdr msg;
    struct cmsghdr *cmsg;
    union {
      char data[64];
      struct cmsghdr alias;
    } scratch;

    if (uv__is_closing(req->send_handle)) {
      err = -EBADF;
      goto error;
    }

    fd_to_send = uv__handle_fd((uv_handle_t*) req->send_handle);

    memset(&scratch, 0, sizeof(scratch));

    assert(fd_to_send >= 0);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = iovcnt;
    msg.msg_flags = 0;

    msg.msg_control = &scratch.alias;
    msg.msg_controllen = CMSG_SPACE(sizeof(fd_to_send));

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd_to_send));

    /* silence aliasing warning */
    {
      void* pv = CMSG_DATA(cmsg);
      int* pi = pv;
      *pi = fd_to_send;
    }

    do {
      n = sendmsg(uv__stream_fd(stream), &msg, 0);
    }
#if defined(__APPLE__)
    /*
     * Due to a possible kernel bug at least in OS X 10.10 "Yosemite",
     * EPROTOTYPE can be returned while trying to write to a socket that is
     * shutting down. If we retry the write, we should get the expected EPIPE
     * instead.
     */
    while (n == -1 && (errno == EINTR || errno == EPROTOTYPE));
#else
    while (n == -1 && errno == EINTR);
#endif
  } else {
    do {
      if (iovcnt == 1) {
        n = write(uv__stream_fd(stream), iov[0].iov_base, iov[0].iov_len);
      } else {
        n = writev(uv__stream_fd(stream), iov, iovcnt);
      }
    }
#if defined(__APPLE__)
    /*
     * Due to a possible kernel bug at least in OS X 10.10 "Yosemite",
     * EPROTOTYPE can be returned while trying to write to a socket that is
     * shutting down. If we retry the write, we should get the expected EPIPE
     * instead.
     */
    while (n == -1 && (errno == EINTR || errno == EPROTOTYPE));
#else
    while (n == -1 && errno == EINTR);
#endif
  }

  if (n < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      err = -errno;
      goto error;
    } else if (stream->flags & UV_STREAM_BLOCKING) {
      /* If this is a blocking stream, try again. */
      goto start;
    }
  } else {
    /* Successful write */

    while (n >= 0) {
      uv_buf_t* buf = &(req->bufs[req->write_index]);
      size_t len = buf->len;

      assert(req->write_index < req->nbufs);

      if ((size_t)n < len) {
        buf->base += n;
        buf->len -= n;
        stream->write_queue_size -= n;
        n = 0;

        /* There is more to write. */
        if (stream->flags & UV_STREAM_BLOCKING) {
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

        if (req->write_index == req->nbufs) {
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
  assert(!(stream->flags & UV_STREAM_BLOCKING));

  /* We're not done. */
  uv__io_start(stream->loop, &stream->io_watcher, POLLOUT);

  /* Notify select() thread about state change */
  uv__stream_osx_interrupt_select(stream);

  return;

error:
  req->error = err;
  uv__write_req_finish(req);
  uv__io_stop(stream->loop, &stream->io_watcher, POLLOUT);
  if (!uv__io_active(&stream->io_watcher, POLLIN))
    uv__handle_stop(stream);
  uv__stream_osx_interrupt_select(stream);
}


static void uv__write_callbacks(uv_stream_t* stream) {
  uv_write_t* req;
  QUEUE* q;

  while (!QUEUE_EMPTY(&stream->write_completed_queue)) {
    /* Pop a req off write_completed_queue. */
    q = QUEUE_HEAD(&stream->write_completed_queue);
    req = QUEUE_DATA(q, uv_write_t, queue);
    QUEUE_REMOVE(q);
    uv__req_unregister(stream->loop, req);

    if (req->bufs != NULL) {
      stream->write_queue_size -= uv__write_req_size(req);
      if (req->bufs != req->bufsml)
        uv__free(req->bufs);
      req->bufs = NULL;
    }

    /* NOTE: call callback AFTER freeing the request data. */
    if (req->cb)
      req->cb(req, req->error);
  }

  assert(QUEUE_EMPTY(&stream->write_completed_queue));
}


uv_handle_type uv__handle_type(int fd) {
  struct sockaddr_storage ss;
  socklen_t sslen;
  socklen_t len;
  int type;

  memset(&ss, 0, sizeof(ss));
  sslen = sizeof(ss);

  if (getsockname(fd, (struct sockaddr*)&ss, &sslen))
    return UV_UNKNOWN_HANDLE;

  len = sizeof type;

  if (getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &len))
    return UV_UNKNOWN_HANDLE;

  if (type == SOCK_STREAM) {
#if defined(_AIX) || defined(__DragonFly__)
    /* on AIX/DragonFly the getsockname call returns an empty sa structure
     * for sockets of type AF_UNIX.  For all other types it will
     * return a properly filled in structure.
     */
    if (sslen == 0)
      return UV_NAMED_PIPE;
#endif
    switch (ss.ss_family) {
      case AF_UNIX:
        return UV_NAMED_PIPE;
      case AF_INET:
      case AF_INET6:
        return UV_TCP;
      }
  }

  if (type == SOCK_DGRAM &&
      (ss.ss_family == AF_INET || ss.ss_family == AF_INET6))
    return UV_UDP;

  return UV_UNKNOWN_HANDLE;
}


static void uv__stream_eof(uv_stream_t* stream, const uv_buf_t* buf) {
  stream->flags |= UV_STREAM_READ_EOF;
  uv__io_stop(stream->loop, &stream->io_watcher, POLLIN);
  if (!uv__io_active(&stream->io_watcher, POLLOUT))
    uv__handle_stop(stream);
  uv__stream_osx_interrupt_select(stream);
  stream->read_cb(stream, UV_EOF, buf);
  stream->flags &= ~UV_STREAM_READING;
}


static int uv__stream_queue_fd(uv_stream_t* stream, int fd) {
  uv__stream_queued_fds_t* queued_fds;
  unsigned int queue_size;

  queued_fds = stream->queued_fds;
  if (queued_fds == NULL) {
    queue_size = 8;
    queued_fds = uv__malloc((queue_size - 1) * sizeof(*queued_fds->fds) +
                            sizeof(*queued_fds));
    if (queued_fds == NULL)
      return -ENOMEM;
    queued_fds->size = queue_size;
    queued_fds->offset = 0;
    stream->queued_fds = queued_fds;

    /* Grow */
  } else if (queued_fds->size == queued_fds->offset) {
    queue_size = queued_fds->size + 8;
    queued_fds = uv__realloc(queued_fds,
                             (queue_size - 1) * sizeof(*queued_fds->fds) +
                              sizeof(*queued_fds));

    /*
     * Allocation failure, report back.
     * NOTE: if it is fatal - sockets will be closed in uv__stream_close
     */
    if (queued_fds == NULL)
      return -ENOMEM;
    queued_fds->size = queue_size;
    stream->queued_fds = queued_fds;
  }

  /* Put fd in a queue */
  queued_fds->fds[queued_fds->offset++] = fd;

  return 0;
}


#define UV__CMSG_FD_COUNT 64
#define UV__CMSG_FD_SIZE (UV__CMSG_FD_COUNT * sizeof(int))


static int uv__stream_recv_cmsg(uv_stream_t* stream, struct msghdr* msg) {
  struct cmsghdr* cmsg;

  for (cmsg = CMSG_FIRSTHDR(msg); cmsg != NULL; cmsg = CMSG_NXTHDR(msg, cmsg)) {
    char* start;
    char* end;
    int err;
    void* pv;
    int* pi;
    unsigned int i;
    unsigned int count;

    if (cmsg->cmsg_type != SCM_RIGHTS) {
      fprintf(stderr, "ignoring non-SCM_RIGHTS ancillary data: %d\n",
          cmsg->cmsg_type);
      continue;
    }

    /* silence aliasing warning */
    pv = CMSG_DATA(cmsg);
    pi = pv;

    /* Count available fds */
    start = (char*) cmsg;
    end = (char*) cmsg + cmsg->cmsg_len;
    count = 0;
    while (start + CMSG_LEN(count * sizeof(*pi)) < end)
      count++;
    assert(start + CMSG_LEN(count * sizeof(*pi)) == end);

    for (i = 0; i < count; i++) {
      /* Already has accepted fd, queue now */
      if (stream->accepted_fd != -1) {
        err = uv__stream_queue_fd(stream, pi[i]);
        if (err != 0) {
          /* Close rest */
          for (; i < count; i++)
            uv__close(pi[i]);
          return err;
        }
      } else {
        stream->accepted_fd = pi[i];
      }
    }
  }

  return 0;
}


#ifdef __clang__
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wgnu-folding-constant"
#endif

static void uv__read(uv_stream_t* stream) {
  uv_buf_t buf;
  ssize_t nread;
  struct msghdr msg;
  char cmsg_space[CMSG_SPACE(UV__CMSG_FD_SIZE)];
  int count;
  int err;
  int is_ipc;

  stream->flags &= ~UV_STREAM_READ_PARTIAL;

  /* Prevent loop starvation when the data comes in as fast as (or faster than)
   * we can read it. XXX Need to rearm fd if we switch to edge-triggered I/O.
   */
  count = 32;

  is_ipc = stream->type == UV_NAMED_PIPE && ((uv_pipe_t*) stream)->ipc;

  /* XXX: Maybe instead of having UV_STREAM_READING we just test if
   * tcp->read_cb is NULL or not?
   */
  while (stream->read_cb
      && (stream->flags & UV_STREAM_READING)
      && (count-- > 0)) {
    assert(stream->alloc_cb != NULL);

    buf = uv_buf_init(NULL, 0);
    stream->alloc_cb((uv_handle_t*)stream, 64 * 1024, &buf);
    if (buf.base == NULL || buf.len == 0) {
      /* User indicates it can't or won't handle the read. */
      stream->read_cb(stream, UV_ENOBUFS, &buf);
      return;
    }

    assert(buf.base != NULL);
    assert(uv__stream_fd(stream) >= 0);

    if (!is_ipc) {
      do {
        nread = read(uv__stream_fd(stream), buf.base, buf.len);
      }
      while (nread < 0 && errno == EINTR);
    } else {
      /* ipc uses recvmsg */
      msg.msg_flags = 0;
      msg.msg_iov = (struct iovec*) &buf;
      msg.msg_iovlen = 1;
      msg.msg_name = NULL;
      msg.msg_namelen = 0;
      /* Set up to receive a descriptor even if one isn't in the message */
      msg.msg_controllen = sizeof(cmsg_space);
      msg.msg_control = cmsg_space;

      do {
        nread = uv__recvmsg(uv__stream_fd(stream), &msg, 0);
      }
      while (nread < 0 && errno == EINTR);
    }

    if (nread < 0) {
      /* Error */
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        /* Wait for the next one. */
        if (stream->flags & UV_STREAM_READING) {
          uv__io_start(stream->loop, &stream->io_watcher, POLLIN);
          uv__stream_osx_interrupt_select(stream);
        }
        stream->read_cb(stream, 0, &buf);
#if defined(__CYGWIN__) || defined(__MSYS__)
      } else if (errno == ECONNRESET && stream->type == UV_NAMED_PIPE) {
        uv__stream_eof(stream, &buf);
        return;
#endif
      } else {
        /* Error. User should call uv_close(). */
        stream->read_cb(stream, -errno, &buf);
        if (stream->flags & UV_STREAM_READING) {
          stream->flags &= ~UV_STREAM_READING;
          uv__io_stop(stream->loop, &stream->io_watcher, POLLIN);
          if (!uv__io_active(&stream->io_watcher, POLLOUT))
            uv__handle_stop(stream);
          uv__stream_osx_interrupt_select(stream);
        }
      }
      return;
    } else if (nread == 0) {
      uv__stream_eof(stream, &buf);
      return;
    } else {
      /* Successful read */
      ssize_t buflen = buf.len;

      if (is_ipc) {
        err = uv__stream_recv_cmsg(stream, &msg);
        if (err != 0) {
          stream->read_cb(stream, err, &buf);
          return;
        }
      }

#if defined(__MVS__)
      if (is_ipc && msg.msg_controllen > 0) {
        uv_buf_t blankbuf;
        int nread;
        struct iovec *old;

        blankbuf.base = 0;
        blankbuf.len = 0;
        old = msg.msg_iov;
        msg.msg_iov = (struct iovec*) &blankbuf;
        nread = 0;
        do {
          nread = uv__recvmsg(uv__stream_fd(stream), &msg, 0);
          err = uv__stream_recv_cmsg(stream, &msg);
          if (err != 0) {
            stream->read_cb(stream, err, &buf);
            msg.msg_iov = old;
            return;
          }
        } while (nread == 0 && msg.msg_controllen > 0);
        msg.msg_iov = old;
      }
#endif
      stream->read_cb(stream, nread, &buf);

      /* Return if we didn't fill the buffer, there is no more data to read. */
      if (nread < buflen) {
        stream->flags |= UV_STREAM_READ_PARTIAL;
        return;
      }
    }
  }
}


#ifdef __clang__
# pragma clang diagnostic pop
#endif

#undef UV__CMSG_FD_COUNT
#undef UV__CMSG_FD_SIZE


int uv_shutdown(uv_shutdown_t* req, uv_stream_t* stream, uv_shutdown_cb cb) {
  assert((stream->type == UV_TCP || stream->type == UV_NAMED_PIPE) &&
         "uv_shutdown (unix) only supports uv_handle_t right now");

  if (!(stream->flags & UV_STREAM_WRITABLE) ||
      stream->flags & UV_STREAM_SHUT ||
      stream->flags & UV_STREAM_SHUTTING ||
      uv__is_closing(stream)) {
    return -ENOTCONN;
  }

  assert(uv__stream_fd(stream) >= 0);

  /* Initialize request */
  uv__req_init(stream->loop, req, UV_SHUTDOWN);
  req->handle = stream;
  req->cb = cb;
  stream->shutdown_req = req;
  stream->flags |= UV_STREAM_SHUTTING;

  uv__io_start(stream->loop, &stream->io_watcher, POLLOUT);
  uv__stream_osx_interrupt_select(stream);

  return 0;
}


static void uv__stream_io(uv_loop_t* loop, uv__io_t* w, unsigned int events) {
  uv_stream_t* stream;

  stream = container_of(w, uv_stream_t, io_watcher);

  assert(stream->type == UV_TCP ||
         stream->type == UV_NAMED_PIPE ||
         stream->type == UV_TTY);
  assert(!(stream->flags & UV_CLOSING));

  if (stream->connect_req) {
    uv__stream_connect(stream);
    return;
  }

  assert(uv__stream_fd(stream) >= 0);

  /* Ignore POLLHUP here. Even it it's set, there may still be data to read. */
  if (events & (POLLIN | POLLERR | POLLHUP))
    uv__read(stream);

  if (uv__stream_fd(stream) == -1)
    return;  /* read_cb closed stream. */

  /* Short-circuit iff POLLHUP is set, the user is still interested in read
   * events and uv__read() reported a partial read but not EOF. If the EOF
   * flag is set, uv__read() called read_cb with err=UV_EOF and we don't
   * have to do anything. If the partial read flag is not set, we can't
   * report the EOF yet because there is still data to read.
   */
  if ((events & POLLHUP) &&
      (stream->flags & UV_STREAM_READING) &&
      (stream->flags & UV_STREAM_READ_PARTIAL) &&
      !(stream->flags & UV_STREAM_READ_EOF)) {
    uv_buf_t buf = { NULL, 0 };
    uv__stream_eof(stream, &buf);
  }

  if (uv__stream_fd(stream) == -1)
    return;  /* read_cb closed stream. */

  if (events & (POLLOUT | POLLERR | POLLHUP)) {
    uv__write(stream);
    uv__write_callbacks(stream);

    /* Write queue drained. */
    if (QUEUE_EMPTY(&stream->write_queue))
      uv__drain(stream);
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
    assert(uv__stream_fd(stream) >= 0);
    getsockopt(uv__stream_fd(stream),
               SOL_SOCKET,
               SO_ERROR,
               &error,
               &errorsize);
    error = -error;
  }

  if (error == -EINPROGRESS)
    return;

  stream->connect_req = NULL;
  uv__req_unregister(stream->loop, req);

  if (error < 0 || QUEUE_EMPTY(&stream->write_queue)) {
    uv__io_stop(stream->loop, &stream->io_watcher, POLLOUT);
  }

  if (req->cb)
    req->cb(req, error);

  if (uv__stream_fd(stream) == -1)
    return;

  if (error < 0) {
    uv__stream_flush_write_queue(stream, -ECANCELED);
    uv__write_callbacks(stream);
  }
}


int uv_write2(uv_write_t* req,
              uv_stream_t* stream,
              const uv_buf_t bufs[],
              unsigned int nbufs,
              uv_stream_t* send_handle,
              uv_write_cb cb) {
  int empty_queue;

  assert(nbufs > 0);
  assert((stream->type == UV_TCP ||
          stream->type == UV_NAMED_PIPE ||
          stream->type == UV_TTY) &&
         "uv_write (unix) does not yet support other types of streams");

  if (uv__stream_fd(stream) < 0)
    return -EBADF;

  if (send_handle) {
    if (stream->type != UV_NAMED_PIPE || !((uv_pipe_t*)stream)->ipc)
      return -EINVAL;

    /* XXX We abuse uv_write2() to send over UDP handles to child processes.
     * Don't call uv__stream_fd() on those handles, it's a macro that on OS X
     * evaluates to a function that operates on a uv_stream_t with a couple of
     * OS X specific fields. On other Unices it does (handle)->io_watcher.fd,
     * which works but only by accident.
     */
    if (uv__handle_fd((uv_handle_t*) send_handle) < 0)
      return -EBADF;

#if defined(__CYGWIN__) || defined(__MSYS__)
    /* Cygwin recvmsg always sets msg_controllen to zero, so we cannot send it.
       See https://github.com/mirror/newlib-cygwin/blob/86fc4bf0/winsup/cygwin/fhandler_socket.cc#L1736-L1743 */
    return -ENOSYS;
#endif
  }

  /* It's legal for write_queue_size > 0 even when the write_queue is empty;
   * it means there are error-state requests in the write_completed_queue that
   * will touch up write_queue_size later, see also uv__write_req_finish().
   * We could check that write_queue is empty instead but that implies making
   * a write() syscall when we know that the handle is in error mode.
   */
  empty_queue = (stream->write_queue_size == 0);

  /* Initialize the req */
  uv__req_init(stream->loop, req, UV_WRITE);
  req->cb = cb;
  req->handle = stream;
  req->error = 0;
  req->send_handle = send_handle;
  QUEUE_INIT(&req->queue);

  req->bufs = req->bufsml;
  if (nbufs > ARRAY_SIZE(req->bufsml))
    req->bufs = uv__malloc(nbufs * sizeof(bufs[0]));

  if (req->bufs == NULL)
    return -ENOMEM;

  memcpy(req->bufs, bufs, nbufs * sizeof(bufs[0]));
  req->nbufs = nbufs;
  req->write_index = 0;
  stream->write_queue_size += uv__count_bufs(bufs, nbufs);

  /* Append the request to write_queue. */
  QUEUE_INSERT_TAIL(&stream->write_queue, &req->queue);

  /* If the queue was empty when this function began, we should attempt to
   * do the write immediately. Otherwise start the write_watcher and wait
   * for the fd to become writable.
   */
  if (stream->connect_req) {
    /* Still connecting, do nothing. */
  }
  else if (empty_queue) {
    uv__write(stream);
  }
  else {
    /*
     * blocking streams should never have anything in the queue.
     * if this assert fires then somehow the blocking stream isn't being
     * sufficiently flushed in uv__write.
     */
    assert(!(stream->flags & UV_STREAM_BLOCKING));
    uv__io_start(stream->loop, &stream->io_watcher, POLLOUT);
    uv__stream_osx_interrupt_select(stream);
  }

  return 0;
}


/* The buffers to be written must remain valid until the callback is called.
 * This is not required for the uv_buf_t array.
 */
int uv_write(uv_write_t* req,
             uv_stream_t* handle,
             const uv_buf_t bufs[],
             unsigned int nbufs,
             uv_write_cb cb) {
  return uv_write2(req, handle, bufs, nbufs, NULL, cb);
}


void uv_try_write_cb(uv_write_t* req, int status) {
  /* Should not be called */
  abort();
}


int uv_try_write(uv_stream_t* stream,
                 const uv_buf_t bufs[],
                 unsigned int nbufs) {
  int r;
  int has_pollout;
  size_t written;
  size_t req_size;
  uv_write_t req;

  /* Connecting or already writing some data */
  if (stream->connect_req != NULL || stream->write_queue_size != 0)
    return -EAGAIN;

  has_pollout = uv__io_active(&stream->io_watcher, POLLOUT);

  r = uv_write(&req, stream, bufs, nbufs, uv_try_write_cb);
  if (r != 0)
    return r;

  /* Remove not written bytes from write queue size */
  written = uv__count_bufs(bufs, nbufs);
  if (req.bufs != NULL)
    req_size = uv__write_req_size(&req);
  else
    req_size = 0;
  written -= req_size;
  stream->write_queue_size -= req_size;

  /* Unqueue request, regardless of immediateness */
  QUEUE_REMOVE(&req.queue);
  uv__req_unregister(stream->loop, &req);
  if (req.bufs != req.bufsml)
    uv__free(req.bufs);
  req.bufs = NULL;

  /* Do not poll for writable, if we wasn't before calling this */
  if (!has_pollout) {
    uv__io_stop(stream->loop, &stream->io_watcher, POLLOUT);
    uv__stream_osx_interrupt_select(stream);
  }

  if (written == 0 && req_size != 0)
    return -EAGAIN;
  else
    return written;
}


int uv_read_start(uv_stream_t* stream,
                  uv_alloc_cb alloc_cb,
                  uv_read_cb read_cb) {
  assert(stream->type == UV_TCP || stream->type == UV_NAMED_PIPE ||
      stream->type == UV_TTY);

  if (stream->flags & UV_CLOSING)
    return -EINVAL;

  /* The UV_STREAM_READING flag is irrelevant of the state of the tcp - it just
   * expresses the desired state of the user.
   */
  stream->flags |= UV_STREAM_READING;

  /* TODO: try to do the read inline? */
  /* TODO: keep track of tcp state. If we've gotten a EOF then we should
   * not start the IO watcher.
   */
  assert(uv__stream_fd(stream) >= 0);
  assert(alloc_cb);

  stream->read_cb = read_cb;
  stream->alloc_cb = alloc_cb;

  uv__io_start(stream->loop, &stream->io_watcher, POLLIN);
  uv__handle_start(stream);
  uv__stream_osx_interrupt_select(stream);

  return 0;
}


int uv_read_stop(uv_stream_t* stream) {
  if (!(stream->flags & UV_STREAM_READING))
    return 0;

  stream->flags &= ~UV_STREAM_READING;
  uv__io_stop(stream->loop, &stream->io_watcher, POLLIN);
  if (!uv__io_active(&stream->io_watcher, POLLOUT))
    uv__handle_stop(stream);
  uv__stream_osx_interrupt_select(stream);

  stream->read_cb = NULL;
  stream->alloc_cb = NULL;
  return 0;
}


int uv_is_readable(const uv_stream_t* stream) {
  return !!(stream->flags & UV_STREAM_READABLE);
}


int uv_is_writable(const uv_stream_t* stream) {
  return !!(stream->flags & UV_STREAM_WRITABLE);
}


#if defined(__APPLE__)
int uv___stream_fd(const uv_stream_t* handle) {
  const uv__stream_select_t* s;

  assert(handle->type == UV_TCP ||
         handle->type == UV_TTY ||
         handle->type == UV_NAMED_PIPE);

  s = handle->select;
  if (s != NULL)
    return s->fd;

  return handle->io_watcher.fd;
}
#endif /* defined(__APPLE__) */


void uv__stream_close(uv_stream_t* handle) {
  unsigned int i;
  uv__stream_queued_fds_t* queued_fds;

#if defined(__APPLE__)
  /* Terminate select loop first */
  if (handle->select != NULL) {
    uv__stream_select_t* s;

    s = handle->select;

    uv_sem_post(&s->close_sem);
    uv_sem_post(&s->async_sem);
    uv__stream_osx_interrupt_select(handle);
    uv_thread_join(&s->thread);
    uv_sem_destroy(&s->close_sem);
    uv_sem_destroy(&s->async_sem);
    uv__close(s->fake_fd);
    uv__close(s->int_fd);
    uv_close((uv_handle_t*) &s->async, uv__stream_osx_cb_close);

    handle->select = NULL;
  }
#endif /* defined(__APPLE__) */

  uv__io_close(handle->loop, &handle->io_watcher);
  uv_read_stop(handle);
  uv__handle_stop(handle);

  if (handle->io_watcher.fd != -1) {
    /* Don't close stdio file descriptors.  Nothing good comes from it. */
    if (handle->io_watcher.fd > STDERR_FILENO)
      uv__close(handle->io_watcher.fd);
    handle->io_watcher.fd = -1;
  }

  if (handle->accepted_fd != -1) {
    uv__close(handle->accepted_fd);
    handle->accepted_fd = -1;
  }

  /* Close all queued fds */
  if (handle->queued_fds != NULL) {
    queued_fds = handle->queued_fds;
    for (i = 0; i < queued_fds->offset; i++)
      uv__close(queued_fds->fds[i]);
    uv__free(handle->queued_fds);
    handle->queued_fds = NULL;
  }

  assert(!uv__io_active(&handle->io_watcher, POLLIN | POLLOUT));
}


int uv_stream_set_blocking(uv_stream_t* handle, int blocking) {
  /* Don't need to check the file descriptor, uv__nonblock()
   * will fail with EBADF if it's not valid.
   */
  return uv__nonblock(uv__stream_fd(handle), !blocking);
}
