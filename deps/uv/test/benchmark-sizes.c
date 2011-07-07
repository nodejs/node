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


BENCHMARK_IMPL(sizes) {
  LOGF("uv_req_t: %u bytes\n", (unsigned int) sizeof(uv_req_t));
  LOGF("uv_tcp_t: %u bytes\n", (unsigned int) sizeof(uv_tcp_t));
  LOGF("uv_prepare_t: %u bytes\n", (unsigned int) sizeof(uv_prepare_t));
  LOGF("uv_check_t: %u bytes\n", (unsigned int) sizeof(uv_check_t));
  LOGF("uv_idle_t: %u bytes\n", (unsigned int) sizeof(uv_idle_t));
  LOGF("uv_async_t: %u bytes\n", (unsigned int) sizeof(uv_async_t));
  LOGF("uv_timer_t: %u bytes\n", (unsigned int) sizeof(uv_timer_t));
  return 0;
}
