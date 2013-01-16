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
#include "task.h"

static int idle_cb_called;
static int timer_cb_called;

static uv_idle_t idle_handle;
static uv_timer_t timer_handle;


/* idle_cb should run before timer_cb */
static void idle_cb(uv_idle_t* handle, int status) {
  ASSERT(idle_cb_called == 0);
  ASSERT(timer_cb_called == 0);
  uv_idle_stop(handle);
  idle_cb_called++;
}


static void timer_cb(uv_timer_t* handle, int status) {
  ASSERT(idle_cb_called == 1);
  ASSERT(timer_cb_called == 0);
  uv_timer_stop(handle);
  timer_cb_called++;
}


static void next_tick(uv_idle_t* handle, int status) {
  uv_loop_t* loop = handle->loop;
  uv_idle_stop(handle);
  uv_idle_init(loop, &idle_handle);
  uv_idle_start(&idle_handle, idle_cb);
  uv_timer_init(loop, &timer_handle);
  uv_timer_start(&timer_handle, timer_cb, 0, 0);
}


TEST_IMPL(callback_order) {
  uv_loop_t* loop;
  uv_idle_t idle;

  loop = uv_default_loop();
  uv_idle_init(loop, &idle);
  uv_idle_start(&idle, next_tick);

  ASSERT(idle_cb_called == 0);
  ASSERT(timer_cb_called == 0);

  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(idle_cb_called == 1);
  ASSERT(timer_cb_called == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
