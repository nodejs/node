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

static int uv__timer_cmp(const uv_timer_t* a, const uv_timer_t* b) {
  if (a->timeout < b->timeout)
    return -1;
  if (a->timeout > b->timeout)
    return 1;
  if (a < b)
    return -1;
  if (a > b)
    return 1;
  return 0;
}


RB_GENERATE_STATIC(uv__timers, uv_timer_s, tree_entry, uv__timer_cmp)


int uv_timer_init(uv_loop_t* loop, uv_timer_t* handle) {
  uv__handle_init(loop, (uv_handle_t*)handle, UV_TIMER);
  handle->timer_cb = NULL;

  return 0;
}


int uv_timer_start(uv_timer_t* handle,
                   uv_timer_cb cb,
                   int64_t timeout,
                   int64_t repeat) {
  assert(timeout >= 0);
  assert(repeat >= 0);

  if (uv__is_active(handle))
    uv_timer_stop(handle);

  handle->timer_cb = cb;
  handle->timeout = handle->loop->time + timeout;
  handle->repeat = repeat;

  RB_INSERT(uv__timers, &handle->loop->timer_handles, handle);
  uv__handle_start(handle);

  return 0;
}


int uv_timer_stop(uv_timer_t* handle) {
  if (!uv__is_active(handle))
    return 0;

  RB_REMOVE(uv__timers, &handle->loop->timer_handles, handle);
  uv__handle_stop(handle);

  return 0;
}


int uv_timer_again(uv_timer_t* handle) {
  if (handle->timer_cb == NULL)
    return uv__set_artificial_error(handle->loop, UV_EINVAL);

  if (handle->repeat) {
    uv_timer_stop(handle);
    uv_timer_start(handle, handle->timer_cb, handle->repeat, handle->repeat);
  }

  return 0;
}


void uv_timer_set_repeat(uv_timer_t* handle, int64_t repeat) {
  assert(repeat >= 0);
  handle->repeat = repeat;
}


int64_t uv_timer_get_repeat(uv_timer_t* handle) {
  return handle->repeat;
}


unsigned int uv__next_timeout(uv_loop_t* loop) {
  uv_timer_t* handle;

  handle = RB_MIN(uv__timers, &loop->timer_handles);

  if (handle == NULL)
    return (unsigned int) -1; /* block indefinitely */

  if (handle->timeout <= loop->time)
    return 0;

  return handle->timeout - loop->time;
}


void uv__run_timers(uv_loop_t* loop) {
  uv_timer_t* handle;

  while ((handle = RB_MIN(uv__timers, &loop->timer_handles))) {
    if (handle->timeout > loop->time)
      break;

    uv_timer_stop(handle);
    uv_timer_again(handle);
    handle->timer_cb(handle, 0);
  }
}


void uv__timer_close(uv_timer_t* handle) {
  uv_timer_stop(handle);
}
