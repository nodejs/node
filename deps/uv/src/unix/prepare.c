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


static void uv__prepare(EV_P_ ev_prepare* w, int revents) {
  uv_prepare_t* prepare = container_of(w, uv_prepare_t, prepare_watcher);

  if (prepare->prepare_cb) {
    prepare->prepare_cb(prepare, 0);
  }
}


int uv_prepare_init(uv_loop_t* loop, uv_prepare_t* prepare) {
  uv__handle_init(loop, (uv_handle_t*)prepare, UV_PREPARE);
  loop->counters.prepare_init++;

  ev_prepare_init(&prepare->prepare_watcher, uv__prepare);
  prepare->prepare_cb = NULL;

  return 0;
}


int uv_prepare_start(uv_prepare_t* prepare, uv_prepare_cb cb) {
  int was_active = ev_is_active(&prepare->prepare_watcher);

  prepare->prepare_cb = cb;

  ev_prepare_start(prepare->loop->ev, &prepare->prepare_watcher);

  if (!was_active) {
    ev_unref(prepare->loop->ev);
  }

  return 0;
}


int uv_prepare_stop(uv_prepare_t* prepare) {
  int was_active = ev_is_active(&prepare->prepare_watcher);

  ev_prepare_stop(prepare->loop->ev, &prepare->prepare_watcher);

  if (was_active) {
    ev_ref(prepare->loop->ev);
  }
  return 0;
}


int uv__prepare_active(const uv_prepare_t* handle) {
  return ev_is_active(&handle->prepare_watcher);
}


void uv__prepare_close(uv_prepare_t* handle) {
  uv_prepare_stop(handle);
}
