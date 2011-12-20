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
#include <stdint.h>
#include <errno.h>

#include <ifaddrs.h>
#include <net/if.h>

#include <TargetConditionals.h>

#if !TARGET_OS_IPHONE
#include <CoreServices/CoreServices.h>
#endif

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach-o/dyld.h> /* _NSGetExecutablePath */
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <unistd.h>  /* sysconf */

static char *process_title;

#if TARGET_OS_IPHONE
/* see: http://developer.apple.com/library/mac/#qa/qa1398/_index.html */
uint64_t uv_hrtime() {
    uint64_t time;
    uint64_t enano;
    static mach_timebase_info_data_t sTimebaseInfo;

    time = mach_absolute_time();

    if (0 == sTimebaseInfo.denom) {
        (void)mach_timebase_info(&sTimebaseInfo);
    }

    enano = time * sTimebaseInfo.numer / sTimebaseInfo.denom;

    return enano;
}
#else
uint64_t uv_hrtime() {
  uint64_t time;
  Nanoseconds enano;
  time = mach_absolute_time(); 
  enano = AbsoluteToNanoseconds(*(AbsoluteTime *)&time);
  return (*(uint64_t *)&enano);
}
#endif

int uv_exepath(char* buffer, size_t* size) {
  uint32_t usize;
  int result;
  char* path;
  char* fullpath;

  if (!buffer || !size) {
    return -1;
  }

  usize = *size;
  result = _NSGetExecutablePath(buffer, &usize);
  if (result) return result;

  path = (char*)malloc(2 * PATH_MAX);
  fullpath = realpath(buffer, path);

  if (fullpath == NULL) {
    free(path);
    return -1;
  }

  strncpy(buffer, fullpath, *size);
  free(fullpath);
  *size = strlen(buffer);
  return 0;
}


uint64_t uv_get_free_memory(void) {
  vm_statistics_data_t info;
  mach_msg_type_number_t count = sizeof(info) / sizeof(integer_t);

  if (host_statistics(mach_host_self(), HOST_VM_INFO,
                      (host_info_t)&info, &count) != KERN_SUCCESS) {
    return -1;
  }

  return (uint64_t) info.free_count * sysconf(_SC_PAGESIZE);
}


uint64_t uv_get_total_memory(void) {
  uint64_t info;
  int which[] = {CTL_HW, HW_MEMSIZE};
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


char** uv_setup_args(int argc, char** argv) {
  process_title = argc ? strdup(argv[0]) : NULL;
  return argv;
}


uv_err_t uv_set_process_title(const char* title) {
  /* TODO implement me */
  return uv__new_artificial_error(UV_ENOSYS);
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
  struct task_basic_info t_info;
  mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

  int r = task_info(mach_task_self(),
                    TASK_BASIC_INFO,
                    (task_info_t)&t_info,
                    &t_info_count);

  if (r != KERN_SUCCESS) {
    return uv__new_sys_error(errno);
  }

  *rss = t_info.resident_size;

  return uv_ok_;
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
               multiplier = ((uint64_t)1000L / ticks);
  char model[512];
  uint64_t cpuspeed;
  size_t size;
  unsigned int i;
  natural_t numcpus;
  mach_msg_type_number_t msg_type;
  processor_cpu_load_info_data_t *info;
  uv_cpu_info_t* cpu_info;

  size = sizeof(model);
  if (sysctlbyname("hw.model", &model, &size, NULL, 0) < 0) {
    return uv__new_sys_error(errno);
  }
  size = sizeof(cpuspeed);
  if (sysctlbyname("hw.cpufrequency", &cpuspeed, &size, NULL, 0) < 0) {
    return uv__new_sys_error(errno);
  }

  if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &numcpus,
                          (processor_info_array_t*)&info,
                          &msg_type) != KERN_SUCCESS) {
    return uv__new_sys_error(errno);
  }

  *cpu_infos = (uv_cpu_info_t*)malloc(numcpus * sizeof(uv_cpu_info_t));
  if (!(*cpu_infos)) {
    return uv__new_artificial_error(UV_ENOMEM);
  }

  *count = numcpus;

  for (i = 0; i < numcpus; i++) {
    cpu_info = &(*cpu_infos)[i];

    cpu_info->cpu_times.user = (uint64_t)(info[i].cpu_ticks[0]) * multiplier;
    cpu_info->cpu_times.nice = (uint64_t)(info[i].cpu_ticks[3]) * multiplier;
    cpu_info->cpu_times.sys = (uint64_t)(info[i].cpu_ticks[1]) * multiplier;
    cpu_info->cpu_times.idle = (uint64_t)(info[i].cpu_ticks[2]) * multiplier;
    cpu_info->cpu_times.irq = 0;

    cpu_info->model = strdup(model);
    cpu_info->speed = cpuspeed/1000000;
  }
  vm_deallocate(mach_task_self(), (vm_address_t)info, msg_type);

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
  struct ifaddrs *addrs, *ent;
  char ip[INET6_ADDRSTRLEN];
  uv_interface_address_t* address;

  if (getifaddrs(&addrs) != 0) {
    return uv__new_sys_error(errno);
  }

  *count = 0;

  /* Count the number of interfaces */
  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    if (!(ent->ifa_flags & IFF_UP && ent->ifa_flags & IFF_RUNNING) ||
        (ent->ifa_addr == NULL) ||
        (ent->ifa_addr->sa_family == AF_LINK)) {
      continue;
    }

    (*count)++;
  }

  *addresses = (uv_interface_address_t*)
    malloc(*count * sizeof(uv_interface_address_t));
  if (!(*addresses)) {
    return uv__new_artificial_error(UV_ENOMEM);
  }

  address = *addresses;

  for (ent = addrs; ent != NULL; ent = ent->ifa_next) {
    bzero(&ip, sizeof (ip));
    if (!(ent->ifa_flags & IFF_UP && ent->ifa_flags & IFF_RUNNING)) {
      continue;
    }

    if (ent->ifa_addr == NULL) {
      continue;
    }

    /*
     * On Mac OS X getifaddrs returns information related to Mac Addresses for
     * various devices, such as firewire, etc. These are not relevant here.
     */
    if (ent->ifa_addr->sa_family == AF_LINK) {
      continue;
    }

    address->name = strdup(ent->ifa_name);

    if (ent->ifa_addr->sa_family == AF_INET6) {
      address->address.address6 = *((struct sockaddr_in6 *)ent->ifa_addr);
    } else {
      address->address.address4 = *((struct sockaddr_in *)ent->ifa_addr);
    }

    address->is_internal = ent->ifa_flags & IFF_LOOPBACK ? 1 : 0;

    address++;
  }

  freeifaddrs(addrs);

  return uv_ok_;
}


void uv_free_interface_addresses(uv_interface_address_t* addresses,
  int count) {
  int i;

  for (i = 0; i < count; i++) {
    free(addresses[i].name);
  }

  free(addresses);
}
