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

#include <string.h>

#define memeq(a, b, c) (memcmp((a), (b), (c)) == 0)


TEST_IMPL(strlcpy) {
  size_t r;

  {
    char dst[2] = "A";
    r = uv_strlcpy(dst, "", 0);
    ASSERT(r == 0);
    ASSERT(memeq(dst, "A", 1));
  }

  {
    char dst[2] = "A";
    r = uv_strlcpy(dst, "B", 1);
    ASSERT(r == 0);
    ASSERT(memeq(dst, "", 1));
  }

  {
    char dst[2] = "A";
    r = uv_strlcpy(dst, "B", 2);
    ASSERT(r == 1);
    ASSERT(memeq(dst, "B", 2));
  }

  {
    char dst[3] = "AB";
    r = uv_strlcpy(dst, "CD", 3);
    ASSERT(r == 2);
    ASSERT(memeq(dst, "CD", 3));
  }

  return 0;
}


TEST_IMPL(strlcat) {
  size_t r;

  {
    char dst[2] = "A";
    r = uv_strlcat(dst, "B", 1);
    ASSERT(r == 1);
    ASSERT(memeq(dst, "A", 2));
  }

  {
    char dst[2] = "A";
    r = uv_strlcat(dst, "B", 2);
    ASSERT(r == 1);
    ASSERT(memeq(dst, "A", 2));
  }

  {
    char dst[3] = "A";
    r = uv_strlcat(dst, "B", 3);
    ASSERT(r == 2);
    ASSERT(memeq(dst, "AB", 3));
  }

  {
    char dst[5] = "AB";
    r = uv_strlcat(dst, "CD", 5);
    ASSERT(r == 4);
    ASSERT(memeq(dst, "ABCD", 5));
  }

  return 0;
}
