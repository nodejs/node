// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CPU specific code for arm independent of OS goes here.

#include <sys/syscall.h>
#include <unistd.h>

#ifdef __mips
#include <asm/cachectl.h>
#endif  // #ifdef __mips

#if V8_TARGET_ARCH_MIPS64

#include "src/cpu-features.h"

namespace v8 {
namespace internal {


void CpuFeatures::FlushICache(void* start, size_t size) {
#if !defined(USE_SIMULATOR)
  // Nothing to do, flushing no instructions.
  if (size == 0) {
    return;
  }

#if defined(ANDROID) && !defined(__LP64__)
  // Bionic cacheflush can typically run in userland, avoiding kernel call.
  char *end = reinterpret_cast<char *>(start) + size;
  cacheflush(
    reinterpret_cast<intptr_t>(start), reinterpret_cast<intptr_t>(end), 0);
#else  // ANDROID
  long res;  // NOLINT(runtime/int)
  // See http://www.linux-mips.org/wiki/Cacheflush_Syscall.
  res = syscall(__NR_cacheflush, start, size, ICACHE);
  if (res) FATAL("Failed to flush the instruction cache");
#endif  // ANDROID
#endif  // !USE_SIMULATOR.
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_MIPS64
