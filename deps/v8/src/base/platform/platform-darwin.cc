// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Platform-specific code shared between macOS and iOS goes here. The macOS
// specific part is in platform-macos.cc, the POSIX-compatible parts in
// platform-posix.cc.

#include <AvailabilityMacros.h>
#include <dlfcn.h>
#include <errno.h>
#include <libkern/OSAtomic.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <mach/mach.h>
#include <mach/mach_init.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#include <mach/vm_statistics.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <cmath>

#undef MAP_TYPE

#include "src/base/macros.h"
#include "src/base/platform/platform-posix-time.h"
#include "src/base/platform/platform-posix.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace base {

std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  std::vector<SharedLibraryAddress> result;
  unsigned int images_count = _dyld_image_count();
  for (unsigned int i = 0; i < images_count; ++i) {
    const mach_header* header = _dyld_get_image_header(i);
    if (header == nullptr) continue;
#if V8_HOST_ARCH_I32
    unsigned int size;
    char* code_ptr = getsectdatafromheader(header, SEG_TEXT, SECT_TEXT, &size);
#else
    uint64_t size;
    char* code_ptr = getsectdatafromheader_64(
        reinterpret_cast<const mach_header_64*>(header), SEG_TEXT, SECT_TEXT,
        &size);
#endif
    if (code_ptr == nullptr) continue;
    const intptr_t slide = _dyld_get_image_vmaddr_slide(i);
    const uintptr_t start = reinterpret_cast<uintptr_t>(code_ptr) + slide;
    result.push_back(SharedLibraryAddress(_dyld_get_image_name(i), start,
                                          start + size, slide));
  }
  return result;
}

void OS::SignalCodeMovingGC() {}

TimezoneCache* OS::CreateTimezoneCache() {
  return new PosixDefaultTimezoneCache();
}

void OS::AdjustSchedulingParams() {
#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32
  {
    // Check availability of scheduling params.
    uint32_t val = 0;
    size_t valSize = sizeof(val);
    int rc = sysctlbyname("kern.tcsm_available", &val, &valSize, NULL, 0);
    if (rc < 0 || !val) return;
  }

  {
    // Adjust scheduling params.
    uint32_t val = 1;
    int rc = sysctlbyname("kern.tcsm_enable", NULL, NULL, &val, sizeof(val));
    DCHECK_GE(rc, 0);
    USE(rc);
  }
#endif
}

std::vector<OS::MemoryRange> OS::GetFreeMemoryRangesWithin(
    OS::Address boundary_start, OS::Address boundary_end, size_t minimum_size,
    size_t alignment) {
  return {};
}

// static
Stack::StackSlot Stack::GetStackStart() {
  return pthread_get_stackaddr_np(pthread_self());
}

}  // namespace base
}  // namespace v8
