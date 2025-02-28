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
#include "internal.h"

#ifdef __linux__

#define uv__random_getrandom_init() 0

#else  /* !__linux__ */

#include <stddef.h>
#include <dlfcn.h>

typedef ssize_t (*uv__getrandom_cb)(void *, size_t, unsigned);

static uv__getrandom_cb uv__getrandom;
static uv_once_t once = UV_ONCE_INIT;

static void uv__random_getrandom_init_once(void) {
  uv__getrandom = (uv__getrandom_cb) dlsym(RTLD_DEFAULT, "getrandom");
}

static int uv__random_getrandom_init(void) {
  uv_once(&once, uv__random_getrandom_init_once);

  if (uv__getrandom == NULL)
    return UV_ENOSYS;

  return 0;
}

#endif  /* !__linux__ */

int uv__random_getrandom(void* buf, size_t buflen) {
  ssize_t n;
  size_t pos;
  int rc;

  rc = uv__random_getrandom_init();
  if (rc != 0)
    return rc;

  for (pos = 0; pos != buflen; pos += n) {
    do {
      n = buflen - pos;

      /* Most getrandom() implementations promise that reads <= 256 bytes
       * will always succeed and won't be interrupted by signals.
       * It's therefore useful to split it up in smaller reads because
       * one big read may, in theory, continuously fail with EINTR.
       */
      if (n > 256)
        n = 256;

      n = uv__getrandom((char *) buf + pos, n, 0);
    } while (n == -1 && errno == EINTR);

    if (n == -1)
      return UV__ERR(errno);

    if (n == 0)
      return UV_EIO;
  }

  return 0;
}
