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

TEST_IMPL(get_passwd) {
/* TODO(gengjiawen): Fix test on QEMU. */
#if defined(__QEMU__)
  RETURN_SKIP("Test does not currently work in QEMU");
#endif

  uv_passwd_t pwd;
  size_t len;
  int r;

  /* Test the normal case */
  r = uv_os_get_passwd(&pwd);
  ASSERT(r == 0);
  len = strlen(pwd.username);
  ASSERT(len > 0);

#ifdef _WIN32
  ASSERT(pwd.shell == NULL);
#else
  len = strlen(pwd.shell);
# ifndef __PASE__
  ASSERT(len > 0);
# endif
#endif

  len = strlen(pwd.homedir);
  ASSERT(len > 0);

#ifdef _WIN32
  if (len == 3 && pwd.homedir[1] == ':')
    ASSERT(pwd.homedir[2] == '\\');
  else
    ASSERT(pwd.homedir[len - 1] != '\\');
#else
  if (len == 1)
    ASSERT(pwd.homedir[0] == '/');
  else
    ASSERT(pwd.homedir[len - 1] != '/');
#endif

#ifdef _WIN32
  ASSERT(pwd.uid == -1);
  ASSERT(pwd.gid == -1);
#else
  ASSERT(pwd.uid >= 0);
  ASSERT(pwd.gid >= 0);
#endif

  /* Test uv_os_free_passwd() */
  uv_os_free_passwd(&pwd);

  ASSERT(pwd.username == NULL);
  ASSERT(pwd.shell == NULL);
  ASSERT(pwd.homedir == NULL);

  /* Test a double free */
  uv_os_free_passwd(&pwd);

  ASSERT(pwd.username == NULL);
  ASSERT(pwd.shell == NULL);
  ASSERT(pwd.homedir == NULL);

  /* Test invalid input */
  r = uv_os_get_passwd(NULL);
  ASSERT(r == UV_EINVAL);

  return 0;
}
