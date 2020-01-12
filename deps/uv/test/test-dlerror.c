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


TEST_IMPL(dlerror) {
  const char* path = "test/fixtures/load_error.node";
  const char* dlerror_no_error = "no error";
  const char* msg;
  uv_lib_t lib;
  int r;

  lib.errmsg = NULL;
  lib.handle = NULL;
  msg = uv_dlerror(&lib);
  ASSERT(msg != NULL);
  ASSERT(strstr(msg, dlerror_no_error) != NULL);

  r = uv_dlopen(path, &lib);
  ASSERT(r == -1);

  msg = uv_dlerror(&lib);
  ASSERT(msg != NULL);
#ifndef __OpenBSD__
  ASSERT(strstr(msg, path) != NULL);
#endif
  ASSERT(strstr(msg, dlerror_no_error) == NULL);

  /* Should return the same error twice in a row. */
  msg = uv_dlerror(&lib);
  ASSERT(msg != NULL);
#ifndef __OpenBSD__
  ASSERT(strstr(msg, path) != NULL);
#endif
  ASSERT(strstr(msg, dlerror_no_error) == NULL);

  uv_dlclose(&lib);

  return 0;
}
