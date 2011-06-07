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

#include "../uv.h"
#include "task.h"


static int once_cb_called = 0;
static int once_close_cb_called = 0;
static int repeat_cb_called = 0;
static int repeat_close_cb_called = 0;

static int64_t start_time;


static void once_close_cb(uv_handle_t* handle, int status) {
  printf("ONCE_CLOSE_CB\n");

  ASSERT(handle != NULL);
  ASSERT(status == 0);

  once_close_cb_called++;

  free(handle);
}


static void once_cb(uv_handle_t* handle, int status) {
  printf("ONCE_CB %d\n", once_cb_called);

  ASSERT(handle != NULL);
  ASSERT(status == 0);

  once_cb_called++;

  uv_close(handle);

  /* Just call this randomly for the code coverage. */
  uv_update_time();
}


static void repeat_close_cb(uv_handle_t* handle, int status) {
  printf("REPEAT_CLOSE_CB\n");

  ASSERT(handle != NULL);
  ASSERT(status == 0);

  repeat_close_cb_called++;
}


static void repeat_cb(uv_handle_t* handle, int status) {
  printf("REPEAT_CB\n");

  ASSERT(handle != NULL);
  ASSERT(status == 0);

  repeat_cb_called++;

  if (repeat_cb_called == 5) {
    uv_close(handle);
  }
}


static void never_close_cb(uv_handle_t* handle, int status) {
  FATAL("never_close_cb should never be called");
}


static void never_cb(uv_handle_t* handle, int status) {
  FATAL("never_cb should never be called");
}


TEST_IMPL(timer) {
  uv_timer_t *once;
  uv_timer_t repeat, never;
  int i, r;

  uv_init();

  start_time = uv_now();
  ASSERT(0 < start_time);

  /* Let 10 timers time out in 500 ms total. */
  for (i = 0; i < 10; i++) {
    once = (uv_timer_t*)malloc(sizeof(*once));
    ASSERT(once != NULL);
    r = uv_timer_init(once, once_close_cb, NULL);
    ASSERT(r == 0);
    r = uv_timer_start(once, once_cb, i * 50, 0);
    ASSERT(r == 0);
  }

  /* The 11th timer is a repeating timer that runs 4 times */
  r = uv_timer_init(&repeat, repeat_close_cb, NULL);
  ASSERT(r == 0);
  r = uv_timer_start(&repeat, repeat_cb, 100, 100);
  ASSERT(r == 0);

  /* The 12th timer should not do anything. */
  r = uv_timer_init(&never, never_close_cb, NULL);
  ASSERT(r == 0);
  r = uv_timer_start(&never, never_cb, 100, 100);
  ASSERT(r == 0);
  r = uv_timer_stop(&never);
  ASSERT(r == 0);
  uv_unref();

  uv_run();

  ASSERT(once_cb_called == 10);
  ASSERT(once_close_cb_called == 10);
  printf("repeat_cb_called %d\n", repeat_cb_called);
  ASSERT(repeat_cb_called == 5);
  ASSERT(repeat_close_cb_called == 1);

  ASSERT(500 <= uv_now() - start_time);

  return 0;
}
