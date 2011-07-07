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


#ifndef MICROSEC
# define MICROSEC 1000000
#endif

#ifndef NANOSEC
# define NANOSEC 1000000000
#endif



/*
 * We expect the amount of time passed to be at least one us plus two system
 * calls. Therefore checking that at least a microsecond has elapsed is safe.
 */
TEST_IMPL(hrtime) {
  uint64_t a, b, diff;

  a = uv_hrtime();
  uv_sleep(100);
  b = uv_hrtime();

  diff = b - a;

  printf("diff = %llu\n", diff);

  ASSERT(diff >= NANOSEC / MICROSEC);
  ASSERT(diff > MICROSEC);
  return 0;
}
