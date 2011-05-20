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


static int expected = 0;
static int timeouts = 0;

static int64_t start_time;

static void timeout_cb(uv_req_t *req, int64_t skew, int status) {
  ASSERT(req != NULL);
  ASSERT(status == 0);

  free(req);
  timeouts++;

  /* Just call this randomly for the code coverage. */
  uv_update_time();
}

static void exit_timeout_cb(uv_req_t *req, int64_t skew, int status) {
  int64_t now = uv_now();
  ASSERT(req != NULL);
  ASSERT(status == 0);
  ASSERT(timeouts == expected);
  ASSERT(start_time < now);
  exit(0);
}

static void dummy_timeout_cb(uv_req_t *req, int64_t skew, int status) {
  /* Should never be called */
  FATAL("dummy_timer_cb should never be called");
}


static uv_buf alloc_cb(uv_handle_t* handle, size_t size) {
  uv_buf buf = {0, 0};
  FATAL("alloc should not be called");
  return buf;
}


TEST_IMPL(timeout) {
  uv_req_t *req;
  uv_req_t exit_req;
  uv_req_t dummy_req;
  int i;

  uv_init(alloc_cb);

  start_time = uv_now();
  ASSERT(0 < start_time);

  /* Let 10 timers time out in 500 ms total. */
  for (i = 0; i < 10; i++) {
    req = (uv_req_t*)malloc(sizeof(*req));
    ASSERT(req != NULL);

    uv_req_init(req, NULL, timeout_cb);

    if (uv_timeout(req, i * 50) < 0) {
      FATAL("uv_timeout failed");
    }

    expected++;
  }

  /* The 11th timer exits the test and runs after 1 s. */
  uv_req_init(&exit_req, NULL, exit_timeout_cb);
  if (uv_timeout(&exit_req, 1000) < 0) {
    FATAL("uv_timeout failed");
  }

  /* The 12th timer should never run. */
  uv_req_init(&dummy_req, NULL, dummy_timeout_cb);
  if (uv_timeout(&dummy_req, 2000)) {
    FATAL("uv_timeout failed");
  }

  uv_run();

  FATAL("should never get here");
  return 2;
}
