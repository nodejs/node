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
#include <sys/user.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <vm/vm_param.h> /* VM_LOADAVG */
#include <time.h>
#include <stdlib.h>
#include <unistd.h> /* sysconf */
#include <fcntl.h>

#ifndef CPUSTATES
# define CPUSTATES 5U
#endif
#ifndef CP_USER
# define CP_USER 0
# define CP_NICE 1
# define CP_SYS 2
# define CP_IDLE 3
# define CP_INTR 4
#endif

static char *process_title;


int uv__platform_loop_init(uv_loop_t* loop) {
  return uv__kqueue_init(loop);
}


void uv__platform_loop_delete(uv_loop_t* loop) {
}


#ifdef __DragonFly__
int uv_exepath(char* buffer, size_t* size) {
  char abspath[PATH_MAX * 2 + 1];
  ssize_t abspath_size;

  if (buffer == NULL || size == NULL || *size == 0)
    return -EINVAL;

  abspath_size = readlink("/proc/curproc/file", abspath, sizeof(abspath));
  if (abspath_size < 0)
    return -errno;

  assert(abspath_size > 0);
  *size -= 1;

  if (*size > abspath_size)
    *size = abspath_size;

  memcpy(buffer, abspath, *size);
  buffer[*size] = '\0';

  return 0;
}
#else
int uv_exepath(char* buffer, size_t* size) {
  char abspath[PATH_MAX * 2 + 1];
  int mib[4];
  size_t abspath_size;

  if (buffer == NULL || size == NULL || *size == 0)
    return -EINVAL;

  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = -1;

  abspath_size = sizeof abspath;
  if (sysctl(mib, 4, abspath, &abspath_size, NULL, 0))
    return -errno;

  assert(abspath_size > 0);
  abspath_size -= 1;
  *size -= 1;

  if (*size > abspath_size)
    *size = abspath_size;

  memcpy(buffer, abspath, *size);
  buffer[*size] = '\0';

  return 0;
}
#endif

uint64_t uv_get_free_memory(void) {
  int freecount;
  size_t size = sizeof(freecount);

  if (sysctlbyname("vm.stats.vm.v_free_count", &freecount, &size, NULL, 0))
    return -errno;

  return (uint64_t) freecount * sysconf(_SC_PAGESIZE);

}


