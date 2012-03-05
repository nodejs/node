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

__declspec( thread ) DWORD saved_errno = 0;

uv_err_t uv_dlopen(const char* filename, uv_lib_t* library) {
  wchar_t filename_w[32768];
  HMODULE handle;

  if (!uv_utf8_to_utf16(filename,
                        filename_w,
                        sizeof(filename_w) / sizeof(wchar_t))) {
    saved_errno = GetLastError();
    return uv__new_sys_error(saved_errno);
  }

  handle = LoadLibraryW(filename_w);
  if (handle == NULL) {
    saved_errno = GetLastError();
    return uv__new_sys_error(saved_errno);
  }

  *library = handle;
  return uv_ok_;
}


uv_err_t uv_dlclose(uv_lib_t library) {
  if (!FreeLibrary(library)) {
    saved_errno = GetLastError();
    return uv__new_sys_error(saved_errno);
  }

  return uv_ok_;
}


uv_err_t uv_dlsym(uv_lib_t library, const char* name, void** ptr) {
  FARPROC proc = GetProcAddress(library, name);
  if (proc == NULL) {
    saved_errno = GetLastError();
    return uv__new_sys_error(saved_errno);
  }

  *ptr = (void*) proc;
  return uv_ok_;
}


const char *uv_dlerror(uv_lib_t library) {
  char* buf = NULL;
  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                 FORMAT_MESSAGE_IGNORE_INSERTS, NULL, saved_errno,
                 MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPSTR)&buf, 0, NULL);
  return buf;
}


void uv_dlerror_free(uv_lib_t library, const char *msg) {
  LocalFree((LPVOID)msg);
}
