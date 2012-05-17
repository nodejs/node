/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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


static void uv__async(EV_P_ ev_async* w, int revents) {
  uv_async_t* async = container_of(w, uv_async_t, async_watcher);

  if (async->async_cb) {
    async->async_cb(async, 0);
  }
}


int uv_async_init(uv_loop_t* loop, uv_async_t* async, uv_async_cb async_cb) {
  uv__handle_init(loop, (uv_handle_t*)async, UV_ASYNC);
  loop->counters.async_init++;

  ev_async_init(&async->async_watcher, uv__async);
  async->async_cb = async_cb;

  /* Note: This does not have symmetry with the other libev wrappers. */
  ev_async_start(loop->ev, &async->async_watcher);
  uv__handle_unref(async);
  uv__handle_start(async);

  return 0;
}


int uv_async_send(uv_async_t* async) {
  ev_async_send(async->loop->ev, &async->async_watcher);
  return 0;
}


void uv__async_close(uv_async_t* handle) {
  ev_async_stop(handle->loop->ev, &handle->async_watcher);
  uv__handle_ref(handle);
  uv__handle_stop(handle);
}
