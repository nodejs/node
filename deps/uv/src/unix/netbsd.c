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
#include <ifaddrs.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <fcntl.h>

#include <net/if.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include <unistd.h>
#include <time.h>

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

  if (sysctl(which, 2, &info, &size, NULL, 0) == -1) return;

  avg[0] = (double) info.ldavg[0] / info.fscale;
  avg[1] = (double) info.ldavg[1] / info.fscale;
  avg[2] = (double) info.ldavg[2] / info.fscale;
}


int uv_exepath(char* buffer, size_t* size) {
  int mib[4];
  size_t cb;
  pid_t mypid;

  if (!buffer || !size) {
    return -1;
  }

  mypid = getpid();
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC_ARGS;
  mib[2] = mypid;
  mib[3] = KERN_PROC_ARGV;

  cb = *size;
  if (sysctl(mib, 4, buffer, &cb, NULL, 0) == -1) {
    *size = 0;
    return -1;
  }
  *size = strlen(buffer);

  return 0;
}


uint64_t uv_get_free_memory(void) {
  struct uvmexp info;
  size_t size = sizeof(info);
  int which[] = {CTL_VM, VM_UVMEXP};

  if (sysctl(which, 2, &info, &size, NULL, 0) == -1) {
    return -1;
  }

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

  if (sysctl(which, 2, &info, &size, NULL, 0) == -1) {
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
  setproctitle("%s", title);

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

  if (sysctl(which, 2, &info, &size, NULL, 0) == -1) {
    return uv__new_sys_error(errno);
  }

  now = time(NULL);

  *uptime = (double)(now - info.tv_sec);
  return uv_ok_;
}


uv_err_t uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
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
  if (sysctlbyname("machdep.cpu_brand", &model, &size, NULL, 0) == -1 &&
      sysctlbyname("hw.model", &model, &size, NULL, 0) == -1) {
    return uv__new_sys_error(errno);
  }

  size = sizeof(numcpus);
  if (sysctlbyname("hw.ncpu", &numcpus, &size, NULL, 0) == -1) {
    return uv__new_sys_error(errno);
  }
  *count = numcpus;

  /* Only i386 and amd64 have machdep.tsc_freq */
  size = sizeof(cpuspeed);
  if (sysctlbyname("machdep.tsc_freq", &cpuspeed, &size, NULL, 0) == -1) {
    cpuspeed = 0;
  }

  size = numcpus * CPUSTATES * sizeof(*cp_times);
  cp_times = malloc(size);
  if (cp_times == NULL) {
    return uv__new_artificial_error(UV_ENOMEM);
  }
  if (sysctlbyname("kern.cp_time", cp_times, &size, NULL, 0) == -1) {
    return uv__new_sys_error(errno);
  }

  *cpu_infos = malloc(numcpus * sizeof(**cpu_infos));
  if (!(*cpu_infos)) {
    free(cp_times);
    free(*cpu_infos);
    return uv__new_artificial_error(UV_ENOMEM);
  }

  for (i = 0; i < numcpus; i++) {
    cpu_info = &(*cpu_infos)[i];
    cpu_info->cpu_times.user = (uint64_t)(cp_times[CP_USER+cur]) * multiplier;
    cpu_info->cpu_times.nice = (uint64_t)(cp_times[CP_NICE+cur]) * multiplier;
    cpu_info->cpu_times.sys = (uint64_t)(cp_times[CP_SYS+cur]) * multiplier;
    cpu_info->cpu_times.idle = (uint64_t)(cp_times[CP_IDLE+cur]) * multiplier;
    cpu_info->cpu_times.irq = (uint64_t)(cp_times[CP_INTR+cur]) * multiplier;
    cpu_info->model = strdup(model);
    cpu_info->speed = (int)(cpuspeed/(uint64_t) 1e6);
    cur += CPUSTATES;
  }
  free(cp_times);
  return uv_ok_;
}

void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  int i;

  for (i = 0; i < count; i++) {
    free(cpu_infos[i].model);
  }

  free(cpu_infos);
}


uv_err_t uv_interface_addresses(uv_interface_address_t** addresses, int* count) {
  struct ifaddrs *addrs;
  struct ifaddrs *ent;
  uv_interface_address_t* address;

  if (getifaddrs(&addrs) != 0) {
    return uv__new_sys_error(errno);
  }

  *count = 0;

  /* Count the number of interfaces */
  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    if (!(ent->ifa_flags & IFF_UP && ent->ifa_flags & IFF_RUNNING) ||
        (ent->ifa_addr == NULL) ||
        (ent->ifa_addr->sa_family != PF_INET)) {
      continue;
    }
    (*count)++;
  }

  *addresses = malloc(*count * sizeof(**addresses));

  if (!(*addresses)) {
    return uv__new_artificial_error(UV_ENOMEM);
  }

  address = *addresses;

  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    if (!(ent->ifa_flags & IFF_UP && ent->ifa_flags & IFF_RUNNING)) {
      continue;
    }

    if (ent->ifa_addr == NULL) {
      continue;
    }

    if (ent->ifa_addr->sa_family != PF_INET) {
      continue;
    }

    address->name = strdup(ent->ifa_name);

    if (ent->ifa_addr->sa_family == AF_INET6) {
      address->address.address6 = *((struct sockaddr_in6*) ent->ifa_addr);
    } else {
      address->address.address4 = *((struct sockaddr_in*) ent->ifa_addr);
    }

    if (ent->ifa_netmask->sa_family == AF_INET6) {
      address->netmask.netmask6 = *((struct sockaddr_in6*) ent->ifa_netmask);
    } else {
      address->netmask.netmask4 = *((struct sockaddr_in*) ent->ifa_netmask);
    }

    address->is_internal = !!(ent->ifa_flags & IFF_LOOPBACK) ? 1 : 0;

    address++;
  }

  freeifaddrs(addrs);

  return uv_ok_;
}


void uv_free_interface_addresses(uv_interface_address_t* addresses, int count) {
  int i;

  for (i = 0; i < count; i++) {
    free(addresses[i].name);
  }

  free(addresses);
}
