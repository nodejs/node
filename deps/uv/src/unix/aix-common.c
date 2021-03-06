/* Copyright libuv project contributors. All rights reserved.
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include <sys/time.h>
#include <unistd.h>

#include <procinfo.h>

#include <ctype.h>

extern char* original_exepath;
extern uv_mutex_t process_title_mutex;
extern uv_once_t process_title_mutex_once;
extern void init_process_title_mutex_once(void);

uint64_t uv__hrtime(uv_clocktype_t type) {
  uint64_t G = 1000000000;
  timebasestruct_t t;
  read_wall_time(&t, TIMEBASE_SZ);
  time_base_to_time(&t, TIMEBASE_SZ);
  return (uint64_t) t.tb_high * G + t.tb_low;
}


/*
 * We could use a static buffer for the path manipulations that we need outside
 * of the function, but this function could be called by multiple consumers and
 * we don't want to potentially create a race condition in the use of snprintf.
 * There is no direct way of getting the exe path in AIX - either through /procfs
 * or through some libc APIs. The below approach is to parse the argv[0]'s pattern
 * and use it in conjunction with PATH environment variable to craft one.
 */
int uv_exepath(char* buffer, size_t* size) {
  int res;
  char args[UV__PATH_MAX];
  size_t cached_len;
  struct procsinfo pi;

  if (buffer == NULL || size == NULL || *size == 0)
    return UV_EINVAL;

  uv_once(&process_title_mutex_once, init_process_title_mutex_once);
  uv_mutex_lock(&process_title_mutex);
  if (original_exepath != NULL) {
    cached_len = strlen(original_exepath);
    *size -= 1;
    if (*size > cached_len)
      *size = cached_len;
    memcpy(buffer, original_exepath, *size);
    buffer[*size] = '\0';
    uv_mutex_unlock(&process_title_mutex);
    return 0;
  }
  uv_mutex_unlock(&process_title_mutex);
  pi.pi_pid = getpid();
  res = getargs(&pi, sizeof(pi), args, sizeof(args));

  if (res < 0)
    return UV_EINVAL;

  return uv__search_path(args, buffer, size);
}
