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


TEST_IMPL(ref) {
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(idle_ref) {
  uv_idle_t h;
  uv_idle_init(uv_default_loop(), &h);
  uv_idle_start(&h, NULL);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(async_ref) {
  uv_async_t h;
  uv_async_init(uv_default_loop(), &h, NULL);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(prepare_ref) {
  uv_prepare_t h;
  uv_prepare_init(uv_default_loop(), &h);
  uv_prepare_start(&h, NULL);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


TEST_IMPL(check_ref) {
  uv_check_t h;
  uv_check_init(uv_default_loop(), &h);
  uv_check_start(&h, NULL);
  uv_unref(uv_default_loop());
  uv_run(uv_default_loop());
  return 0;
}


static void prepare_cb(uv_prepare_t* handle, int status) {
  ASSERT(handle != NULL);
  ASSERT(status == 0);

  uv_unref(uv_default_loop());
}


TEST_IMPL(unref_in_prepare_cb) {
  uv_prepare_t h;
  uv_prepare_init(uv_default_loop(), &h);
  uv_prepare_start(&h, prepare_cb);
  uv_run(uv_default_loop());
  return 0;
}
