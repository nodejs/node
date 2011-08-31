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
#include <string.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>

int uv_pipe_init(uv_loop_t* loop, uv_pipe_t* handle) {
  memset(handle, 0, sizeof *handle);

  uv__handle_init(loop, (uv_handle_t*)handle, UV_NAMED_PIPE);
  loop->counters.pipe_init++;

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
    uv_err_new_artificial(handle->loop, UV_EINVAL);
    goto out;
  }

  /* Make a copy of the file name, it outlives this function's scope. */
  if ((pipe_fname = strdup(name)) == NULL) {
    uv_err_new(handle->loop, ENOMEM);
    goto out;
  }

  /* We've got a copy, don't touch the original any more. */
  name = NULL;

  if ((sockfd = uv__socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    uv_err_new(handle->loop, errno);
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
      uv_err_new(handle->loop, (errno == ENOENT) ? EACCES : errno);
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


int uv_pipe_listen(uv_pipe_t* handle, int backlog, uv_connection_cb cb) {
  int saved_errno;
  int status;

  saved_errno = errno;
  status = -1;

  if (handle->fd == -1) {
    uv_err_new_artificial(handle->loop, UV_EINVAL);
    goto out;
  }
  assert(handle->fd >= 0);

  if ((status = listen(handle->fd, backlog)) == -1) {
    uv_err_new(handle->loop, errno);
  } else {
    handle->connection_cb = cb;
    ev_io_init(&handle->read_watcher, uv__pipe_accept, handle->fd, EV_READ);
    ev_io_start(handle->loop->ev, &handle->read_watcher);
  }

out:
  errno = saved_errno;
  return status;
}


int uv_pipe_cleanup(uv_pipe_t* handle) {
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
    uv_err_new(handle->loop, errno);
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
    uv_err_new(handle->loop, errno);
    uv__close(sockfd);
    goto out;
  }

  uv__stream_open((uv_stream_t*)handle, sockfd, UV_READABLE | UV_WRITABLE);

  ev_io_start(handle->loop->ev, &handle->read_watcher);
  ev_io_start(handle->loop->ev, &handle->write_watcher);

  status = 0;

out:
  handle->delayed_error = status; /* Passed to callback. */
  handle->connect_req = req;
  req->handle = (uv_stream_t*)handle;
  req->type = UV_CONNECT;
  req->cb = cb;
  ngx_queue_init(&req->queue);

  /* Run callback on next tick. */
  ev_feed_event(handle->loop->ev, &handle->read_watcher, EV_CUSTOM);
  assert(ev_is_pending(&handle->read_watcher));

  /* Mimic the Windows pipe implementation, always
   * return 0 and let the callback handle errors.
   */
  errno = saved_errno;
  return 0;
}


/* TODO merge with uv__server_io()? */
void uv__pipe_accept(EV_P_ ev_io* watcher, int revents) {
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
      uv_err_new(pipe->loop, errno);
    }
  } else {
    pipe->accepted_fd = sockfd;
    pipe->connection_cb((uv_stream_t*)pipe, 0);
    if (pipe->accepted_fd == sockfd) {
      /* The user hasn't yet accepted called uv_accept() */
      ev_io_stop(pipe->loop->ev, &pipe->read_watcher);
    }
  }

  errno = saved_errno;
}
