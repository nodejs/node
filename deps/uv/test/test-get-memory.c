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

TEST_IMPL(get_memory) {
  uint64_t free_mem = uv_get_free_memory();
  uint64_t total_mem = uv_get_total_memory();
  uint64_t constrained_mem = uv_get_constrained_memory();

  printf("free_mem=%llu, total_mem=%llu, constrained_mem=%llu\n",
         (unsigned long long) free_mem,
         (unsigned long long) total_mem,
         (unsigned long long) constrained_mem);

  ASSERT(free_mem > 0);
  ASSERT(total_mem > 0);
  /* On IBMi PASE, the amount of memory in use is always zero. */
#ifdef __PASE__
  ASSERT(total_mem == free_mem);
#else
  ASSERT(total_mem > free_mem);
#endif
  return 0;
}
