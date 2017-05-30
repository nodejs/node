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

#include <sys/sysinfo.h>
#include <unistd.h>

int uv_uptime(double* uptime) {
  struct sysinfo info;

  if (sysinfo(&info) < 0)
    return -errno;

  *uptime = info.uptime;
  return 0;
}

int uv_resident_set_memory(size_t* rss) {
  /* FIXME: read /proc/meminfo? */
  *rss = 0;
  return UV_ENOSYS;
}

int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  /* FIXME: read /proc/stat? */
  *cpu_infos = NULL;
  *count = 0;
  return UV_ENOSYS;
}

void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  (void)cpu_infos;
  (void)count;
}
