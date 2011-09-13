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

/*
 * TODO Share this code with Windows.
 * See https://github.com/joyent/libuv/issues/76
 */

#include "uv.h"
#include "internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


/* TODO Expose callback to user to handle fatal error like V8 does. */
void uv_fatal_error(const int errorno, const char* syscall) {
  char* buf = NULL;
  const char* errmsg;

  if (buf) {
    errmsg = buf;
  } else {
    errmsg = "Unknown error";
  }

  if (syscall) {
    fprintf(stderr, "\nlibuv fatal error. %s: (%d) %s\n", syscall, errorno,
        errmsg);
  } else {
    fprintf(stderr, "\nlibuv fatal error. (%d) %s\n", errorno, errmsg);
  }

  abort();
}


uv_err_t uv_last_error(uv_loop_t* loop) {
  return loop->last_err;
}


char* uv_strerror(uv_err_t err) {
  return strerror(err.sys_errno_);
}


uv_err_code uv_translate_sys_error(int sys_errno) {
  switch (sys_errno) {
    case 0: return UV_OK;
    case ENOENT: return UV_ENOENT;
    case EACCES: return UV_EACCESS;
    case EBADF: return UV_EBADF;
    case EPIPE: return UV_EPIPE;
    case EAGAIN: return UV_EAGAIN;
    case ECONNRESET: return UV_ECONNRESET;
    case EFAULT: return UV_EFAULT;
    case EMFILE: return UV_EMFILE;
    case EMSGSIZE: return UV_EMSGSIZE;
    case EINVAL: return UV_EINVAL;
    case ECONNREFUSED: return UV_ECONNREFUSED;
    case EADDRINUSE: return UV_EADDRINUSE;
    case EADDRNOTAVAIL: return UV_EADDRNOTAVAIL;
    case ENOTCONN: return UV_ENOTCONN;
    case EEXIST: return UV_EEXIST;
    default: return UV_UNKNOWN;
  }

  assert(0 && "unreachable");
  return -1;
}


uv_err_t uv_err_new_artificial(uv_loop_t* loop, int code) {
  uv_err_t err;
  err.sys_errno_ = 0;
  err.code = code;
  loop->last_err = err;
  return err;
}


uv_err_t uv_err_new(uv_loop_t* loop, int sys_error) {
  uv_err_t err;
  err.sys_errno_ = sys_error;
  err.code = uv_translate_sys_error(sys_error);
  loop->last_err = err;
  return err;
}
