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

#include <CoreFoundation/CFRunLoop.h>

#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach-o/dyld.h> /* _NSGetExecutablePath */
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <unistd.h>  /* sysconf */

/* Forward declarations */
static void uv__cf_loop_runner(void* arg);
static void uv__cf_loop_cb(void* arg);

typedef struct uv__cf_loop_signal_s uv__cf_loop_signal_t;
struct uv__cf_loop_signal_s {
  void* arg;
  cf_loop_signal_cb cb;
  QUEUE member;
};


int uv__platform_loop_init(uv_loop_t* loop, int default_loop) {
  CFRunLoopSourceContext ctx;
  int r;

  if (uv__kqueue_init(loop))
    return -1;

  loop->cf_loop = NULL;
  if ((r = uv_mutex_init(&loop->cf_mutex)))
    return r;
  if ((r = uv_sem_init(&loop->cf_sem, 0)))
    return r;
  QUEUE_INIT(&loop->cf_signals);

  memset(&ctx, 0, sizeof(ctx));
  ctx.info = loop;
  ctx.perform = uv__cf_loop_cb;
  loop->cf_cb = CFRunLoopSourceCreate(NULL, 0, &ctx);

  if ((r = uv_thread_create(&loop->cf_thread, uv__cf_loop_runner, loop)))
    return r;

  /* Synchronize threads */
  uv_sem_wait(&loop->cf_sem);
  assert(ACCESS_ONCE(CFRunLoopRef, loop->cf_loop) != NULL);

  return 0;
}


void uv__platform_loop_delete(uv_loop_t* loop) {
  QUEUE* item;
  uv__cf_loop_signal_t* s;

  assert(loop->cf_loop != NULL);
  uv__cf_loop_signal(loop, NULL, NULL);
  uv_thread_join(&loop->cf_thread);

  uv_sem_destroy(&loop->cf_sem);
  uv_mutex_destroy(&loop->cf_mutex);

  /* Free any remaining data */
  while (!QUEUE_EMPTY(&loop->cf_signals)) {
    item = QUEUE_HEAD(&loop->cf_signals);

    s = QUEUE_DATA(item, uv__cf_loop_signal_t, member);

    QUEUE_REMOVE(item);
    free(s);
  }
}


static void uv__cf_loop_runner(void* arg) {
  uv_loop_t* loop;

  loop = arg;

  /* Get thread's loop */
  ACCESS_ONCE(CFRunLoopRef, loop->cf_loop) = CFRunLoopGetCurrent();

  CFRunLoopAddSource(loop->cf_loop,
                     loop->cf_cb,
                     kCFRunLoopDefaultMode);

  uv_sem_post(&loop->cf_sem);

  CFRunLoopRun();

  CFRunLoopRemoveSource(loop->cf_loop,
                        loop->cf_cb,
                        kCFRunLoopDefaultMode);
}


static void uv__cf_loop_cb(void* arg) {
  uv_loop_t* loop;
  QUEUE* item;
  QUEUE split_head;
  uv__cf_loop_signal_t* s;

  loop = arg;

  uv_mutex_lock(&loop->cf_mutex);
  QUEUE_INIT(&split_head);
  if (!QUEUE_EMPTY(&loop->cf_signals)) {
    QUEUE* split_pos = QUEUE_HEAD(&loop->cf_signals);
    QUEUE_SPLIT(&loop->cf_signals, split_pos, &split_head);
  }
  uv_mutex_unlock(&loop->cf_mutex);

  while (!QUEUE_EMPTY(&split_head)) {
    item = QUEUE_HEAD(&split_head);

    s = QUEUE_DATA(item, uv__cf_loop_signal_t, member);

    /* This was a termination signal */
    if (s->cb == NULL)
      CFRunLoopStop(loop->cf_loop);
    else
      s->cb(s->arg);

    QUEUE_REMOVE(item);
    free(s);
  }
}


void uv__cf_loop_signal(uv_loop_t* loop, cf_loop_signal_cb cb, void* arg) {
  uv__cf_loop_signal_t* item;

  item = malloc(sizeof(*item));
  /* XXX: Fail */
  if (item == NULL)
    abort();

  item->arg = arg;
  item->cb = cb;

  uv_mutex_lock(&loop->cf_mutex);
  QUEUE_INSERT_TAIL(&loop->cf_signals, &item->member);
  uv_mutex_unlock(&loop->cf_mutex);

  assert(loop->cf_loop != NULL);
  CFRunLoopSourceSignal(loop->cf_cb);
  CFRunLoopWakeUp(loop->cf_loop);
}


uint64_t uv__hrtime(void) {
    mach_timebase_info_data_t info;

    if (mach_timebase_info(&info) != KERN_SUCCESS)
      abort();

    return mach_absolute_time() * info.numer / info.denom;
}


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


uv_err_t uv_resident_set_memory(size_t* rss) {
  mach_msg_type_number_t count;
  task_basic_info_data_t info;
  kern_return_t err;

  count = TASK_BASIC_INFO_COUNT;
  err = task_info(mach_task_self(),
                  TASK_BASIC_INFO,
                  (task_info_t) &info,
                  &count);
  (void) &err;
  /* task_info(TASK_BASIC_INFO) cannot really fail. Anything other than
   * KERN_SUCCESS implies a libuv bug.
   */
  assert(err == KERN_SUCCESS);
  *rss = info.resident_size;

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
  if (sysctlbyname("machdep.cpu.brand_string", &model, &size, NULL, 0) < 0 &&
      sysctlbyname("hw.model", &model, &size, NULL, 0) < 0) {
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
      address->address.address6 = *((struct sockaddr_in6*) ent->ifa_addr);
    } else {
      address->address.address4 = *((struct sockaddr_in*) ent->ifa_addr);
    }

    if (ent->ifa_netmask->sa_family == AF_INET6) {
      address->netmask.netmask6 = *((struct sockaddr_in6*) ent->ifa_netmask);
    } else {
      address->netmask.netmask4 = *((struct sockaddr_in*) ent->ifa_netmask);
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
