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

#include <sys/stat.h>
#include <unistd.h>

static uv_once_t once = UV_ONCE_INIT;
static int status;


int uv__random_readpath(const char* path, void* buf, size_t buflen) {
  struct stat s;
  size_t pos;
  ssize_t n;
  int fd;

  fd = uv__open_cloexec(path, O_RDONLY);

  if (fd < 0)
    return fd;

  if (uv__fstat(fd, &s)) {
    uv__close(fd);
    return UV__ERR(errno);
  }

  if (!S_ISCHR(s.st_mode)) {
    uv__close(fd);
    return UV_EIO;
  }

  for (pos = 0; pos != buflen; pos += n) {
    do
      n = read(fd, (char*) buf + pos, buflen - pos);
    while (n == -1 && errno == EINTR);

    if (n == -1) {
      uv__close(fd);
      return UV__ERR(errno);
    }

    if (n == 0) {
      uv__close(fd);
      return UV_EIO;
    }
  }

  uv__close(fd);
  return 0;
}


static void uv__random_devurandom_init(void) {
  char c;

  /* Linux's random(4) man page suggests applications should read at least
   * once from /dev/random before switching to /dev/urandom in order to seed
   * the system RNG. Reads from /dev/random can of course block indefinitely
   * until entropy is available but that's the point.
   */
  status = uv__random_readpath("/dev/random", &c, 1);
}


int uv__random_devurandom(void* buf, size_t buflen) {
  uv_once(&once, uv__random_devurandom_init);

  if (status != 0)
    return status;

  return uv__random_readpath("/dev/urandom", buf, buflen);
}
