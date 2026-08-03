/* Copyright libuv contributors. All rights reserved.
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
#include <stdlib.h>
#include <string.h>

static int limit;
static int alloc;

static void* t_realloc(void* p, size_t n) {
  alloc += n;
  if (alloc > limit)
    return NULL;
  p = realloc(p, n);
  ASSERT_NOT_NULL(p);
  return p;
}

static void* t_calloc(size_t m, size_t n) {
  return t_realloc(NULL, m * n);
}

static void* t_malloc(size_t n) {
  return t_realloc(NULL, n);
}

TEST_IMPL(loop_init_oom) {
  uv_loop_t loop;
  int err;

  ASSERT_OK(uv_replace_allocator(t_malloc, t_realloc, t_calloc, free));
  for (;;) {
    err = uv_loop_init(&loop);
    if (err == 0)
      break;
    ASSERT_EQ(err, UV_ENOMEM);
    limit += 8;
    alloc = 0;
  }
  ASSERT_OK(uv_loop_close(&loop));
  return 0;
}
