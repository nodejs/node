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

static void uv__pipe_accept(uv_loop_t* loop, uv__io_t* w, unsigned int events);


int uv_pipe_init(uv_loop_t* loop, uv_pipe_t* handle, int ipc) {
  uv__stream_init(loop, (uv_stream_t*)handle, UV_NAMED_PIPE);
  handle->shutdown_req = NULL;
  handle->connect_req = NULL;
  handle->pipe_fname = NULL;
  handle->ipc = ipc;
  return 0;
}


int uv_pipe_bind(uv_pipe_t* handle, const char* name) {
  struct sockaddr_un saddr;
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
  if (uv__stream_fd(handle) >= 0) {
    uv__set_artificial_error(handle->loop, UV_EINVAL);
    goto out;
  }

  /* Make a copy of the file name, it outlives this function's scope. */
  if ((pipe_fname = strdup(name)) == NULL) {
    uv__set_sys_error(handle->loop, ENOMEM);
    goto out;
  }

  /* We've got a copy, don't touch the original any more. */
  name = NULL;

  if ((sockfd = uv__socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
    uv__set_sys_error(handle->loop, errno);
    goto out;
  }

  memset(&saddr, 0, sizeof saddr);
  uv_strlcpy(saddr.sun_path, pipe_fname, sizeof(saddr.sun_path));
  saddr.sun_family = AF_UNIX;

  if (bind(sockfd, (struct sockaddr*)&saddr, sizeof saddr)) {
    /* Convert ENOENT to EACCES for compatibility with Windows. */
    uv__set_sys_error(handle->loop, (errno == ENOENT) ? EACCES : errno);
    goto out;
  }
  bound = 1;

  /* Success. */
  handle->pipe_fname = pipe_fname; /* Is a strdup'ed copy. */
  handle->io_watcher.fd = sockfd;
  status = 0;

out:
  /* Clean up on error. */
  if (status) {
    if (bound) {
      /* unlink() before close() to avoid races. */
      assert(pipe_fname != NULL);
      unlink(pipe_fname);
    }
    close(sockfd);

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

  if (uv__stream_fd(handle) == -1) {
    uv__set_artificial_error(handle->loop, UV_EINVAL);
    goto out;
  }
  assert(uv__stream_fd(handle) >= 0);

  if ((status = listen(uv__stream_fd(handle), backlog)) == -1) {
    uv__set_sys_error(handle->loop, errno);
  } else {
    handle->connection_cb = cb;
    handle->io_watcher.cb = uv__pipe_accept;
    uv__io_start(handle->loop, &handle->io_watcher, UV__POLLIN);
  }

out:
  errno = saved_errno;
  return status;
}


void uv__pipe_close(uv_pipe_t* handle) {
  if (handle->pipe_fname) {
    /*
     * Unlink the file system entity before closing the file descriptor.
     * Doing it the other way around introduces a race where our process
     * unlinks a socket with the same name that's just been created by
     * another thread or process.
     */
    unlink(handle->pipe_fname);
    free((void*)handle->pipe_fname);
    handle->pipe_fname = NULL;
  }

  uv__stream_close((uv_stream_t*)handle);
}


int uv_pipe_open(uv_pipe_t* handle, uv_file fd) {
#if defined(__APPLE__)
  if (uv__stream_try_select((uv_stream_t*) handle, &fd))
    return -1;
#endif /* defined(__APPLE__) */

  return uv__stream_open((uv_stream_t*)handle,
                         fd,
                         UV_STREAM_READABLE | UV_STREAM_WRITABLE);
}


void uv_pipe_connect(uv_connect_t* req,
                    uv_pipe_t* handle,
                    const char* name,
                    uv_connect_cb cb) {
  struct sockaddr_un saddr;
  int saved_errno;
  int new_sock;
  int err;
  int r;

  saved_errno = errno;
  new_sock = (uv__stream_fd(handle) == -1);
  err = -1;

  if (new_sock)
    if ((handle->io_watcher.fd = uv__socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
      goto out;

  memset(&saddr, 0, sizeof saddr);
  uv_strlcpy(saddr.sun_path, name, sizeof(saddr.sun_path));
  saddr.sun_family = AF_UNIX;

  do {
    r = connect(uv__stream_fd(handle),
                (struct sockaddr*)&saddr, sizeof saddr);
  }
  while (r == -1 && errno == EINTR);

  if (r == -1)
    if (errno != EINPROGRESS)
      goto out;

  if (new_sock)
    if (uv__stream_open((uv_stream_t*)handle,
                        uv__stream_fd(handle),
                        UV_STREAM_READABLE | UV_STREAM_WRITABLE))
      goto out;

  uv__io_start(handle->loop, &handle->io_watcher, UV__POLLIN | UV__POLLOUT);
  err = 0;

out:
  handle->delayed_error = err ? errno : 0; /* Passed to callback. */
  handle->connect_req = req;

  uv__req_init(handle->loop, req, UV_CONNECT);
  req->handle = (uv_stream_t*)handle;
  req->cb = cb;
  ngx_queue_init(&req->queue);

  /* Force callback to run on next tick in case of error. */
  if (err != 0)
    uv__io_feed(handle->loop, &handle->io_watcher);

  /* Mimic the Windows pipe implementation, always
   * return 0 and let the callback handle errors.
   */
  errno = saved_errno;
}


/* TODO merge with uv__server_io()? */
static void uv__pipe_accept(uv_loop_t* loop, uv__io_t* w, unsigned int events) {
  uv_pipe_t* pipe;
  int saved_errno;
  int sockfd;

  saved_errno = errno;
  pipe = container_of(w, uv_pipe_t, io_watcher);

  assert(pipe->type == UV_NAMED_PIPE);

  sockfd = uv__accept(uv__stream_fd(pipe));
  if (sockfd == -1) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      uv__set_sys_error(pipe->loop, errno);
      pipe->connection_cb((uv_stream_t*)pipe, -1);
    }
  } else {
    pipe->accepted_fd = sockfd;
    pipe->connection_cb((uv_stream_t*)pipe, 0);
    if (pipe->accepted_fd == sockfd) {
      /* The user hasn't called uv_accept() yet */
      uv__io_stop(pipe->loop, &pipe->io_watcher, UV__POLLIN);
    }
  }

  errno = saved_errno;
}


void uv_pipe_pending_instances(uv_pipe_t* handle, int count) {
}
