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

#include <assert.h>
#include <string.h>
#include <errno.h>

#include <kvm.h>
#include <paths.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>

#include <sys/resource.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <uvm/uvm_extern.h>

#include <unistd.h>
#include <time.h>


int uv__platform_loop_init(uv_loop_t* loop) {
  return uv__kqueue_init(loop);
}


void uv__platform_loop_delete(uv_loop_t* loop) {
}


void uv_loadavg(double avg[3]) {
  struct loadavg info;
  size_t size = sizeof(info);
  int which[] = {CTL_VM, VM_LOADAVG};

  if (sysctl(which, ARRAY_SIZE(which), &info, &size, NULL, 0) == -1) return;

  avg[0] = (double) info.ldavg[0] / info.fscale;
  avg[1] = (double) info.ldavg[1] / info.fscale;
  avg[2] = (double) info.ldavg[2] / info.fscale;
}


int uv_exepath(char* buffer, size_t* size) {
  /* Intermediate buffer, retrieving partial path name does not work
   * As of NetBSD-8(beta), vnode->path translator does not handle files
   * with longer names than 31 characters.
   */
  char int_buf[PATH_MAX];
  size_t int_size;
  int mib[4];

  if (buffer == NULL || size == NULL || *size == 0)
    return UV_EINVAL;

  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC_ARGS;
  mib[2] = -1;
  mib[3] = KERN_PROC_PATHNAME;
  int_size = ARRAY_SIZE(int_buf);

  if (sysctl(mib, 4, int_buf, &int_size, NULL, 0))
    return UV__ERR(errno);

  /* Copy string from the intermediate buffer to outer one with appropriate
   * length.
   */
  /* TODO(bnoordhuis) Check uv__strscpy() return value. */
  uv__strscpy(buffer, int_buf, *size);

  /* Set new size. */
  *size = strlen(buffer);

  return 0;
}


uint64_t uv_get_free_memory(void) {
  struct uvmexp info;
  size_t size = sizeof(info);
  int which[] = {CTL_VM, VM_UVMEXP};

  if (sysctl(which, ARRAY_SIZE(which), &info, &size, NULL, 0))
    return UV__ERR(errno);

  return (uint64_t) info.free * sysconf(_SC_PAGESIZE);
}


uint64_t uv_get_total_memory(void) {
#if defined(HW_PHYSMEM64)
  uint64_t info;
  int which[] = {CTL_HW, HW_PHYSMEM64};
#else
  unsigned int info;
  int which[] = {CTL_HW, HW_PHYSMEM};
#endif
  size_t size = sizeof(info);

  if (sysctl(which, ARRAY_SIZE(which), &info, &size, NULL, 0))
    return UV__ERR(errno);

  return (uint64_t) info;
}


uint64_t uv_get_constrained_memory(void) {
  return 0;  /* Memory constraints are unknown. */
}


int uv_resident_set_memory(size_t* rss) {
  kvm_t *kd = NULL;
  struct kinfo_proc2 *kinfo = NULL;
  pid_t pid;
  int nprocs;
  int max_size = sizeof(struct kinfo_proc2);
  int page_size;

  page_size = getpagesize();
  pid = getpid();

  kd = kvm_open(NULL, NULL, NULL, KVM_NO_FILES, "kvm_open");

  if (kd == NULL) goto error;

  kinfo = kvm_getproc2(kd, KERN_PROC_PID, pid, max_size, &nprocs);
  if (kinfo == NULL) goto error;

  *rss = kinfo->p_vm_rssize * page_size;

  kvm_close(kd);

  return 0;

error:
  if (kd) kvm_close(kd);
  return UV_EPERM;
}


int uv_uptime(double* uptime) {
  time_t now;
  struct timeval info;
  size_t size = sizeof(info);
  static int which[] = {CTL_KERN, KERN_BOOTTIME};

  if (sysctl(which, ARRAY_SIZE(which), &info, &size, NULL, 0))
    return UV__ERR(errno);

  now = time(NULL);

  *uptime = (double)(now - info.tv_sec);
  return 0;
}


int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  unsigned int ticks = (unsigned int)sysconf(_SC_CLK_TCK);
  unsigned int multiplier = ((uint64_t)1000L / ticks);
  unsigned int cur = 0;
  uv_cpu_info_t* cpu_info;
  u_int64_t* cp_times;
  char model[512];
  u_int64_t cpuspeed;
  int numcpus;
  size_t size;
  int i;

  size = sizeof(model);
  if (sysctlbyname("machdep.cpu_brand", &model, &size, NULL, 0) &&
      sysctlbyname("hw.model", &model, &size, NULL, 0)) {
    return UV__ERR(errno);
  }

  size = sizeof(numcpus);
  if (sysctlbyname("hw.ncpu", &numcpus, &size, NULL, 0))
    return UV__ERR(errno);
  *count = numcpus;

  /* Only i386 and amd64 have machdep.tsc_freq */
  size = sizeof(cpuspeed);
  if (sysctlbyname("machdep.tsc_freq", &cpuspeed, &size, NULL, 0))
    cpuspeed = 0;

  size = numcpus * CPUSTATES * sizeof(*cp_times);
  cp_times = uv__malloc(size);
  if (cp_times == NULL)
    return UV_ENOMEM;

  if (sysctlbyname("kern.cp_time", cp_times, &size, NULL, 0))
    return UV__ERR(errno);

  *cpu_infos = uv__malloc(numcpus * sizeof(**cpu_infos));
  if (!(*cpu_infos)) {
    uv__free(cp_times);
    uv__free(*cpu_infos);
    return UV_ENOMEM;
  }

  for (i = 0; i < numcpus; i++) {
    cpu_info = &(*cpu_infos)[i];
    cpu_info->cpu_times.user = (uint64_t)(cp_times[CP_USER+cur]) * multiplier;
    cpu_info->cpu_times.nice = (uint64_t)(cp_times[CP_NICE+cur]) * multiplier;
    cpu_info->cpu_times.sys = (uint64_t)(cp_times[CP_SYS+cur]) * multiplier;
    cpu_info->cpu_times.idle = (uint64_t)(cp_times[CP_IDLE+cur]) * multiplier;
    cpu_info->cpu_times.irq = (uint64_t)(cp_times[CP_INTR+cur]) * multiplier;
    cpu_info->model = uv__strdup(model);
    cpu_info->speed = (int)(cpuspeed/(uint64_t) 1e6);
    cur += CPUSTATES;
  }
  uv__free(cp_times);
  return 0;
}

int uv__random_sysctl(void* buf, size_t len) {
  static int name[] = {CTL_KERN, KERN_ARND};
  size_t count, req;
  unsigned char* p;

  p = buf;
  while (len) {
    req = len < 32 ? len : 32;
    count = req;

    if (sysctl(name, ARRAY_SIZE(name), p, &count, NULL, 0) == -1)
      return UV__ERR(errno);

    if (count != req)
      return UV_EIO;  /* Can't happen. */

    p += count;
    len -= count;
  }

  return 0;
}
