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
#include "internal.h"

#include <assert.h>


int uv_tty_init(uv_loop_t* loop, uv_tty_t* tty, uv_file fd) {
  assert(0 && "implement me");
  return -1;
}


int uv_tty_set_mode(uv_tty_t* tty, int mode) {
  assert(0 && "implement me");
  return -1;
}


int uv_is_tty(uv_file file) {
}


int uv_tty_get_winsize(uv_tty_t* tty, int* width, int* height) {
  assert(0 && "implement me");
  return -1;
}


uv_handle_type uv_guess_handle(uv_file file) {
  DWORD result;
  int r = GetConsoleMode((HANDLE)_get_osfhandle(file), &result);

  if (r) {
    return UV_TTY;
  }

  assert(0 && "implement me");

  return UV_UNKNOWN_HANDLE;
}
