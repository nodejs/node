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

#define PATHMAX 1024
extern char executable_path[];

TEST_IMPL(cwd_and_chdir) {
  char buffer_orig[PATHMAX];
  char buffer_new[PATHMAX];
  size_t size;
  char* last_slash;
  int err;

  size = sizeof(buffer_orig);
  err = uv_cwd(buffer_orig, &size);
  ASSERT(err == 0);

  /* Remove trailing slash unless at a root directory. */
#ifdef _WIN32
  last_slash = strrchr(buffer_orig, '\\');
  ASSERT(last_slash);
  if (last_slash > buffer_orig && *(last_slash - 1) != ':') {
    *last_slash = '\0';
  }
#else /* Unix */
  last_slash = strrchr(buffer_orig, '/');
  ASSERT(last_slash);
  if (last_slash != buffer_orig) {
    *last_slash = '\0';
  }
#endif

  err = uv_chdir(buffer_orig);
  ASSERT(err == 0);

  err = uv_cwd(buffer_new, &size);
  ASSERT(err == 0);

  ASSERT(strcmp(buffer_orig, buffer_new) == 0);

  return 0;
}
