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


static void uv__check(EV_P_ ev_check* w, int revents) {
  uv_check_t* check = container_of(w, uv_check_t, check_watcher);

  if (check->check_cb) {
    check->check_cb(check, 0);
  }
}


int uv_check_init(uv_loop_t* loop, uv_check_t* check) {
  uv__handle_init(loop, (uv_handle_t*)check, UV_CHECK);
  loop->counters.check_init++;

  ev_check_init(&check->check_watcher, uv__check);
  check->check_cb = NULL;

  return 0;
}


int uv_check_start(uv_check_t* check, uv_check_cb cb) {
  int was_active = ev_is_active(&check->check_watcher);

  check->check_cb = cb;

  ev_check_start(check->loop->ev, &check->check_watcher);

  if (!was_active) {
    ev_unref(check->loop->ev);
  }

  return 0;
}


int uv_check_stop(uv_check_t* check) {
  int was_active = ev_is_active(&check->check_watcher);

  ev_check_stop(check->loop->ev, &check->check_watcher);

  if (was_active) {
    ev_ref(check->loop->ev);
  }

  return 0;
}


int uv__check_active(const uv_check_t* handle) {
  return ev_is_active(&handle->check_watcher);
}


void uv__check_close(uv_check_t* handle) {
  uv_check_stop(handle);
}
