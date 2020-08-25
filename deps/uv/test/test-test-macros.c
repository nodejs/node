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

#include "task.h"

int test_macros_evil(void) {
  static int x;
  return x++;
}


TEST_IMPL(test_macros) {
  char* a = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  char* b = "ABCDEFGHIJKLMNOPQRSTUVWXYz";
  char* c = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  int i;

  i = test_macros_evil();
  ASSERT_STR_NE(a, b);
  ASSERT_STR_EQ(a, c);
  ASSERT_EQ(i + 1, test_macros_evil());
  ASSERT_EQ(i + 2, test_macros_evil());
  return 0;
}
