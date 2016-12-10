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
#include <net/if_dl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <uvm/uvm_extern.h>

#include <unistd.h>
#include <time.h>

#undef NANOSEC
#define NANOSEC ((uint64_t) 1e9)

static char *process_title;


int uv__platform_loop_init(uv_loop_t* loop) {
  return uv__kqueue_init(loop);
}


void uv__platform_loop_delete(uv_loop_t* loop) {
}


uint64_t uv__hrtime(uv_clocktype_t type) {
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

  if (buffer == NULL || size == NULL || *size == 0)
    return -EINVAL;

  mypid = getpid();
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC_ARGS;
  mib[2] = mypid;
  mib[3] = KERN_PROC_ARGV;

  cb = *size;
  if (sysctl(mib, 4, buffer, &cb, NULL, 0))
    return -errno;
  *size = strlen(buffer);

  return 0;
}


uint64_t uv_get_free_memory(void) {
  struct uvmexp info;
  size_t size = sizeof(info);
  int which[] = {CTL_VM, VM_UVMEXP};

  if (sysctl(which, 2, &info, &size, NULL, 0))
    return -errno;

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

  if (sysctl(which, 2, &info, &size, NULL, 0))
    return -errno;

  return (uint64_t) info;
}


char** uv_setup_args(int argc, char** argv) {
  process_title = argc ? uv__strdup(argv[0]) : NULL;
  return argv;
}


int uv_set_process_title(const char* title) {
  if (process_title) uv__free(process_title);

  process_title = uv__strdup(title);
  setproctitle("%s", title);

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
  return -EPERM;
}


int uv_uptime(double* uptime) {
  time_t now;
  struct timeval info;
  size_t size = sizeof(info);
  static int which[] = {CTL_KERN, KERN_BOOTTIME};

  if (sysctl(which, 2, &info, &size, NULL, 0))
    return -errno;

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
    return -errno;
  }

  size = sizeof(numcpus);
  if (sysctlbyname("hw.ncpu", &numcpus, &size, NULL, 0))
    return -errno;
  *count = numcpus;

  /* Only i386 and amd64 have machdep.tsc_freq */
  size = sizeof(cpuspeed);
  if (sysctlbyname("machdep.tsc_freq", &cpuspeed, &size, NULL, 0))
    cpuspeed = 0;

  size = numcpus * CPUSTATES * sizeof(*cp_times);
  cp_times = uv__malloc(size);
  if (cp_times == NULL)
    return -ENOMEM;

  if (sysctlbyname("kern.cp_time", cp_times, &size, NULL, 0))
    return -errno;

  *cpu_infos = uv__malloc(numcpus * sizeof(**cpu_infos));
  if (!(*cpu_infos)) {
    uv__free(cp_times);
    uv__free(*cpu_infos);
    return -ENOMEM;
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


void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  int i;

  for (i = 0; i < count; i++) {
    uv__free(cpu_infos[i].model);
  }

  uv__free(cpu_infos);
}


int uv_interface_addresses(uv_interface_address_t** addresses, int* count) {
  struct ifaddrs *addrs, *ent;
  uv_interface_address_t* address;
  int i;
  struct sockaddr_dl *sa_addr;

  if (getifaddrs(&addrs))
    return -errno;

  *count = 0;

  /* Count the number of interfaces */
  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    if (!((ent->ifa_flags & IFF_UP) && (ent->ifa_flags & IFF_RUNNING)) ||
        (ent->ifa_addr == NULL) ||
        (ent->ifa_addr->sa_family != PF_INET)) {
      continue;
    }
    (*count)++;
  }

  *addresses = uv__malloc(*count * sizeof(**addresses));

  if (!(*addresses)) {
    freeifaddrs(addrs);
    return -ENOMEM;
  }

  address = *addresses;

  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    if (!((ent->ifa_flags & IFF_UP) && (ent->ifa_flags & IFF_RUNNING)))
      continue;

    if (ent->ifa_addr == NULL)
      continue;

    if (ent->ifa_addr->sa_family != PF_INET)
      continue;

    address->name = uv__strdup(ent->ifa_name);

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

    address->is_internal = !!(ent->ifa_flags & IFF_LOOPBACK);

    address++;
  }

  /* Fill in physical addresses for each interface */
  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    if (!((ent->ifa_flags & IFF_UP) && (ent->ifa_flags & IFF_RUNNING)) ||
        (ent->ifa_addr == NULL) ||
        (ent->ifa_addr->sa_family != AF_LINK)) {
      continue;
    }

    address = *addresses;

    for (i = 0; i < (*count); i++) {
      if (strcmp(address->name, ent->ifa_name) == 0) {
        sa_addr = (struct sockaddr_dl*)(ent->ifa_addr);
        memcpy(address->phys_addr, LLADDR(sa_addr), sizeof(address->phys_addr));
      }
      address++;
    }
  }

  freeifaddrs(addrs);

  return 0;
}


void uv_free_interface_addresses(uv_interface_address_t* addresses, int count) {
  int i;

  for (i = 0; i < count; i++) {
    uv__free(addresses[i].name);
  }

  uv__free(addresses);
}
