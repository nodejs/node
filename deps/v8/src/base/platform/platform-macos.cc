// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Platform-specific code for MacOS goes here. For the POSIX-compatible
// parts, the implementation is in platform-posix.cc.

#include <dlfcn.h>
#include <mach/mach_init.h>
#include <mach-o/dyld.h>
#include <mach-o/getsect.h>
#include <sys/mman.h>
#include <unistd.h>

#include <AvailabilityMacros.h>

#include <errno.h>
#include <libkern/OSAtomic.h>
#include <mach/mach.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#include <mach/vm_statistics.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>

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
#if V8_HOST_ARCH_X64
    uint64_t size;
    char* code_ptr = getsectdatafromheader_64(
        reinterpret_cast<const mach_header_64*>(header), SEG_TEXT, SECT_TEXT,
        &size);
#else
    unsigned int size;
    char* code_ptr = getsectdatafromheader(header, SEG_TEXT, SECT_TEXT, &size);
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

}  // namespace base
}  // namespace v8
