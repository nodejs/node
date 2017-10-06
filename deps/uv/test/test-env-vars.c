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
#include <string.h>

#define BUF_SIZE 10

TEST_IMPL(env_vars) {
  const char* name = "UV_TEST_FOO";
  char buf[BUF_SIZE];
  size_t size;
  int r;

  /* Reject invalid inputs when setting an environment variable */
  r = uv_os_setenv(NULL, "foo");
  ASSERT(r == UV_EINVAL);
  r = uv_os_setenv(name, NULL);
  ASSERT(r == UV_EINVAL);
  r = uv_os_setenv(NULL, NULL);
  ASSERT(r == UV_EINVAL);

  /* Reject invalid inputs when retrieving an environment variable */
  size = BUF_SIZE;
  r = uv_os_getenv(NULL, buf, &size);
  ASSERT(r == UV_EINVAL);
  r = uv_os_getenv(name, NULL, &size);
  ASSERT(r == UV_EINVAL);
  r = uv_os_getenv(name, buf, NULL);
  ASSERT(r == UV_EINVAL);
  size = 0;
  r = uv_os_getenv(name, buf, &size);
  ASSERT(r == UV_EINVAL);

  /* Reject invalid inputs when deleting an environment variable */
  r = uv_os_unsetenv(NULL);
  ASSERT(r == UV_EINVAL);

  /* Successfully set an environment variable */
  r = uv_os_setenv(name, "123456789");
  ASSERT(r == 0);

  /* Successfully read an environment variable */
  size = BUF_SIZE;
  buf[0] = '\0';
  r = uv_os_getenv(name, buf, &size);
  ASSERT(r == 0);
  ASSERT(strcmp(buf, "123456789") == 0);
  ASSERT(size == BUF_SIZE - 1);

  /* Return UV_ENOBUFS if the buffer cannot hold the environment variable */
  size = BUF_SIZE - 1;
  buf[0] = '\0';
  r = uv_os_getenv(name, buf, &size);
  ASSERT(r == UV_ENOBUFS);
  ASSERT(size == BUF_SIZE);

  /* Successfully delete an environment variable */
  r = uv_os_unsetenv(name);
  ASSERT(r == 0);

  /* Return UV_ENOENT retrieving an environment variable that does not exist */
  r = uv_os_getenv(name, buf, &size);
  ASSERT(r == UV_ENOENT);

  /* Successfully delete an environment variable that does not exist */
  r = uv_os_unsetenv(name);
  ASSERT(r == 0);

  return 0;
}
