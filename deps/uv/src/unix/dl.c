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
#include <string.h>
#include <locale.h>

static int uv__dlerror(uv_lib_t* lib);


int uv_dlopen(const char* filename, uv_lib_t* lib) {
  dlerror(); /* Reset error status. */
  lib->errmsg = NULL;
  lib->handle = dlopen(filename, RTLD_LAZY);
  return lib->handle ? 0 : uv__dlerror(lib);
}


void uv_dlclose(uv_lib_t* lib) {
  if (lib->errmsg) {
    free(lib->errmsg);
    lib->errmsg = NULL;
  }

  if (lib->handle) {
    /* Ignore errors. No good way to signal them without leaking memory. */
    dlclose(lib->handle);
    lib->handle = NULL;
  }
}


int uv_dlsym(uv_lib_t* lib, const char* name, void** ptr) {
  dlerror(); /* Reset error status. */
  *ptr = dlsym(lib->handle, name);
  return uv__dlerror(lib);
}


const char* uv_dlerror(uv_lib_t* lib) {
  return lib->errmsg ? lib->errmsg : "no error";
}


static int uv__dlerror(uv_lib_t* lib) {
  const char* errmsg;

  if (lib->errmsg)
    free(lib->errmsg);

  errmsg = dlerror();

  if (errmsg) {
    lib->errmsg = strdup(errmsg);
    return -1;
  }
  else {
    lib->errmsg = NULL;
    return 0;
  }
}
