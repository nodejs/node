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

#define _GNU_SOURCE 1

#include "uv.h"
#include "internal.h"

#include <hurd.h>
#include <hurd/process.h>
#include <mach/task_info.h>
#include <mach/vm_statistics.h>
#include <mach/vm_param.h>

#include <inttypes.h>
#include <stddef.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

int uv_exepath(char* buffer, size_t* size) {
  kern_return_t err;
  /* XXX in current Hurd, strings are char arrays of 1024 elements */
  string_t exepath;
  ssize_t copied;

  if (buffer == NULL || size == NULL || *size == 0)
    return UV_EINVAL;

  if (*size - 1 > 0) {
    /* XXX limited length of buffer in current Hurd, this API will probably
     * evolve in the future */
    err = proc_get_exe(getproc(), getpid(), exepath);

    if (err)
      return UV__ERR(err);
  }

  copied = uv__strscpy(buffer, exepath, *size);

  /* do not return error on UV_E2BIG failure */
  *size = copied < 0 ? strlen(buffer) : (size_t) copied;

  return 0;
}

int uv_resident_set_memory(size_t* rss) {
  kern_return_t err;
  struct task_basic_info bi;
  mach_msg_type_number_t count;

  count = TASK_BASIC_INFO_COUNT;
  err = task_info(mach_task_self(), TASK_BASIC_INFO,
		  (task_info_t) &bi, &count);

  if (err)
    return UV__ERR(err);

  *rss = bi.resident_size;

  return 0;
}

uint64_t uv_get_free_memory(void) {
  kern_return_t err;
  struct vm_statistics vmstats;
  
  err = vm_statistics(mach_task_self(), &vmstats);

  if (err)
    return 0;
  
  return vmstats.free_count * vm_page_size;
}


uint64_t uv_get_total_memory(void) {
  kern_return_t err;
  host_basic_info_data_t hbi;
  mach_msg_type_number_t cnt;
  
  cnt = HOST_BASIC_INFO_COUNT;
  err = host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t) &hbi, &cnt); 

  if (err)
    return 0;

  return hbi.memory_size;
}


int uv_uptime(double* uptime) {
  char buf[128];

  /* Try /proc/uptime first */
  if (0 == uv__slurp("/proc/uptime", buf, sizeof(buf)))
    if (1 == sscanf(buf, "%lf", uptime))
      return 0;

  /* Reimplement here code from procfs to calculate uptime if not mounted? */

  return UV__ERR(EIO);
}

void uv_loadavg(double avg[3]) {
  char buf[128];  /* Large enough to hold all of /proc/loadavg. */

  if (0 == uv__slurp("/proc/loadavg", buf, sizeof(buf)))
    if (3 == sscanf(buf, "%lf %lf %lf", &avg[0], &avg[1], &avg[2]))
      return;

  /* Reimplement here code from procfs to calculate loadavg if not mounted? */
}


int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  kern_return_t err;
  host_basic_info_data_t hbi;
  mach_msg_type_number_t cnt;
  
  /* Get count of cpus  */
  cnt = HOST_BASIC_INFO_COUNT;
  err = host_info(mach_host_self(), HOST_BASIC_INFO, (host_info_t) &hbi, &cnt); 

  if (err) {
    err = UV__ERR(err);
    goto abort;
  }

  /* XXX not implemented on the Hurd */
  *cpu_infos = uv__calloc(hbi.avail_cpus, sizeof(**cpu_infos));
  if (*cpu_infos == NULL) {
    err = UV_ENOMEM;
    goto abort;
  }

  *count = hbi.avail_cpus;

  return 0;
  
 abort:
  *cpu_infos = NULL;
  *count = 0;
  return err;
}

uint64_t uv_get_constrained_memory(void) {
  return 0;  /* Memory constraints are unknown. */
}
