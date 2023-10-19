// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CPU specific code for LoongArch independent of OS goes here.

#include <sys/syscall.h>
#include <unistd.h>

#if V8_TARGET_ARCH_LOONG64

#include "src/codegen/cpu-features.h"

namespace v8 {
namespace internal {

void CpuFeatures::FlushICache(void* start, size_t size) {
#if defined(V8_HOST_ARCH_LOONG64)
  // Nothing to do, flushing no instructions.
  if (size == 0) {
    return;
  }

#if defined(ANDROID) && !defined(__LP64__)
  // Bionic cacheflush can typically run in userland, avoiding kernel call.
  char* end = reinterpret_cast<char*>(start) + size;
  cacheflush(reinterpret_cast<intptr_t>(start), reinterpret_cast<intptr_t>(end),
             0);
#else   // ANDROID
  asm("ibar 0\n");
#endif  // ANDROID
#endif  // V8_HOST_ARCH_LOONG64
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_LOONG64
