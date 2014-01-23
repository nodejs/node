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
#include <kvm.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#undef NANOSEC
#define NANOSEC ((uint64_t) 1e9)


static char *process_title;


int uv__platform_loop_init(uv_loop_t* loop, int default_loop) {
  return uv__kqueue_init(loop);
}


void uv__platform_loop_delete(uv_loop_t* loop) {
}


uint64_t uv__hrtime(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (((uint64_t) ts.tv_sec) * NANOSEC + ts.tv_nsec);
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


int uv_exepath(char* buffer, size_t* size) {
  int mib[4];
  char **argsbuf = NULL;
  char **argsbuf_tmp;
  size_t argsbuf_size = 100U;
  size_t exepath_size;
  pid_t mypid;
  int status = -1;

  if (!buffer || !size) {
    goto out;
  }
  mypid = getpid();
  for (;;) {
    if ((argsbuf_tmp = realloc(argsbuf, argsbuf_size)) == NULL) {
      goto out;
    }
    argsbuf = argsbuf_tmp;
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC_ARGS;
    mib[2] = mypid;
    mib[3] = KERN_PROC_ARGV;
    if (sysctl(mib, 4, argsbuf, &argsbuf_size, NULL, 0) == 0) {
      break;
    }
    if (errno != ENOMEM) {
      goto out;
    }
    argsbuf_size *= 2U;
  }
  if (argsbuf[0] == NULL) {
    goto out;
  }
  exepath_size = strlen(argsbuf[0]);
  if (exepath_size >= *size) {
    goto out;
  }
  memcpy(buffer, argsbuf[0], exepath_size + 1U);
  *size = exepath_size;
  status = 0;

out:
  free(argsbuf);

  return status;
}


uint64_t uv_get_free_memory(void) {
  struct uvmexp info;
  size_t size = sizeof(info);
  int which[] = {CTL_VM, VM_UVMEXP};

  if (sysctl(which, 2, &info, &size, NULL, 0) < 0) {
    return -1;
  }

  return (uint64_t) info.free * sysconf(_SC_PAGESIZE);
}


uint64_t uv_get_total_memory(void) {
  uint64_t info;
  int which[] = {CTL_HW, HW_PHYSMEM64};
  size_t size = sizeof(info);

  if (sysctl(which, 2, &info, &size, NULL, 0) < 0) {
    return -1;
  }

  return (uint64_t) info;
}


char** uv_setup_args(int argc, char** argv) {
  process_title = argc ? strdup(argv[0]) : NULL;
  return argv;
}


uv_err_t uv_set_process_title(const char* title) {
  if (process_title) free(process_title);
  process_title = strdup(title);
  setproctitle(title);
  return uv_ok_;
}


uv_err_t uv_get_process_title(char* buffer, size_t size) {
  if (process_title) {
    strncpy(buffer, process_title, size);
  } else {
    if (size > 0) {
      buffer[0] = '\0';
    }
  }

  return uv_ok_;
}


uv_err_t uv_resident_set_memory(size_t* rss) {
  kvm_t *kd = NULL;
  struct kinfo_proc *kinfo = NULL;
  pid_t pid;
  int nprocs, max_size = sizeof(struct kinfo_proc);
  size_t page_size = getpagesize();

  pid = getpid();

  kd = kvm_open(NULL, _PATH_MEM, NULL, O_RDONLY, "kvm_open");
  if (kd == NULL) goto error;

  kinfo = kvm_getprocs(kd, KERN_PROC_PID, pid, max_size, &nprocs);
  if (kinfo == NULL) goto error;

  *rss = kinfo->p_vm_rssize * page_size;

  kvm_close(kd);

  return uv_ok_;

error:
  if (kd) kvm_close(kd);
  return uv__new_sys_error(errno);
}


uv_err_t uv_uptime(double* uptime) {
  time_t now;
  struct timeval info;
  size_t size = sizeof(info);
  static int which[] = {CTL_KERN, KERN_BOOTTIME};

  if (sysctl(which, 2, &info, &size, NULL, 0) < 0) {
    return uv__new_sys_error(errno);
  }

  now = time(NULL);

  *uptime = (double)(now - info.tv_sec);
  return uv_ok_;
}


uv_err_t uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  unsigned int ticks = (unsigned int)sysconf(_SC_CLK_TCK),
               multiplier = ((uint64_t)1000L / ticks), cpuspeed;
  uint64_t info[CPUSTATES];
  char model[512];
  int numcpus = 1;
  int which[] = {CTL_HW,HW_MODEL,0};
  size_t size;
  int i;
  uv_cpu_info_t* cpu_info;

  size = sizeof(model);
  if (sysctl(which, 2, &model, &size, NULL, 0) < 0) {
    return uv__new_sys_error(errno);
  }
  which[1] = HW_NCPU;
  size = sizeof(numcpus);
  if (sysctl(which, 2, &numcpus, &size, NULL, 0) < 0) {
    return uv__new_sys_error(errno);
  }

  *cpu_infos = (uv_cpu_info_t*)malloc(numcpus * sizeof(uv_cpu_info_t));
  if (!(*cpu_infos)) {
    return uv__new_artificial_error(UV_ENOMEM);
  }

  *count = numcpus;

  which[1] = HW_CPUSPEED;
  size = sizeof(cpuspeed);
  if (sysctl(which, 2, &cpuspeed, &size, NULL, 0) < 0) {
    free(*cpu_infos);
    return uv__new_sys_error(errno);
  }

  size = sizeof(info);
  which[0] = CTL_KERN;
  which[1] = KERN_CPTIME2;
  for (i = 0; i < numcpus; i++) {
    which[2] = i;
    size = sizeof(info);
    if (sysctl(which, 3, &info, &size, NULL, 0) < 0) {
      free(*cpu_infos);
      return uv__new_sys_error(errno);
    }

    cpu_info = &(*cpu_infos)[i];
    
    cpu_info->cpu_times.user = (uint64_t)(info[CP_USER]) * multiplier;
    cpu_info->cpu_times.nice = (uint64_t)(info[CP_NICE]) * multiplier;
    cpu_info->cpu_times.sys = (uint64_t)(info[CP_SYS]) * multiplier;
    cpu_info->cpu_times.idle = (uint64_t)(info[CP_IDLE]) * multiplier;
    cpu_info->cpu_times.irq = (uint64_t)(info[CP_INTR]) * multiplier;

    cpu_info->model = strdup(model);
    cpu_info->speed = cpuspeed;
  }

  return uv_ok_;
}


void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  int i;

  for (i = 0; i < count; i++) {
    free(cpu_infos[i].model);
  }

  free(cpu_infos);
}


uv_err_t uv_interface_addresses(uv_interface_address_t** addresses,
  int* count) {
  /* TODO: implement */
  *addresses = NULL;
  *count = 0;
  return uv_ok_;
}


void uv_free_interface_addresses(uv_interface_address_t* addresses,
  int count) {
}
