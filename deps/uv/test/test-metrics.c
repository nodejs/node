/* Copyright libuv project contributors. All rights reserved.
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
#include <string.h> /* memset */

#define UV_NS_TO_MS 1000000


static void timer_spin_cb(uv_timer_t* handle) {
  uint64_t t;

  (*(int*) handle->data)++;
  t = uv_hrtime();
  /* Spin for 500 ms to spin loop time out of the delta check. */
  while (uv_hrtime() - t < 600 * UV_NS_TO_MS) { }
}


TEST_IMPL(metrics_idle_time) {
  const uint64_t timeout = 1000;
  uv_timer_t timer;
  uint64_t idle_time;
  int cntr;

  cntr = 0;
  timer.data = &cntr;

  ASSERT_EQ(0, uv_loop_configure(uv_default_loop(), UV_METRICS_IDLE_TIME));
  ASSERT_EQ(0, uv_timer_init(uv_default_loop(), &timer));
  ASSERT_EQ(0, uv_timer_start(&timer, timer_spin_cb, timeout, 0));

  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));
  ASSERT_GT(cntr, 0);

  idle_time = uv_metrics_idle_time(uv_default_loop());

  /* Permissive check that the idle time matches within the timeout ±500 ms. */
  ASSERT((idle_time <= (timeout + 500) * UV_NS_TO_MS) &&
         (idle_time >= (timeout - 500) * UV_NS_TO_MS));

  MAKE_VALGRIND_HAPPY();
  return 0;
}


static void metrics_routine_cb(void* arg) {
  const uint64_t timeout = 1000;
  uv_loop_t loop;
  uv_timer_t timer;
  uint64_t idle_time;
  int cntr;

  cntr = 0;
  timer.data = &cntr;

  ASSERT_EQ(0, uv_loop_init(&loop));
  ASSERT_EQ(0, uv_loop_configure(&loop, UV_METRICS_IDLE_TIME));
  ASSERT_EQ(0, uv_timer_init(&loop, &timer));
  ASSERT_EQ(0, uv_timer_start(&timer, timer_spin_cb, timeout, 0));

  ASSERT_EQ(0, uv_run(&loop, UV_RUN_DEFAULT));
  ASSERT_GT(cntr, 0);

  idle_time = uv_metrics_idle_time(&loop);

  /* Only checking that idle time is greater than the lower bound since there
   * may have been thread contention, causing the event loop to be delayed in
   * the idle phase longer than expected.
   */
  ASSERT_GE(idle_time, (timeout - 500) * UV_NS_TO_MS);

  close_loop(&loop);
  ASSERT_EQ(0, uv_loop_close(&loop));
}


TEST_IMPL(metrics_idle_time_thread) {
  uv_thread_t threads[5];
  int i;

  for (i = 0; i < 5; i++) {
    ASSERT_EQ(0, uv_thread_create(&threads[i], metrics_routine_cb, NULL));
  }

  for (i = 0; i < 5; i++) {
    uv_thread_join(&threads[i]);
  }

  return 0;
}


static void timer_noop_cb(uv_timer_t* handle) {
  (*(int*) handle->data)++;
}


TEST_IMPL(metrics_idle_time_zero) {
  uv_timer_t timer;
  int cntr;

  cntr = 0;
  timer.data = &cntr;
  ASSERT_EQ(0, uv_loop_configure(uv_default_loop(), UV_METRICS_IDLE_TIME));
  ASSERT_EQ(0, uv_timer_init(uv_default_loop(), &timer));
  ASSERT_EQ(0, uv_timer_start(&timer, timer_noop_cb, 0, 0));

  ASSERT_EQ(0, uv_run(uv_default_loop(), UV_RUN_DEFAULT));

  ASSERT_GT(cntr, 0);
  ASSERT_EQ(0, uv_metrics_idle_time(uv_default_loop()));

  MAKE_VALGRIND_HAPPY();
  return 0;
}
