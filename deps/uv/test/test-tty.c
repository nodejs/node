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

TEST_IMPL(tty) {
  int r, width, height;
  uv_tty_t tty;
  uv_loop_t* loop = uv_default_loop();

  /*
   * Not necessarally a problem if this assert goes off. E.G you are piping
   * this test to a file. 0 == stdin.
   */
  ASSERT(UV_TTY == uv_guess_handle(0));

  r = uv_tty_init(uv_default_loop(), &tty, 0);
  ASSERT(r == 0);

  r = uv_tty_get_winsize(&tty, &width, &height);
  ASSERT(r == 0);

  printf("width=%d height=%d\n", width, height);

  /*
   * Is it a safe assumption that most people have terminals larger than
   * 10x10?
   */
  ASSERT(width > 10);
  ASSERT(height > 10);

  uv_close((uv_handle_t*)&tty, NULL);

  uv_run(loop);

  return 0;
}
