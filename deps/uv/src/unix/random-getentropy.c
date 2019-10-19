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

#include <stddef.h>
#include <dlfcn.h>

typedef int (*uv__getentropy_cb)(void *, size_t);

static uv__getentropy_cb uv__getentropy;
static uv_once_t once = UV_ONCE_INIT;


static void uv__random_getentropy_init(void) {
  uv__getentropy = (uv__getentropy_cb) dlsym(RTLD_DEFAULT, "getentropy");
}


int uv__random_getentropy(void* buf, size_t buflen) {
  size_t pos;
  size_t stride;

  uv_once(&once, uv__random_getentropy_init);

  if (uv__getentropy == NULL)
    return UV_ENOSYS;

  /* getentropy() returns an error for requests > 256 bytes. */
  for (pos = 0, stride = 256; pos + stride < buflen; pos += stride)
    if (uv__getentropy((char *) buf + pos, stride))
      return UV__ERR(errno);

  if (uv__getentropy((char *) buf + pos, buflen - pos))
    return UV__ERR(errno);

  return 0;
}
