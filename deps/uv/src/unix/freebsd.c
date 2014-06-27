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

#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>

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

#undef NANOSEC
#define NANOSEC ((uint64_t) 1e9)

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


int uv__platform_loop_init(uv_loop_t* loop, int default_loop) {
  return uv__kqueue_init(loop);
}


void uv__platform_loop_delete(uv_loop_t* loop) {
}


uint64_t uv__hrtime(uv_clocktype_t type) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (((uint64_t) ts.tv_sec) * NANOSEC + ts.tv_nsec);
}


int uv_exepath(char* buffer, size_t* size) {
  int mib[4];
  size_t cb;

  if (buffer == NULL || size == NULL)
    return -EINVAL;

#ifdef __DragonFly__
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_ARGS;
  mib[3] = getpid();
#else
  mib[0] = CTL_KERN;
  mib[1] = KERN_PROC;
  mib[2] = KERN_PROC_PATHNAME;
  mib[3] = -1;
#endif

  cb = *size;
  if (sysctl(mib, 4, buffer, &cb, NULL, 0))
    return -errno;
  *size = strlen(buffer);

  return 0;
}


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
  process_title = argc ? strdup(argv[0]) : NULL;
  return argv;
}


int uv_set_process_title(const char* title) {
  int oid[4];

  if (process_title) free(process_title);
  process_title = strdup(title);

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
  if (process_title) {
    strncpy(buffer, process_title, size);
  } else {
    if (size > 0) {
      buffer[0] = '\0';
    }
  }

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

  *cpu_infos = malloc(numcpus * sizeof(**cpu_infos));
  if (!(*cpu_infos))
    return -ENOMEM;

  *count = numcpus;

  size = sizeof(cpuspeed);
  if (sysctlbyname("hw.clockrate", &cpuspeed, &size, NULL, 0)) {
    SAVE_ERRNO(free(*cpu_infos));
    return -errno;
  }

  /* kern.cp_times on FreeBSD i386 gives an array up to maxcpus instead of
   * ncpu.
   */
  size = sizeof(maxcpus);
  if (sysctlbyname(maxcpus_key, &maxcpus, &size, NULL, 0)) {
    SAVE_ERRNO(free(*cpu_infos));
    return -errno;
  }

  size = maxcpus * CPUSTATES * sizeof(long);

  cp_times = malloc(size);
  if (cp_times == NULL) {
    free(*cpu_infos);
    return -ENOMEM;
  }

  if (sysctlbyname(cptimes_key, cp_times, &size, NULL, 0)) {
    SAVE_ERRNO(free(cp_times));
    SAVE_ERRNO(free(*cpu_infos));
    return -errno;
  }

  for (i = 0; i < numcpus; i++) {
    cpu_info = &(*cpu_infos)[i];

    cpu_info->cpu_times.user = (uint64_t)(cp_times[CP_USER+cur]) * multiplier;
    cpu_info->cpu_times.nice = (uint64_t)(cp_times[CP_NICE+cur]) * multiplier;
    cpu_info->cpu_times.sys = (uint64_t)(cp_times[CP_SYS+cur]) * multiplier;
    cpu_info->cpu_times.idle = (uint64_t)(cp_times[CP_IDLE+cur]) * multiplier;
    cpu_info->cpu_times.irq = (uint64_t)(cp_times[CP_INTR+cur]) * multiplier;

    cpu_info->model = strdup(model);
    cpu_info->speed = cpuspeed;

    cur+=CPUSTATES;
  }

  free(cp_times);
  return 0;
}


void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  int i;

  for (i = 0; i < count; i++) {
    free(cpu_infos[i].model);
  }

  free(cpu_infos);
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
        (ent->ifa_addr->sa_family == AF_LINK)) {
      continue;
    }

    (*count)++;
  }

  *addresses = malloc(*count * sizeof(**addresses));
  if (!(*addresses))
    return -ENOMEM;

  address = *addresses;

  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    if (!((ent->ifa_flags & IFF_UP) && (ent->ifa_flags & IFF_RUNNING)))
      continue;

    if (ent->ifa_addr == NULL)
      continue;

    /*
     * On FreeBSD getifaddrs returns information related to the raw underlying
     * devices. We're not interested in this information yet.
     */
    if (ent->ifa_addr->sa_family == AF_LINK)
      continue;

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


void uv_free_interface_addresses(uv_interface_address_t* addresses,
  int count) {
  int i;

  for (i = 0; i < count; i++) {
    free(addresses[i].name);
  }

  free(addresses);
}
