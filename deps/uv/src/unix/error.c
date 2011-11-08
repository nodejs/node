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


static int uv__translate_lib_error(int code) {
  switch (code) {
    case UV_ENOSYS: return ENOSYS;
    case UV_ENOTSOCK: return ENOTSOCK;
    case UV_ENOENT: return ENOENT;
    case UV_EACCES: return EACCES;
    case UV_EAFNOSUPPORT: return EAFNOSUPPORT;
    case UV_EBADF: return EBADF;
    case UV_EPIPE: return EPIPE;
    case UV_EAGAIN: return EAGAIN;
    case UV_ECONNRESET: return ECONNRESET;
    case UV_EFAULT: return EFAULT;
    case UV_EMFILE: return EMFILE;
    case UV_EMSGSIZE: return EMSGSIZE;
    case UV_EINVAL: return EINVAL;
    case UV_ECONNREFUSED: return ECONNREFUSED;
    case UV_EADDRINUSE: return EADDRINUSE;
    case UV_EADDRNOTAVAIL: return EADDRNOTAVAIL;
    case UV_ENOTDIR: return ENOTDIR;
    case UV_EISDIR: return EISDIR;
    case UV_ENOTCONN: return ENOTCONN;
    case UV_EEXIST: return EEXIST;
    case UV_EHOSTUNREACH: return EHOSTUNREACH;
    default: return -1;
  }

  assert(0 && "unreachable");
  return -1;
}


uv_err_code uv_translate_sys_error(int sys_errno) {
  switch (sys_errno) {
    case 0: return UV_OK;
    case ENOSYS: return UV_ENOSYS;
    case ENOTSOCK: return UV_ENOTSOCK;
    case ENOENT: return UV_ENOENT;
    case EACCES: return UV_EACCES;
    case EAFNOSUPPORT: return UV_EAFNOSUPPORT;
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
    case ENOTDIR: return UV_ENOTDIR;
    case EISDIR: return UV_EISDIR;
    case ENOTCONN: return UV_ENOTCONN;
    case EEXIST: return UV_EEXIST;
    case EHOSTUNREACH: return UV_EHOSTUNREACH;
    case EAI_NONAME: return UV_ENOENT;
    default: return UV_UNKNOWN;
  }

  assert(0 && "unreachable");
  return -1;
}


/* TODO Pull in error messages so we don't have to
 *  a) rely on what the system provides us
 *  b) reverse-map the error codes
 */
const char* uv_strerror(uv_err_t err) {
  int errorno;

  if (err.sys_errno_)
    errorno = err.sys_errno_;
  else
    errorno = uv__translate_lib_error(err.code);

  if (err.code == UV_EADDRINFO)
    return gai_strerror(errorno);

  if (errorno == -1)
    return "Unknown error";
  else
    return strerror(errorno);
}
