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

#include <unistd.h>
#include <assert.h>
#include <errno.h>


static void uv__poll_io(uv_loop_t* loop, uv__io_t* w, unsigned int events) {
  uv_poll_t* handle;
  int pevents;

  handle = container_of(w, uv_poll_t, io_watcher);

  if (events & UV__POLLERR) {
    uv__io_stop(loop, w, UV__POLLIN | UV__POLLOUT | UV__POLLRDHUP);
    uv__handle_stop(handle);
    handle->poll_cb(handle, -EBADF, 0);
    return;
  }

  pevents = 0;
  if (events & UV__POLLIN)
    pevents |= UV_READABLE;
  if (events & UV__POLLOUT)
    pevents |= UV_WRITABLE;
  if (events & UV__POLLRDHUP)
    pevents |= UV_DISCONNECT;

  handle->poll_cb(handle, 0, pevents);
}


int uv_poll_init(uv_loop_t* loop, uv_poll_t* handle, int fd) {
  int err;

  err = uv__io_check_fd(loop, fd);
  if (err)
    return err;

  err = uv__nonblock(fd, 1);
  if (err)
    return err;

  uv__handle_init(loop, (uv_handle_t*) handle, UV_POLL);
  uv__io_init(&handle->io_watcher, uv__poll_io, fd);
  handle->poll_cb = NULL;
  return 0;
}


int uv_poll_init_socket(uv_loop_t* loop, uv_poll_t* handle,
    uv_os_sock_t socket) {
  return uv_poll_init(loop, handle, socket);
}


static void uv__poll_stop(uv_poll_t* handle) {
  uv__io_stop(handle->loop,
              &handle->io_watcher,
              UV__POLLIN | UV__POLLOUT | UV__POLLRDHUP);
  uv__handle_stop(handle);
}


int uv_poll_stop(uv_poll_t* handle) {
  assert(!(handle->flags & (UV_CLOSING | UV_CLOSED)));
  uv__poll_stop(handle);
  return 0;
}


int uv_poll_start(uv_poll_t* handle, int pevents, uv_poll_cb poll_cb) {
  int events;

  assert((pevents & ~(UV_READABLE | UV_WRITABLE | UV_DISCONNECT)) == 0);
  assert(!(handle->flags & (UV_CLOSING | UV_CLOSED)));

  uv__poll_stop(handle);

  if (pevents == 0)
    return 0;

  events = 0;
  if (pevents & UV_READABLE)
    events |= UV__POLLIN;
  if (pevents & UV_WRITABLE)
    events |= UV__POLLOUT;
  if (pevents & UV_DISCONNECT)
    events |= UV__POLLRDHUP;

  uv__io_start(handle->loop, &handle->io_watcher, events);
  uv__handle_start(handle);
  handle->poll_cb = poll_cb;

  return 0;
}


void uv__poll_close(uv_poll_t* handle) {
  uv__poll_stop(handle);
}
