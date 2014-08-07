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


TEST_IMPL(loop_update_time) {
  uint64_t start;

  start = uv_now(uv_default_loop());
  while (uv_now(uv_default_loop()) - start < 1000)
    ASSERT(0 == uv_run(uv_default_loop(), UV_RUN_NOWAIT));

  MAKE_VALGRIND_HAPPY();
  return 0;
}

static void cb(uv_timer_t* timer) {
  uv_close((uv_handle_t*)timer, NULL);
}

TEST_IMPL(loop_backend_timeout) {
  uv_loop_t *loop = uv_default_loop();
  uv_timer_t timer;
  int r;

  r = uv_timer_init(loop, &timer);
  ASSERT(r == 0);

  ASSERT(!uv_loop_alive(loop));
  ASSERT(uv_backend_timeout(loop) == 0);

  r = uv_timer_start(&timer, cb, 1000, 0); /* 1 sec */
  ASSERT(r == 0);
  ASSERT(uv_backend_timeout(loop) > 100); /* 0.1 sec */
  ASSERT(uv_backend_timeout(loop) <= 1000); /* 1 sec */

  r = uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(r == 0);
  ASSERT(uv_backend_timeout(loop) == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
