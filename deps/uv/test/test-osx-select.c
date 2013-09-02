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

#ifdef __APPLE__

#include <sys/ioctl.h>
#include <string.h>

static int read_count;


static void alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* buf) {
  static char slab[1024];
  buf->base = slab;
  buf->len = sizeof(slab);
}


static void read_cb(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  fprintf(stdout, "got data %d\n", ++read_count);

  if (read_count == 3)
    uv_close((uv_handle_t*) stream, NULL);
}


TEST_IMPL(osx_select) {
  int r;
  int fd;
  size_t i;
  size_t len;
  const char* str;
  uv_tty_t tty;

  fd = open("/dev/tty", O_RDONLY);

  ASSERT(fd >= 0);

  r = uv_tty_init(uv_default_loop(), &tty, fd, 1);
  ASSERT(r == 0);

  uv_read_start((uv_stream_t*) &tty, alloc_cb, read_cb);

  /* Emulate user-input */
  str = "got some input\n"
        "with a couple of lines\n"
        "feel pretty happy\n";
  for (i = 0, len = strlen(str); i < len; i++) {
    r = ioctl(fd, TIOCSTI, str + i);
    ASSERT(r == 0);
  }

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);

  ASSERT(read_count == 3);

  MAKE_VALGRIND_HAPPY();
  return 0;
}

#endif /* __APPLE__ */
