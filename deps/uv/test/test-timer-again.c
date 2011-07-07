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


static int close_cb_called = 0;
static int repeat_1_cb_called = 0;
static int repeat_2_cb_called = 0;

static int repeat_2_cb_allowed = 0;

static uv_timer_t dummy, repeat_1, repeat_2;

static int64_t start_time;


static void close_cb(uv_handle_t* handle) {
  ASSERT(handle != NULL);

  close_cb_called++;
}


static void repeat_1_cb(uv_timer_t* handle, int status) {
  int r;

  ASSERT(handle == &repeat_1);
  ASSERT(status == 0);

  ASSERT(uv_timer_get_repeat((uv_timer_t*)handle) == 50);

  LOGF("repeat_1_cb called after %ld ms\n", (long int)(uv_now() - start_time));

  repeat_1_cb_called++;

  r = uv_timer_again(&repeat_2);
  ASSERT(r == 0);

  if (uv_now() >= start_time + 500) {
    uv_close((uv_handle_t*)handle, close_cb);
    /* We're not calling uv_timer_again on repeat_2 any more, so after this */
    /* timer_2_cb is expected. */
    repeat_2_cb_allowed = 1;
    return;
  }
}


static void repeat_2_cb(uv_timer_t* handle, int status) {
  ASSERT(handle == &repeat_2);
  ASSERT(status == 0);
  ASSERT(repeat_2_cb_allowed);

  LOGF("repeat_2_cb called after %ld ms\n", (long int)(uv_now() - start_time));

  repeat_2_cb_called++;

  if (uv_timer_get_repeat(&repeat_2) == 0) {
    ASSERT(!uv_is_active((uv_handle_t*)handle));
    uv_close((uv_handle_t*)handle, close_cb);
    return;
  }

  LOGF("uv_timer_get_repeat %ld ms\n",
      (long int)uv_timer_get_repeat(&repeat_2));
  ASSERT(uv_timer_get_repeat(&repeat_2) == 100);

  /* This shouldn't take effect immediately. */
  uv_timer_set_repeat(&repeat_2, 0);
}


TEST_IMPL(timer_again) {
  int r;

  uv_init();

  start_time = uv_now();
  ASSERT(0 < start_time);

  /* Verify that it is not possible to uv_timer_again a never-started timer. */
  r = uv_timer_init(&dummy);
  ASSERT(r == 0);
  r = uv_timer_again(&dummy);
  ASSERT(r == -1);
  ASSERT(uv_last_error().code == UV_EINVAL);
  uv_unref();

  /* Start timer repeat_1. */
  r = uv_timer_init(&repeat_1);
  ASSERT(r == 0);
  r = uv_timer_start(&repeat_1, repeat_1_cb, 50, 0);
  ASSERT(r == 0);
  ASSERT(uv_timer_get_repeat(&repeat_1) == 0);

  /* Actually make repeat_1 repeating. */
  uv_timer_set_repeat(&repeat_1, 50);
  ASSERT(uv_timer_get_repeat(&repeat_1) == 50);

  /*
   * Start another repeating timer. It'll be again()ed by the repeat_1 so
   * it should not time out until repeat_1 stops.
   */
  r = uv_timer_init(&repeat_2);
  ASSERT(r == 0);
  r = uv_timer_start(&repeat_2, repeat_2_cb, 100, 100);
  ASSERT(r == 0);
  ASSERT(uv_timer_get_repeat(&repeat_2) == 100);

  uv_run();

  ASSERT(repeat_1_cb_called == 10);
  ASSERT(repeat_2_cb_called == 2);
  ASSERT(close_cb_called == 2);

  LOGF("Test took %ld ms (expected ~700 ms)\n",
       (long int)(uv_now() - start_time));
  ASSERT(700 <= uv_now() - start_time);

  return 0;
}
