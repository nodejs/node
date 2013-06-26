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


int uv_accept(uv_stream_t* server, uv_stream_t* client) {
  switch (server->type) {
    case UV_TCP:
      return uv_tcp_accept((uv_tcp_t*)server, (uv_tcp_t*)client);
    case UV_NAMED_PIPE:
      return uv_pipe_accept((uv_pipe_t*)server, client);
    default:
      assert(0);
      return -1;
  }
}


int uv_read_start(uv_stream_t* handle, uv_alloc_cb alloc_cb,
    uv_read_cb read_cb) {
  if (handle->flags & UV_HANDLE_READING) {
    uv__set_artificial_error(handle->loop, UV_EALREADY);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_READABLE)) {
    uv__set_artificial_error(handle->loop, UV_ENOTCONN);
    return -1;
  }

  switch (handle->type) {
    case UV_TCP:
      return uv_tcp_read_start((uv_tcp_t*)handle, alloc_cb, read_cb);
    case UV_NAMED_PIPE:
      return uv_pipe_read_start((uv_pipe_t*)handle, alloc_cb, read_cb);
    case UV_TTY:
      return uv_tty_read_start((uv_tty_t*) handle, alloc_cb, read_cb);
    default:
      assert(0);
      return -1;
  }
}


int uv_read2_start(uv_stream_t* handle, uv_alloc_cb alloc_cb,
    uv_read2_cb read_cb) {
  if (handle->flags & UV_HANDLE_READING) {
    uv__set_artificial_error(handle->loop, UV_EALREADY);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_READABLE)) {
    uv__set_artificial_error(handle->loop, UV_ENOTCONN);
    return -1;
  }

  switch (handle->type) {
    case UV_NAMED_PIPE:
      return uv_pipe_read2_start((uv_pipe_t*)handle, alloc_cb, read_cb);
    default:
      assert(0);
      return -1;
  }
}


int uv_read_stop(uv_stream_t* handle) {
  if (!(handle->flags & UV_HANDLE_READING))
    return 0;

  if (handle->type == UV_TTY) {
    return uv_tty_read_stop((uv_tty_t*) handle);
  } else {
    handle->flags &= ~UV_HANDLE_READING;
    DECREASE_ACTIVE_COUNT(handle->loop, handle);
    return 0;
  }
}


int uv_write(uv_write_t* req, uv_stream_t* handle, uv_buf_t bufs[], int bufcnt,
    uv_write_cb cb) {
  uv_loop_t* loop = handle->loop;

  if (!(handle->flags & UV_HANDLE_WRITABLE)) {
    uv__set_artificial_error(loop, UV_EPIPE);
    return -1;
  }

  switch (handle->type) {
    case UV_TCP:
      return uv_tcp_write(loop, req, (uv_tcp_t*) handle, bufs, bufcnt, cb);
    case UV_NAMED_PIPE:
      return uv_pipe_write(loop, req, (uv_pipe_t*) handle, bufs, bufcnt, cb);
    case UV_TTY:
      return uv_tty_write(loop, req, (uv_tty_t*) handle, bufs, bufcnt, cb);
    default:
      assert(0);
      uv__set_sys_error(loop, WSAEINVAL);
      return -1;
  }
}


int uv_write2(uv_write_t* req, uv_stream_t* handle, uv_buf_t bufs[], int bufcnt,
    uv_stream_t* send_handle, uv_write_cb cb) {
  uv_loop_t* loop = handle->loop;

  if (!(handle->flags & UV_HANDLE_WRITABLE)) {
    uv__set_artificial_error(loop, UV_EPIPE);
    return -1;
  }

  switch (handle->type) {
    case UV_NAMED_PIPE:
      return uv_pipe_write2(loop, req, (uv_pipe_t*) handle, bufs, bufcnt, send_handle, cb);
    default:
      assert(0);
      uv__set_sys_error(loop, WSAEINVAL);
      return -1;
  }
}


int uv_shutdown(uv_shutdown_t* req, uv_stream_t* handle, uv_shutdown_cb cb) {
  uv_loop_t* loop = handle->loop;

 if (!(handle->flags & UV_HANDLE_WRITABLE)) {
    uv__set_artificial_error(loop, UV_EPIPE);
    return -1;
  }

  if (!(handle->flags & UV_HANDLE_WRITABLE)) {
    uv__set_artificial_error(loop, UV_EPIPE);
    return -1;
  }

  uv_req_init(loop, (uv_req_t*) req);
  req->type = UV_SHUTDOWN;
  req->handle = handle;
  req->cb = cb;

  handle->flags &= ~UV_HANDLE_WRITABLE;
  handle->shutdown_req = req;
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
  if (blocking != 0)
    handle->flags |= UV_HANDLE_BLOCKING_WRITES;
  else
    handle->flags &= ~UV_HANDLE_BLOCKING_WRITES;

  return 0;
}
