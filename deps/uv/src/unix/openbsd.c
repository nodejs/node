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

#include <ifaddrs.h>
#include <net/if.h>
#include <net/if_dl.h>

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
  int err;

  if (buffer == NULL || size == NULL || *size == 0)
    return -EINVAL;

  mypid = getpid();
  for (;;) {
    err = -ENOMEM;
    argsbuf_tmp = uv__realloc(argsbuf, argsbuf_size);
    if (argsbuf_tmp == NULL)
      goto out;
    argsbuf = argsbuf_tmp;
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC_ARGS;
    mib[2] = mypid;
    mib[3] = KERN_PROC_ARGV;
    if (sysctl(mib, 4, argsbuf, &argsbuf_size, NULL, 0) == 0) {
      break;
    }
    if (errno != ENOMEM) {
      err = -errno;
      goto out;
    }
    argsbuf_size *= 2U;
  }

  if (argsbuf[0] == NULL) {
    err = -EINVAL;  /* FIXME(bnoordhuis) More appropriate error. */
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

  if (sysctl(which, 2, &info, &size, NULL, 0))
    return -errno;

  return (uint64_t) info.free * sysconf(_SC_PAGESIZE);
}


uint64_t uv_get_total_memory(void) {
  uint64_t info;
  int which[] = {CTL_HW, HW_PHYSMEM64};
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
  uv__free(process_title);
  process_title = uv__strdup(title);
  setproctitle(title);
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

  if (sysctl(mib, 6, &kinfo, &size, NULL, 0) < 0)
    return -errno;

  *rss = kinfo.p_vm_rssize * page_size;
  return 0;
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
               multiplier = ((uint64_t)1000L / ticks), cpuspeed;
  uint64_t info[CPUSTATES];
  char model[512];
  int numcpus = 1;
  int which[] = {CTL_HW,HW_MODEL,0};
  size_t size;
  int i;
  uv_cpu_info_t* cpu_info;

  size = sizeof(model);
  if (sysctl(which, 2, &model, &size, NULL, 0))
    return -errno;

  which[1] = HW_NCPU;
  size = sizeof(numcpus);
  if (sysctl(which, 2, &numcpus, &size, NULL, 0))
    return -errno;

  *cpu_infos = uv__malloc(numcpus * sizeof(**cpu_infos));
  if (!(*cpu_infos))
    return -ENOMEM;

  *count = numcpus;

  which[1] = HW_CPUSPEED;
  size = sizeof(cpuspeed);
  if (sysctl(which, 2, &cpuspeed, &size, NULL, 0)) {
    uv__free(*cpu_infos);
    return -errno;
  }

  size = sizeof(info);
  which[0] = CTL_KERN;
  which[1] = KERN_CPTIME2;
  for (i = 0; i < numcpus; i++) {
    which[2] = i;
    size = sizeof(info);
    if (sysctl(which, 3, &info, &size, NULL, 0)) {
      uv__free(*cpu_infos);
      return -errno;
    }

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
}


void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count) {
  int i;

  for (i = 0; i < count; i++) {
    uv__free(cpu_infos[i].model);
  }

  uv__free(cpu_infos);
}


int uv_interface_addresses(uv_interface_address_t** addresses,
  int* count) {
  struct ifaddrs *addrs, *ent;
  uv_interface_address_t* address;
  int i;
  struct sockaddr_dl *sa_addr;

  if (getifaddrs(&addrs) != 0)
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


void uv_free_interface_addresses(uv_interface_address_t* addresses,
  int count) {
  int i;

  for (i = 0; i < count; i++) {
    uv__free(addresses[i].name);
  }

  uv__free(addresses);
}
