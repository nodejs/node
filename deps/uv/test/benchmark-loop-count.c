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

#include "task.h"
#include "uv.h"

#include <stdio.h>
#include <stdlib.h>

static unsigned long ticks;
static uv_idle_t idle_handle;
static uv_timer_t timer_handle;


static void idle_cb(uv_idle_t* handle, int status) {
  ticks++;
}


static void timer_cb(uv_timer_t* handle, int status) {
  uv_idle_stop(&idle_handle);
  uv_timer_stop(&timer_handle);
}


BENCHMARK_IMPL(loop_count) {
  uv_loop_t* loop = uv_default_loop();

  uv_timer_init(loop, &timer_handle);
  uv_timer_start(&timer_handle, timer_cb, 5000, 0);

  uv_idle_init(loop, &idle_handle);
  uv_idle_start(&idle_handle, idle_cb);

  uv_run(loop);

  LOGF("loop_count: %lu ticks (%.0f ticks/s)\n", ticks, ticks / 5.0);

  return 0;
}
