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

#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach-o/dyld.h> /* _NSGetExecutablePath */
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <unistd.h>  /* sysconf */

#include "darwin-stub.h"

static uv_once_t once = UV_ONCE_INIT;
static uint64_t (*time_func)(void);
static mach_timebase_info_data_t timebase;

typedef unsigned char UInt8;

int uv__platform_loop_init(uv_loop_t* loop) {
  loop->cf_state = NULL;

  if (uv__kqueue_init(loop))
    return UV__ERR(errno);

  return 0;
}


void uv__platform_loop_delete(uv_loop_t* loop) {
  uv__fsevents_loop_delete(loop);
}


static void uv__hrtime_init_once(void) {
  if (KERN_SUCCESS != mach_timebase_info(&timebase))
    abort();

  time_func = (uint64_t (*)(void)) dlsym(RTLD_DEFAULT, "mach_continuous_time");
  if (time_func == NULL)
    time_func = mach_absolute_time;
}


uint64_t uv__hrtime(uv_clocktype_t type) {
  uv_once(&once, uv__hrtime_init_once);
  return time_func() * timebase.numer / timebase.denom;
}


int uv_exepath(char* buffer, size_t* size) {
  /* realpath(exepath) may be > PATH_MAX so double it to be on the safe side. */
  char abspath[PATH_MAX * 2 + 1];
  char exepath[PATH_MAX + 1];
  uint32_t exepath_size;
  size_t abspath_size;

  if (buffer == NULL || size == NULL || *size == 0)
    return UV_EINVAL;

  exepath_size = sizeof(exepath);
  if (_NSGetExecutablePath(exepath, &exepath_size))
    return UV_EIO;

  if (realpath(exepath, abspath) != abspath)
    return UV__ERR(errno);

  abspath_size = strlen(abspath);
  if (abspath_size == 0)
    return UV_EIO;

  *size -= 1;
  if (*size > abspath_size)
    *size = abspath_size;

  memcpy(buffer, abspath, *size);
  buffer[*size] = '\0';

  return 0;
}


uint64_t uv_get_free_memory(void) {
  vm_statistics_data_t info;
  mach_msg_type_number_t count = sizeof(info) / sizeof(integer_t);

  if (host_statistics(mach_host_self(), HOST_VM_INFO,
                      (host_info_t)&info, &count) != KERN_SUCCESS) {
    return UV_EINVAL;  /* FIXME(bnoordhuis) Translate error. */
  }

  return (uint64_t) info.free_count * sysconf(_SC_PAGESIZE);
}


uint64_t uv_get_total_memory(void) {
  uint64_t info;
  int which[] = {CTL_HW, HW_MEMSIZE};
  size_t size = sizeof(info);

  if (sysctl(which, ARRAY_SIZE(which), &info, &size, NULL, 0))
    return UV__ERR(errno);

  return (uint64_t) info;
}


