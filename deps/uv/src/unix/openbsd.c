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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/sched.h>
#include <sys/time.h>
#include <sys/sysctl.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


int uv__platform_loop_init(uv_loop_t* loop) {
  return uv__kqueue_init(loop);
}


void uv__platform_loop_delete(uv_loop_t* loop) {
}


void uv_loadavg(double avg[3]) {
  struct loadavg info;
  size_t size = sizeof(info);
  int which[] = {CTL_VM, VM_LOADAVG};

  if (sysctl(which, ARRAY_SIZE(which), &info, &size, NULL, 0) < 0) return;

  avg[0] = (double) info.ldavg[0] / info.fscale;
  avg[1] = (double) info.ldavg[1] / info.fscale;
  avg[2] = (double) info.ldavg[2] / info.fscale;
}


int uv_exepath(char* buffer, size_t* size) {
  int mib[4];
  char **argsbuf = NULL;
  size_t argsbuf_size = 100U;
  size_t exepath_size;
  pid_t mypid;
  int err;

  if (buffer == NULL || size == NULL || *size == 0)
    return UV_EINVAL;

  mypid = getpid();
  for (;;) {
    err = UV_ENOMEM;
    argsbuf = uv__reallocf(argsbuf, argsbuf_size);
    if (argsbuf == NULL)
      goto out;
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC_ARGS;
    mib[2] = mypid;
    mib[3] = KERN_PROC_ARGV;
    if (sysctl(mib, ARRAY_SIZE(mib), argsbuf, &argsbuf_size, NULL, 0) == 0) {
      break;
    }
    if (errno != ENOMEM) {
      err = UV__ERR(errno);
      goto out;
    }
    argsbuf_size *= 2U;
  }

  if (argsbuf[0] == NULL) {
    err = UV_EINVAL;  /* FIXME(bnoordhuis) More appropriate error. */
    goto out;
  }

  *size -= 1;
  exepath_size = strlen(argsbuf[0]);
  if (*size > exepath_size)
    *size = exepath_size;

  memcpy(buffer, argsbuf[0], *size);
  buffer[*size] = '\0';
  err = 0;

out:
  uv__free(argsbuf);

  return err;
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
  uint64_t info;
  int which[] = {CTL_HW, HW_PHYSMEM64};
  size_t size = sizeof(info);

  if (sysctl(which, ARRAY_SIZE(which), &info, &size, NULL, 0))
    return UV__ERR(errno);

  return (uint64_t) info;
}


uint64_t uv_get_constrained_memory(void) {
  return 0;  /* Memory constraints are unknown. */
}


int uv_resident_set_memory(size_t* rss) {
  struct kinfo_proc kinfo;
  size_t page_size = getpagesize();
  size_t size = sizeof(struct kinfo_proc);
  int mib[6];

  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PID;
  mib[3] = getpid();
  mib[4] = sizeof(struct kinfo_proc);
  mib[5] = 1;

  if (sysctl(mib, ARRAY_SIZE(mib), &kinfo, &size, NULL, 0) < 0)
    return UV__ERR(errno);

  *rss = kinfo.p_vm_rssize * page_size;
  return 0;
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
  unsigned int ticks = (unsigned int)sysconf(_SC_CLK_TCK),
               multiplier = ((uint64_t)1000L / ticks), cpuspeed;
  uint64_t info[CPUSTATES];
  char model[512];
  int numcpus = 1;
  int which[] = {CTL_HW,HW_MODEL};
  int percpu[] = {CTL_KERN,KERN_CPTIME2,0};
  size_t size;
  int i, j;
  uv_cpu_info_t* cpu_info;

  size = sizeof(model);
  if (sysctl(which, ARRAY_SIZE(which), &model, &size, NULL, 0))
    return UV__ERR(errno);

  which[1] = HW_NCPUONLINE;
  size = sizeof(numcpus);
  if (sysctl(which, ARRAY_SIZE(which), &numcpus, &size, NULL, 0))
    return UV__ERR(errno);

  *cpu_infos = uv__malloc(numcpus * sizeof(**cpu_infos));
  if (!(*cpu_infos))
    return UV_ENOMEM;

  i = 0;
  *count = numcpus;

  which[1] = HW_CPUSPEED;
  size = sizeof(cpuspeed);
  if (sysctl(which, ARRAY_SIZE(which), &cpuspeed, &size, NULL, 0))
    goto error;

  size = sizeof(info);
  for (i = 0; i < numcpus; i++) {
    percpu[2] = i;
    if (sysctl(percpu, ARRAY_SIZE(percpu), &info, &size, NULL, 0))
      goto error;

    cpu_info = &(*cpu_infos)[i];

    cpu_info->cpu_times.user = (uint64_t)(info[CP_USER]) * multiplier;
    cpu_info->cpu_times.nice = (uint64_t)(info[CP_NICE]) * multiplier;
    cpu_info->cpu_times.sys = (uint64_t)(info[CP_SYS]) * multiplier;
    cpu_info->cpu_times.idle = (uint64_t)(info[CP_IDLE]) * multiplier;
    cpu_info->cpu_times.irq = (uint64_t)(info[CP_INTR]) * multiplier;

    cpu_info->model = uv__strdup(model);
    cpu_info->speed = cpuspeed;
  }

  return 0;

error:
  *count = 0;
  for (j = 0; j < i; j++)
    uv__free((*cpu_infos)[j].model);

  uv__free(*cpu_infos);
  *cpu_infos = NULL;
  return UV__ERR(errno);
}
