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

#define NUM_TIMERS (1000 * 1000)

static int timer_cb_called;
static int close_cb_called;


static void timer_cb(uv_timer_t* handle, int status) {
  timer_cb_called++;
}


static void close_cb(uv_handle_t* handle) {
  close_cb_called++;
}


BENCHMARK_IMPL(million_timers) {
  uv_timer_t* timers;
  uv_loop_t* loop;
  uint64_t before;
  uint64_t after;
  int timeout;
  int i;

  timers = malloc(NUM_TIMERS * sizeof(timers[0]));
  ASSERT(timers != NULL);

  loop = uv_default_loop();
  timeout = 0;

  for (i = 0; i < NUM_TIMERS; i++) {
    if (i % 1000 == 0) timeout++;
    ASSERT(0 == uv_timer_init(loop, timers + i));
    ASSERT(0 == uv_timer_start(timers + i, timer_cb, timeout, 0));
  }

  before = uv_hrtime();
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  after = uv_hrtime();

  for (i = 0; i < NUM_TIMERS; i++)
    uv_close((uv_handle_t*) (timers + i), close_cb);

  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(timer_cb_called == NUM_TIMERS);
  ASSERT(close_cb_called == NUM_TIMERS);
  free(timers);

  LOGF("%.2f seconds\n", (after - before) / 1e9);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