uint64_t uv_get_constrained_memory(void) {
  return 0;  /* Memory constraints are unknown. */
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


int uv_resident_set_memory(size_t* rss) {
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
  *uptime = now - info.tv_sec;

  return 0;
}

static int uv__get_cpu_speed(uint64_t* speed) {
  /* IOKit */
  void (*pIOObjectRelease)(io_object_t);
  kern_return_t (*pIOMasterPort)(mach_port_t, mach_port_t*);
  CFMutableDictionaryRef (*pIOServiceMatching)(const char*);
  kern_return_t (*pIOServiceGetMatchingServices)(mach_port_t,
                                                 CFMutableDictionaryRef,
                                                 io_iterator_t*);
  io_service_t (*pIOIteratorNext)(io_iterator_t);
  CFTypeRef (*pIORegistryEntryCreateCFProperty)(io_registry_entry_t,
                                                CFStringRef,
                                                CFAllocatorRef,
                                                IOOptionBits);

  /* CoreFoundation */
  CFStringRef (*pCFStringCreateWithCString)(CFAllocatorRef,
                                            const char*,
                                            CFStringEncoding);
  CFStringEncoding (*pCFStringGetSystemEncoding)(void);
  UInt8 *(*pCFDataGetBytePtr)(CFDataRef);
  CFIndex (*pCFDataGetLength)(CFDataRef);
  void (*pCFDataGetBytes)(CFDataRef, CFRange, UInt8*);
  void (*pCFRelease)(CFTypeRef);

  void* core_foundation_handle;
  void* iokit_handle;
  int err;

  kern_return_t kr;
  mach_port_t mach_port;
  io_iterator_t it;
  io_object_t service;

  mach_port = 0;

  err = UV_ENOENT;
  core_foundation_handle = dlopen("/System/Library/Frameworks/"
                                  "CoreFoundation.framework/"
                                  "CoreFoundation",
                                  RTLD_LAZY | RTLD_LOCAL);
  iokit_handle = dlopen("/System/Library/Frameworks/IOKit.framework/"
                        "IOKit",
                        RTLD_LAZY | RTLD_LOCAL);

  if (core_foundation_handle == NULL || iokit_handle == NULL)
    goto out;

#define V(handle, symbol)                                                     \
  do {                                                                        \
    *(void **)(&p ## symbol) = dlsym((handle), #symbol);                      \
    if (p ## symbol == NULL)                                                  \
      goto out;                                                               \
  }                                                                           \
  while (0)
  V(iokit_handle, IOMasterPort);
  V(iokit_handle, IOServiceMatching);
  V(iokit_handle, IOServiceGetMatchingServices);
  V(iokit_handle, IOIteratorNext);
  V(iokit_handle, IOObjectRelease);
  V(iokit_handle, IORegistryEntryCreateCFProperty);
  V(core_foundation_handle, CFStringCreateWithCString);
  V(core_foundation_handle, CFStringGetSystemEncoding);
  V(core_foundation_handle, CFDataGetBytePtr);
  V(core_foundation_handle, CFDataGetLength);
  V(core_foundation_handle, CFDataGetBytes);
  V(core_foundation_handle, CFRelease);
#undef V

#define S(s) pCFStringCreateWithCString(NULL, (s), kCFStringEncodingUTF8)

  kr = pIOMasterPort(MACH_PORT_NULL, &mach_port);
  assert(kr == KERN_SUCCESS);
  CFMutableDictionaryRef classes_to_match
      = pIOServiceMatching("IOPlatformDevice");
  kr = pIOServiceGetMatchingServices(mach_port, classes_to_match, &it);
  assert(kr == KERN_SUCCESS);
  service = pIOIteratorNext(it);

  CFStringRef device_type_str = S("device_type");
  CFStringRef clock_frequency_str = S("clock-frequency");

  while (service != 0) {
    CFDataRef data;
    data = pIORegistryEntryCreateCFProperty(service,
                                            device_type_str,
                                            NULL,
                                            0);
    if (data) {
      const UInt8* raw = pCFDataGetBytePtr(data);
      if (strncmp((char*)raw, "cpu", 3) == 0 ||
          strncmp((char*)raw, "processor", 9) == 0) {
        CFDataRef freq_ref;
        freq_ref = pIORegistryEntryCreateCFProperty(service,
                                                    clock_frequency_str,
                                                    NULL,
                                                    0);
        if (freq_ref) {
          const UInt8* freq_ref_ptr = pCFDataGetBytePtr(freq_ref);
          CFIndex len = pCFDataGetLength(freq_ref);
          if (len == 8)
            memcpy(speed, freq_ref_ptr, 8);
          else if (len == 4) {
            uint32_t v;
            memcpy(&v, freq_ref_ptr, 4);
            *speed = v;
          } else {
            *speed = 0;
          }

          pCFRelease(freq_ref);
          pCFRelease(data);
          break;
        }
      }
      pCFRelease(data);
    }

    service = pIOIteratorNext(it);
  }

  pIOObjectRelease(it);

  err = 0;

  if (device_type_str != NULL)
    pCFRelease(device_type_str);
  if (clock_frequency_str != NULL)
    pCFRelease(clock_frequency_str);

out:
  if (core_foundation_handle != NULL)
    dlclose(core_foundation_handle);

  if (iokit_handle != NULL)
    dlclose(iokit_handle);

  mach_port_deallocate(mach_task_self(), mach_port);

  return err;
}

int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count) {
  unsigned int ticks = (unsigned int)sysconf(_SC_CLK_TCK),
               multiplier = ((uint64_t)1000L / ticks);
  char model[512];
  size_t size;
  unsigned int i;
  natural_t numcpus;
  mach_msg_type_number_t msg_type;
  processor_cpu_load_info_data_t *info;
  uv_cpu_info_t* cpu_info;
  uint64_t cpuspeed;
  int err;

  size = sizeof(model);
  if (sysctlbyname("machdep.cpu.brand_string", &model, &size, NULL, 0) &&
      sysctlbyname("hw.model", &model, &size, NULL, 0)) {
    return UV__ERR(errno);
  }

  err = uv__get_cpu_speed(&cpuspeed);
  if (err < 0)
    return err;

  if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &numcpus,
                          (processor_info_array_t*)&info,
                          &msg_type) != KERN_SUCCESS) {
    return UV_EINVAL;  /* FIXME(bnoordhuis) Translate error. */
  }

  *cpu_infos = uv__malloc(numcpus * sizeof(**cpu_infos));
  if (!(*cpu_infos)) {
    vm_deallocate(mach_task_self(), (vm_address_t)info, msg_type);
    return UV_ENOMEM;
  }

  *count = numcpus;

  for (i = 0; i < numcpus; i++) {
    cpu_info = &(*cpu_infos)[i];

    cpu_info->cpu_times.user = (uint64_t)(info[i].cpu_ticks[0]) * multiplier;
    cpu_info->cpu_times.nice = (uint64_t)(info[i].cpu_ticks[3]) * multiplier;
    cpu_info->cpu_times.sys = (uint64_t)(info[i].cpu_ticks[1]) * multiplier;
    cpu_info->cpu_times.idle = (uint64_t)(info[i].cpu_ticks[2]) * multiplier;
    cpu_info->cpu_times.irq = 0;

    cpu_info->model = uv__strdup(model);
    cpu_info->speed = cpuspeed/1000000;
  }
  vm_deallocate(mach_task_self(), (vm_address_t)info, msg_type);

  return 0;
}
