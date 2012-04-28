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
#include <assert.h>


static int uv__timer_repeating(const uv_timer_t* timer) {
  return timer->flags & UV_TIMER_REPEAT;
}


static void uv__timer_cb(EV_P_ ev_timer* w, int revents) {
  uv_timer_t* timer = container_of(w, uv_timer_t, timer_watcher);

  assert(uv__timer_active(timer));

  if (!uv__timer_repeating(timer)) {
    timer->flags &= ~UV_TIMER_ACTIVE;
    ev_ref(EV_A);
  }

  if (timer->timer_cb) {
    timer->timer_cb(timer, 0);
  }
}


int uv_timer_init(uv_loop_t* loop, uv_timer_t* timer) {
  uv__handle_init(loop, (uv_handle_t*)timer, UV_TIMER);
  loop->counters.timer_init++;

  ev_init(&timer->timer_watcher, uv__timer_cb);

  return 0;
}


int uv_timer_start(uv_timer_t* timer, uv_timer_cb cb, int64_t timeout,
    int64_t repeat) {
  if (uv__timer_active(timer)) {
    return -1;
  }

  timer->timer_cb = cb;
  timer->flags |= UV_TIMER_ACTIVE;

  if (repeat)
    timer->flags |= UV_TIMER_REPEAT;
  else
    timer->flags &= ~UV_TIMER_REPEAT;

  ev_timer_set(&timer->timer_watcher, timeout / 1000.0, repeat / 1000.0);
  ev_timer_start(timer->loop->ev, &timer->timer_watcher);
  ev_unref(timer->loop->ev);

  return 0;
}


int uv_timer_stop(uv_timer_t* timer) {
  if (uv__timer_active(timer)) {
    ev_ref(timer->loop->ev);
  }

  timer->flags &= ~(UV_TIMER_ACTIVE | UV_TIMER_REPEAT);
  ev_timer_stop(timer->loop->ev, &timer->timer_watcher);

  return 0;
}


int uv_timer_again(uv_timer_t* timer) {
  if (!uv__timer_active(timer)) {
    uv__set_artificial_error(timer->loop, UV_EINVAL);
    return -1;
  }

  assert(uv__timer_repeating(timer));
  ev_timer_again(timer->loop->ev, &timer->timer_watcher);
  return 0;
}


void uv_timer_set_repeat(uv_timer_t* timer, int64_t repeat) {
  assert(timer->type == UV_TIMER);
  timer->timer_watcher.repeat = repeat / 1000.0;

  if (repeat)
    timer->flags |= UV_TIMER_REPEAT;
  else
    timer->flags &= ~UV_TIMER_REPEAT;
}


int64_t uv_timer_get_repeat(uv_timer_t* timer) {
  assert(timer->type == UV_TIMER);
  return (int64_t)(1000 * timer->timer_watcher.repeat);
}


int uv__timer_active(const uv_timer_t* timer) {
  return timer->flags & UV_TIMER_ACTIVE;
}


void uv__timer_close(uv_timer_t* handle) {
  uv_timer_stop(handle);
}
