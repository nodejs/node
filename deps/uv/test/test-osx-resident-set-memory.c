/* Copyright libuv project contributors. All rights reserved.
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

#ifdef __APPLE__

#include <string.h>
#include <sys/mman.h>

TEST_IMPL(osx_resident_set_memory) {
  size_t before;
  size_t during;
  size_t after;
  size_t size;
  char* mem;

  size = 64 << 20;

  ASSERT_OK(uv_resident_set_memory(&before));

  mem = mmap(NULL,
             size,
             PROT_READ | PROT_WRITE,
             MAP_ANON | MAP_PRIVATE,
             -1,
             0);
  ASSERT_PTR_NE(mem, MAP_FAILED);
  memset(mem, 42, size);

  ASSERT_OK(uv_resident_set_memory(&during));
  ASSERT_GE(during, before + size / 2);

  /* How allocators free memory on macOS; the pages stay resident. */
  ASSERT_OK(madvise(mem, size, MADV_FREE_REUSABLE));

  ASSERT_OK(uv_resident_set_memory(&after));
  ASSERT_LT(after, before + size / 2);

  ASSERT_OK(munmap(mem, size));

  return 0;
}

#endif /* __APPLE__ */
