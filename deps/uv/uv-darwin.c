/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
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

#include <stdint.h>
#include <CoreServices/CoreServices.h>
#include <mach/mach.h>
#include <mach/mach_time.h>


uint64_t uv_hrtime() {
  uint64_t time;
  Nanoseconds enano;
  time = mach_absolute_time(); 
  enano = AbsoluteToNanoseconds(*(AbsoluteTime *)&time);
  return (*(uint64_t *)&enano);
}


int uv_exepath(char* buffer, size_t* size) {
  uint32_t usize;
  int result;
  char* path;
  char* fullpath;

  if (!buffer || !size) {
    return -1;
  }

  usize = *size;
  result = _NSGetExecutablePath(buffer, &usize);
  if (result) return result;

  path = (char*)malloc(2 * PATH_MAX);
  fullpath = realpath(buffer, path);

  if (fullpath == NULL) {
    free(path);
    return -1;
  }

  strncpy(buffer, fullpath, *size);
  free(fullpath);
  *size = strlen(buffer);
  return 0;
}
