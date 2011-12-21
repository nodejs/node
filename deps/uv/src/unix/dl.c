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

#include <dlfcn.h>
#include <errno.h>

/* The dl family of functions don't set errno. We need a good way to communicate
 * errors to the caller but there is only dlerror() and that returns a string -
 * a string that may or may not be safe to keep a reference to...
 */
static const uv_err_t uv_inval_ = { UV_EINVAL, EINVAL };


uv_err_t uv_dlopen(const char* filename, uv_lib_t* library) {
  void* handle = dlopen(filename, RTLD_LAZY);
  if (handle == NULL) {
    return uv_inval_;
  }

  *library = handle;
  return uv_ok_;
}


uv_err_t uv_dlclose(uv_lib_t library) {
  if (dlclose(library) != 0) {
    return uv_inval_;
  }

  return uv_ok_;
}


uv_err_t uv_dlsym(uv_lib_t library, const char* name, void** ptr) {
  void* address;

  /* Reset error status. */
  dlerror();

  address = dlsym(library, name);

  if (dlerror()) {
    return uv_inval_;
  }

  *ptr = (void*) address;
  return uv_ok_;
}
