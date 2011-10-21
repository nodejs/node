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

#include <assert.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <vm/vm_param.h> /* VM_LOADAVG */
#include <time.h>
#include <unistd.h> /* sysconf */

#undef NANOSEC
#define NANOSEC 1000000000


uint64_t uv_hrtime(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (ts.tv_sec * NANOSEC + ts.tv_nsec);
}


int uv_exepath(char* buffer, size_t* size) {
  uint32_t usize;
  int result;
  char* path;
  char* fullpath;
  int mib[4];
  size_t cb;

  if (!buffer || !size) {
    return -1;
  }


  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = -1;

  cb = *size;
  if (sysctl(mib, 4, buffer, &cb, NULL, 0) < 0) {
    *size = 0;
    return -1;
  }
  *size = strlen(buffer);

  return 0;
}

uint64_t uv_get_free_memory(void) {
  int freecount;
  size_t size = sizeof(freecount);

  if(sysctlbyname("vm.stats.vm.v_free_count",
                  &freecount, &size, NULL, 0) == -1){
    return -1;
  }
  return (uint64_t) freecount * sysconf(_SC_PAGESIZE);

}

uint64_t uv_get_total_memory(void) {
  unsigned long info;
  int which[] = {CTL_HW, HW_PHYSMEM};

  size_t size = sizeof(info);

  if (sysctl(which, 2, &info, &size, NULL, 0) < 0) {
    return -1;
  }

  return (uint64_t) info;
}

void uv_loadavg(double avg[3]) {
  struct loadavg info;
  size_t size = sizeof(info);
  int which[] = {CTL_VM, VM_LOADAVG};

  if (sysctl(which, 2, &info, &size, NULL, 0) < 0) return;

  avg[0] = (double) info.ldavg[0] / info.fscale;
  avg[1] = (double) info.ldavg[1] / info.fscale;
  avg[2] = (double) info.ldavg[2] / info.fscale;
}
