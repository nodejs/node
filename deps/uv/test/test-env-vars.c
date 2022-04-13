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
  const char* name2 = "UV_TEST_FOO2";
  char buf[BUF_SIZE];
  size_t size;
  int i, r, envcount, found, found_win_special;
  uv_env_item_t* envitems;

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

  /* Setting an environment variable to the empty string does not delete it. */
  r = uv_os_setenv(name, "");
  ASSERT(r == 0);
  size = BUF_SIZE;
  r = uv_os_getenv(name, buf, &size);
  ASSERT(r == 0);
  ASSERT(size == 0);
  ASSERT(strlen(buf) == 0);

  /* Check getting all env variables. */
  r = uv_os_setenv(name, "123456789");
  ASSERT(r == 0);
  r = uv_os_setenv(name2, "");
  ASSERT(r == 0);
#ifdef _WIN32
  /* Create a special environment variable on Windows in case there are no
     naturally occurring ones. */
  r = uv_os_setenv("=Z:", "\\");
  ASSERT(r == 0);
#endif

  r = uv_os_environ(&envitems, &envcount);
  ASSERT(r == 0);
  ASSERT(envcount > 0);

  found = 0;
  found_win_special = 0;

  for (i = 0; i < envcount; i++) {
    /* printf("Env: %s = %s\n", envitems[i].name, envitems[i].value); */
    if (strcmp(envitems[i].name, name) == 0) {
      found++;
      ASSERT(strcmp(envitems[i].value, "123456789") == 0);
    } else if (strcmp(envitems[i].name, name2) == 0) {
      found++;
      ASSERT(strlen(envitems[i].value) == 0);
    } else if (envitems[i].name[0] == '=') {
      found_win_special++;
    }
  }

  ASSERT(found == 2);
#ifdef _WIN32
  ASSERT(found_win_special > 0);
#endif

  uv_os_free_environ(envitems, envcount);

  r = uv_os_unsetenv(name);
  ASSERT(r == 0);

  r = uv_os_unsetenv(name2);
  ASSERT(r == 0);

  for (i = 1; i <= 4; i++) {
    size_t n;
    char* p;

    n = i * 32768;
    size = n + 1;

    p = malloc(size);
    ASSERT_NOT_NULL(p);

    memset(p, 'x', n);
    p[n] = '\0';

    ASSERT_EQ(0, uv_os_setenv(name, p));
    ASSERT_EQ(0, uv_os_getenv(name, p, &size));
    ASSERT_EQ(n, size);

    for (n = 0; n < size; n++)
      ASSERT_EQ('x', p[n]);

    ASSERT_EQ(0, uv_os_unsetenv(name));
    free(p);
  }

  return 0;
}
