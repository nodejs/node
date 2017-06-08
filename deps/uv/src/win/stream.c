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

#include <assert.h>

#include "uv.h"
#include "internal.h"
#include "handle-inl.h"
#include "req-inl.h"


int uv_listen(uv_stream_t* stream, int backlog, uv_connection_cb cb) {
  int err;

  err = ERROR_INVALID_PARAMETER;
  switch (stream->type) {
    case UV_TCP:
      err = uv_tcp_listen((uv_tcp_t*)stream, backlog, cb);
      break;
    case UV_NAMED_PIPE:
      err = uv_pipe_listen((uv_pipe_t*)stream, backlog, cb);
      break;
    default:
      assert(0);
  }

  return uv_translate_sys_error(err);
}


int uv_accept(uv_stream_t* server, uv_stream_t* client) {
  int err;

  err = ERROR_INVALID_PARAMETER;
  switch (server->type) {
    case UV_TCP:
      err = uv_tcp_accept((uv_tcp_t*)server, (uv_tcp_t*)client);
      break;
    case UV_NAMED_PIPE:
      err = uv_pipe_accept((uv_pipe_t*)server, client);
      break;
    default:
      assert(0);
  }

  return uv_translate_sys_error(err);
}


int uv_read_start(uv_stream_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb) {
  int err;

  if (handle->flags & UV_HANDLE_READING) {
    return UV_EALREADY;
  }

  if (!(handle->flags & UV_HANDLE_READABLE)) {
    return UV_ENOTCONN;
  }

  err = ERROR_INVALID_PARAMETER;
  switch (handle->type) {
    case UV_TCP:
      err = uv_tcp_read_start((uv_tcp_t*)handle, alloc_cb, read_cb);
      break;
    case UV_NAMED_PIPE:
      err = uv_pipe_read_start((uv_pipe_t*)handle, alloc_cb, read_cb);
      break;
    case UV_TTY:
      err = uv_tty_read_start((uv_tty_t*) handle, alloc_cb, read_cb);
      break;
    default:
      assert(0);
  }

  return uv_translate_sys_error(err);
}


int uv_read_stop(uv_stream_t* handle) {
  int err;

  if (!(handle->flags & UV_HANDLE_READING))
    return 0;

  err = 0;
  if (handle->type == UV_TTY) {
    err = uv_tty_read_stop((uv_tty_t*) handle);
  } else {
    if (handle->type == UV_NAMED_PIPE) {
      uv__pipe_stop_read((uv_pipe_t*) handle);
    } else {
      handle->flags &= ~UV_HANDLE_READING;
    }
    DECREASE_ACTIVE_COUNT(handle->loop, handle);
  }

  return uv_translate_sys_error(err);
}


int uv_write(uv_write_t* req,
             uv_stream_t* handle,
             const uv_buf_t bufs[],
             unsigned int nbufs,
             uv_write_cb cb) {
  uv_loop_t* loop = handle->loop;
  int err;

  if (!(handle->flags & UV_HANDLE_WRITABLE)) {
    return UV_EPIPE;
  }

  err = ERROR_INVALID_PARAMETER;
  switch (handle->type) {
    case UV_TCP:
      err = uv_tcp_write(loop, req, (uv_tcp_t*) handle, bufs, nbufs, cb);
      break;
    case UV_NAMED_PIPE:
      err = uv_pipe_write(loop, req, (uv_pipe_t*) handle, bufs, nbufs, cb);
      break;
    case UV_TTY:
      err = uv_tty_write(loop, req, (uv_tty_t*) handle, bufs, nbufs, cb);
      break;
    default:
      assert(0);
  }

  return uv_translate_sys_error(err);
}


int uv_write2(uv_write_t* req,
              uv_stream_t* handle,
              const uv_buf_t bufs[],
              unsigned int nbufs,
              uv_stream_t* send_handle,
              uv_write_cb cb) {
  uv_loop_t* loop = handle->loop;
  int err;

  if (!(handle->flags & UV_HANDLE_WRITABLE)) {
    return UV_EPIPE;
  }

  err = ERROR_INVALID_PARAMETER;
  switch (handle->type) {
    case UV_NAMED_PIPE:
      err = uv_pipe_write2(loop,
                           req,
                           (uv_pipe_t*) handle,
                           bufs,
                           nbufs,
                           send_handle,
                           cb);
      break;
    default:
      assert(0);
  }

  return uv_translate_sys_error(err);
}


int uv_try_write(uv_stream_t* stream,
                 const uv_buf_t bufs[],
                 unsigned int nbufs) {
  if (stream->flags & UV__HANDLE_CLOSING)
    return UV_EBADF;
  if (!(stream->flags & UV_HANDLE_WRITABLE))
    return UV_EPIPE;

  switch (stream->type) {
    case UV_TCP:
      return uv__tcp_try_write((uv_tcp_t*) stream, bufs, nbufs);
    case UV_TTY:
      return uv__tty_try_write((uv_tty_t*) stream, bufs, nbufs);
    case UV_NAMED_PIPE:
      return UV_EAGAIN;
    default:
      assert(0);
      return UV_ENOSYS;
  }
}


int uv_shutdown(uv_shutdown_t* req, uv_stream_t* handle, uv_shutdown_cb cb) {
  uv_loop_t* loop = handle->loop;

  if (!(handle->flags & UV_HANDLE_WRITABLE)) {
    return UV_EPIPE;
  }

  UV_REQ_INIT(req, UV_SHUTDOWN);
  req->handle = handle;
  req->cb = cb;

  handle->flags &= ~UV_HANDLE_WRITABLE;
  handle->stream.conn.shutdown_req = req;
  handle->reqs_pending++;
  REGISTER_HANDLE_REQ(loop, handle, req);

  uv_want_endgame(loop, (uv_handle_t*)handle);

  return 0;
}


int uv_is_readable(const uv_stream_t* handle) {
  return !!(handle->flags & UV_HANDLE_READABLE);
}


int uv_is_writable(const uv_stream_t* handle) {
  return !!(handle->flags & UV_HANDLE_WRITABLE);
}


int uv_stream_set_blocking(uv_stream_t* handle, int blocking) {
  if (handle->type != UV_NAMED_PIPE)
    return UV_EINVAL;

  if (blocking != 0)
    handle->flags |= UV_HANDLE_BLOCKING_WRITES;
  else
    handle->flags &= ~UV_HANDLE_BLOCKING_WRITES;

  return 0;
}
