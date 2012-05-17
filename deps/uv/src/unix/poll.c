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


static void uv__poll_io(EV_P_ ev_io* watcher, int ev_events) {
  uv_poll_t* handle = watcher->data;
  int events;

  if (ev_events & EV_ERROR) {
    /* An error happened. Libev has implicitly stopped the watcher, but we */
    /* need to fix the refcount. */
    uv__handle_stop(handle);
    uv__set_sys_error(handle->loop, EBADF);
    handle->poll_cb(handle, -1, 0);
    return;
  }

  assert(ev_events & (EV_READ | EV_WRITE));
  assert((ev_events & ~(EV_READ | EV_WRITE)) == 0);

  events = 0;
  if (ev_events & EV_READ)
    events |= UV_READABLE;
  if (ev_events & EV_WRITE)
    events |= UV_WRITABLE;

  handle->poll_cb(handle, 0, events);
}


int uv_poll_init(uv_loop_t* loop, uv_poll_t* handle, int fd) {
  uv__handle_init(loop, (uv_handle_t*) handle, UV_POLL);
  loop->counters.poll_init++;

  handle->fd = fd;
  handle->poll_cb = NULL;

  ev_init(&handle->io_watcher, uv__poll_io);
  handle->io_watcher.data = handle;

  return 0;
}


int uv_poll_init_socket(uv_loop_t* loop, uv_poll_t* handle,
    uv_os_sock_t socket) {
  return uv_poll_init(loop, handle, socket);
}


static void uv__poll_stop(uv_poll_t* handle) {
  ev_io_stop(handle->loop->ev, &handle->io_watcher);
  uv__handle_stop(handle);
}


int uv_poll_stop(uv_poll_t* handle) {
  assert(!(handle->flags & (UV_CLOSING | UV_CLOSED)));
  uv__poll_stop(handle);
  return 0;
}


int uv_poll_start(uv_poll_t* handle, int events, uv_poll_cb poll_cb) {
  int ev_events;

  assert((events & ~(UV_READABLE | UV_WRITABLE)) == 0);
  assert(!(handle->flags & (UV_CLOSING | UV_CLOSED)));

  if (events == 0) {
    uv__poll_stop(handle);
    return 0;
  }

  ev_events = 0;
  if (events & UV_READABLE)
    ev_events |= EV_READ;
  if (events & UV_WRITABLE)
    ev_events |= EV_WRITE;

  ev_io_set(&handle->io_watcher, handle->fd, ev_events);
  ev_io_start(handle->loop->ev, &handle->io_watcher);

  handle->poll_cb = poll_cb;
  uv__handle_start(handle);

  return 0;
}


void uv__poll_close(uv_poll_t* handle) {
  uv__poll_stop(handle);
}


int uv__poll_active(const uv_poll_t* handle) {
  return ev_is_active(&handle->io_watcher);
}
