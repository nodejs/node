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

#ifdef _WIN32
# include <io.h>
# include <windows.h>
#else /*  Unix */
# include <fcntl.h>
# include <unistd.h>
#endif

#include <string.h>
#include <errno.h>


TEST_IMPL(tty) {
  int r, width, height;
  int ttyin_fd, ttyout_fd;
  uv_tty_t tty_in, tty_out;
  uv_loop_t* loop = uv_default_loop();

  /* Make sure we have an FD that refers to a tty */
#ifdef _WIN32
  HANDLE handle;
  handle = CreateFileA("conin$",
                       GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,
                       NULL);
  ASSERT(handle != INVALID_HANDLE_VALUE);
  ttyin_fd = _open_osfhandle((intptr_t) handle, 0);

  handle = CreateFileA("conout$",
                       GENERIC_READ | GENERIC_WRITE,
                       FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL,
                       OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL,
                       NULL);
  ASSERT(handle != INVALID_HANDLE_VALUE);
  ttyout_fd = _open_osfhandle((intptr_t) handle, 0);

#else /* unix */
  ttyin_fd = open("/dev/tty", O_RDONLY, 0);
  if (ttyin_fd < 0)
    LOGF("Cannot open /dev/tty as read-only: %s\n", strerror(errno));

  ttyout_fd = open("/dev/tty", O_WRONLY, 0);
  if (ttyout_fd < 0)
    LOGF("Cannot open /dev/tty as write-only: %s\n", strerror(errno));
#endif

  ASSERT(ttyin_fd >= 0);
  ASSERT(ttyout_fd >= 0);

  ASSERT(UV_UNKNOWN_HANDLE == uv_guess_handle(-1));

  ASSERT(UV_TTY == uv_guess_handle(ttyin_fd));
  ASSERT(UV_TTY == uv_guess_handle(ttyout_fd));

  r = uv_tty_init(uv_default_loop(), &tty_in, ttyin_fd, 1);  /* Readable. */
  ASSERT(r == 0);

  r = uv_tty_init(uv_default_loop(), &tty_out, ttyout_fd, 0);  /* Writable. */
  ASSERT(r == 0);

  r = uv_tty_get_winsize(&tty_out, &width, &height);
  ASSERT(r == 0);

  printf("width=%d height=%d\n", width, height);

  /*
   * Is it a safe assumption that most people have terminals larger than
   * 10x10?
   */
  ASSERT(width > 10);
  ASSERT(height > 10);

  /* Turn on raw mode. */
  r = uv_tty_set_mode(&tty_in, 1);
  ASSERT(r == 0);

  /* Turn off raw mode. */
  r = uv_tty_set_mode(&tty_in, 0);
  ASSERT(r == 0);

  /* TODO check the actual mode! */

  uv_close((uv_handle_t*) &tty_in, NULL);
  uv_close((uv_handle_t*) &tty_out, NULL);

  uv_run(loop, UV_RUN_DEFAULT);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
