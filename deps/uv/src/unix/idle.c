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


static void uv__idle(EV_P_ ev_idle* w, int revents) {
  uv_idle_t* idle = container_of(w, uv_idle_t, idle_watcher);

  if (idle->idle_cb) {
    idle->idle_cb(idle, 0);
  }
}


int uv_idle_init(uv_loop_t* loop, uv_idle_t* idle) {
  uv__handle_init(loop, (uv_handle_t*)idle, UV_IDLE);
  loop->counters.idle_init++;

  ev_idle_init(&idle->idle_watcher, uv__idle);
  idle->idle_cb = NULL;

  return 0;
}


int uv_idle_start(uv_idle_t* idle, uv_idle_cb cb) {
  int was_active = ev_is_active(&idle->idle_watcher);

  idle->idle_cb = cb;
  ev_idle_start(idle->loop->ev, &idle->idle_watcher);

  if (!was_active) {
    ev_unref(idle->loop->ev);
  }

  return 0;
}


int uv_idle_stop(uv_idle_t* idle) {
  int was_active = ev_is_active(&idle->idle_watcher);

  ev_idle_stop(idle->loop->ev, &idle->idle_watcher);

  if (was_active) {
    ev_ref(idle->loop->ev);
  }

  return 0;
}


int uv__idle_active(const uv_idle_t* handle) {
  return ev_is_active(&handle->idle_watcher);
}


void uv__idle_close(uv_idle_t* handle) {
  uv_idle_stop(handle);
}