uint64_t uv_get_total_memory(void) {
  unsigned long info;
  int which[] = {CTL_HW, HW_PHYSMEM};

  size_t size = sizeof(info);

  if (sysctl(which, 2, &info, &size, NULL, 0))
    return -errno;

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


char** uv_setup_args(int argc, char** argv) {
  process_title = argc ? uv__strdup(argv[0]) : NULL;
  return argv;
}


int uv_set_process_title(const char* title) {
  int oid[4];

  uv__free(process_title);
  process_title = uv__strdup(title);

  oid[0] = CTL_KERN;
  oid[1] = KERN_PROC;
  oid[2] = KERN_PROC_ARGS;
  oid[3] = getpid();

  sysctl(oid,
         ARRAY_SIZE(oid),
         NULL,
         NULL,
         process_title,
         strlen(process_title) + 1);

  return 0;
}


int uv_get_process_title(char* buffer, size_t size) {
  size_t len;

  if (buffer == NULL || size == 0)
    return -EINVAL;

  if (process_title) {
    len = strlen(process_title) + 1;

    if (size < len)
      return -ENOBUFS;

    memcpy(buffer, process_title, len);
  } else {
    len = 0;
  }

  buffer[len] = '\0';

  return 0;
}


int uv_resident_set_memory(size_t* rss) {
  kvm_t *kd = NULL;
  struct kinfo_proc *kinfo = NULL;
  pid_t pid;
  int nprocs;
  size_t page_size = getpagesize();

  pid = getpid();

  kd = kvm_open(NULL, _PATH_DEVNULL, NULL, O_RDONLY, "kvm_open");
  if (kd == NULL) goto error;

  kinfo = kvm_getprocs(kd, KERN_PROC_PID, pid, &nprocs);
  if (kinfo == NULL) goto error;

#ifdef __DragonFly__
  *rss = kinfo->kp_vm_rssize * page_size;
#else
  *rss = kinfo->ki_rssize * page_size;
#endif

  kvm_close(kd);

  return 0;

error:
  if (kd) kvm_close(kd);
  return -EPERM;
}


int uv_uptime(double* uptime) {
  int r;
  struct timespec sp;
  r = clock_gettime(CLOCK_MONOTONIC, &sp);
  if (r)
    return -errno;

  *uptime = sp.tv_sec;
  return 0;
}


int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  unsigned int ticks = (unsigned int)sysconf(_SC_CLK_TCK),
               multiplier = ((uint64_t)1000L / ticks), cpuspeed, maxcpus,
               cur = 0;
  uv_cpu_info_t* cpu_info;
  const char* maxcpus_key;
  const char* cptimes_key;
  char model[512];
  long* cp_times;
  int numcpus;
  size_t size;
  int i;

#if defined(__DragonFly__)
  /* This is not quite correct but DragonFlyBSD doesn't seem to have anything
   * comparable to kern.smp.maxcpus or kern.cp_times (kern.cp_time is a total,
   * not per CPU). At least this stops uv_cpu_info() from failing completely.
   */
  maxcpus_key = "hw.ncpu";
  cptimes_key = "kern.cp_time";
#else
  maxcpus_key = "kern.smp.maxcpus";
  cptimes_key = "kern.cp_times";
#endif

  size = sizeof(model);
  if (sysctlbyname("hw.model", &model, &size, NULL, 0))
    return -errno;

  size = sizeof(numcpus);
  if (sysctlbyname("hw.ncpu", &numcpus, &size, NULL, 0))
    return -errno;

  *cpu_infos = uv__malloc(numcpus * sizeof(**cpu_infos));
  if (!(*cpu_infos))
    return -ENOMEM;

  *count = numcpus;

  size = sizeof(cpuspeed);
  if (sysctlbyname("hw.clockrate", &cpuspeed, &size, NULL, 0)) {
    uv__free(*cpu_infos);
    return -errno;
  }

  /* kern.cp_times on FreeBSD i386 gives an array up to maxcpus instead of
   * ncpu.
   */
  size = sizeof(maxcpus);
  if (sysctlbyname(maxcpus_key, &maxcpus, &size, NULL, 0)) {
    uv__free(*cpu_infos);
    return -errno;
  }

  size = maxcpus * CPUSTATES * sizeof(long);

  cp_times = uv__malloc(size);
  if (cp_times == NULL) {
    uv__free(*cpu_infos);
    return -ENOMEM;
  }

  if (sysctlbyname(cptimes_key, cp_times, &size, NULL, 0)) {
    uv__free(cp_times);
    uv__free(*cpu_infos);
    return -errno;
  }

  for (i = 0; i < numcpus; i++) {
    cpu_info = &(*cpu_infos)[i];

    cpu_info->cpu_times.user = (uint64_t)(cp_times[CP_USER+cur]) * multiplier;
    cpu_info->cpu_times.nice = (uint64_t)(cp_times[CP_NICE+cur]) * multiplier;
    cpu_info->cpu_times.sys = (uint64_t)(cp_times[CP_SYS+cur]) * multiplier;
    cpu_info->cpu_times.idle = (uint64_t)(cp_times[CP_IDLE+cur]) * multiplier;
    cpu_info->cpu_times.irq = (uint64_t)(cp_times[CP_INTR+cur]) * multiplier;

    cpu_info->model = uv__strdup(model);
    cpu_info->speed = cpuspeed;

    cur+=CPUSTATES;
  }

  uv__free(cp_times);
  return 0;
}


void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  int i;

  for (i = 0; i < count; i++) {
    uv__free(cpu_infos[i].model);
  }

  uv__free(cpu_infos);
}
