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
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>


int uv_tty_init(uv_loop_t* loop, uv_tty_t* tty, int fd) {
  uv__nonblock(fd, 1);
  uv__stream_init(loop, (uv_stream_t*)tty, UV_TTY);
  uv__stream_open((uv_stream_t*)tty, fd, UV_READABLE | UV_WRITABLE);
  loop->counters.tty_init++;
  return 0;
}


int uv_tty_set_mode(uv_tty_t* tty, int mode) {
  int fd = tty->fd;
  struct termios orig_termios; /* in order to restore at exit */
  struct termios raw;

  if (tcgetattr(fd, &orig_termios) == -1) goto fatal;

  raw = orig_termios;  /* modify the original mode */
  /* input modes: no break, no CR to NL, no parity check, no strip char,
   * no start/stop output control. */
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  /* output modes */
  raw.c_oflag |= (ONLCR);
  /* control modes - set 8 bit chars */
  raw.c_cflag |= (CS8);
  /* local modes - echoing off, canonical off, no extended functions,
   * no signal chars (^Z,^C) */
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  /* control chars - set return condition: min number of bytes and timer.
   * We want read to return every single byte, without timeout. */
  raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0; /* 1 byte, no timer */

  /* put terminal in raw mode after flushing */
  if (tcsetattr(fd, TCSAFLUSH, &raw) < 0) goto fatal;
  return 0;

fatal:
  uv_err_new(tty->loop, ENOTTY);
  return -1;
}


int uv_tty_get_winsize(uv_tty_t* tty, int* width, int* height) {
  struct winsize ws;

  if (ioctl(tty->fd, TIOCGWINSZ, &ws) < 0) {
    uv_err_new(tty->loop, errno);
    return -1;
  }

  *width = ws.ws_col;
  *height = ws.ws_row;

  return 0;
}


uv_handle_type uv_guess_handle(uv_file file) {
  struct stat s;

  if (file < 0) {
    uv_err_new(NULL, EINVAL); /* XXX Need loop? */
    return -1;
  }

  if (isatty(file)) {
    return UV_TTY;
  }

  if (fstat(file, &s)) {
    uv_err_new(NULL, errno); /* XXX Need loop? */
    return -1;
  }

  if (!S_ISSOCK(s.st_mode) && !S_ISFIFO(s.st_mode)) {
    return UV_FILE;
  }

  return UV_NAMED_PIPE;
}
