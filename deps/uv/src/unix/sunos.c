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
#include "internal.h"

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/loadavg.h>
#include <unistd.h>
#include <kstat.h>


uint64_t uv_hrtime() {
  return (gethrtime());
}


/*
 * We could use a static buffer for the path manipulations that we need outside
 * of the function, but this function could be called by multiple consumers and
 * we don't want to potentially create a race condition in the use of snprintf.
 */
int uv_exepath(char* buffer, size_t* size) {
  ssize_t res;
  pid_t pid;
  char buf[128];

  if (buffer == NULL)
    return (-1);

  if (size == NULL)
    return (-1);

  pid = getpid();
  (void) snprintf(buf, sizeof (buf), "/proc/%d/path/a.out", pid);
  res = readlink(buf, buffer, *size - 1);

  if (res < 0)
    return (res);

  buffer[res] = '\0';
  *size = res;
  return (0);
}


uint64_t uv_get_free_memory(void) {
  return (uint64_t) sysconf(_SC_PAGESIZE) * sysconf(_SC_AVPHYS_PAGES);
}


uint64_t uv_get_total_memory(void) {
  return (uint64_t) sysconf(_SC_PAGESIZE) * sysconf(_SC_PHYS_PAGES);
}


void uv_loadavg(double avg[3]) {
  (void) getloadavg(avg, 3);
}


int uv_fs_event_init(uv_loop_t* loop,
                     uv_fs_event_t* handle,
                     const char* filename,
                     uv_fs_event_cb cb) {
  uv__set_sys_error(loop, ENOSYS);
  return -1;
}


void uv__fs_event_destroy(uv_fs_event_t* handle) {
  assert(0 && "implement me");
}
