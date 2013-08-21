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

static uv_async_t async_handle;
static uv_check_t check_handle;
static int check_cb_called;
static uv_thread_t thread;


static void thread_cb(void* dummy) {
  (void) &dummy;
  uv_async_send(&async_handle);
}


static void check_cb(uv_check_t* handle, int status) {
  ASSERT(check_cb_called == 0);
  uv_close((uv_handle_t*) &async_handle, NULL);
  uv_close((uv_handle_t*) &check_handle, NULL);
  check_cb_called++;
}


TEST_IMPL(async_null_cb) {
  ASSERT(0 == uv_async_init(uv_default_loop(), &async_handle, NULL));
  ASSERT(0 == uv_check_init(uv_default_loop(), &check_handle));
  ASSERT(0 == uv_check_start(&check_handle, check_cb));
  ASSERT(0 == uv_thread_create(&thread, thread_cb, NULL));
  ASSERT(0 == uv_run(uv_default_loop(), UV_RUN_DEFAULT));
  ASSERT(0 == uv_thread_join(&thread));
  ASSERT(1 == check_cb_called);
  MAKE_VALGRIND_HAPPY();
  return 0;
}
