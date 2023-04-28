// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CPU specific code for ppc independent of OS goes here.

#if V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64

#include "src/codegen/cpu-features.h"

#define INSTR_AND_DATA_CACHE_COHERENCY PPC_6_PLUS

namespace v8 {
namespace internal {

void CpuFeatures::FlushICache(void* buffer, size_t size) {
#if !defined(USE_SIMULATOR)
  if (CpuFeatures::IsSupported(INSTR_AND_DATA_CACHE_COHERENCY)) {
    __asm__ __volatile__(
        "sync \n"
        "icbi 0, %0  \n"
        "isync  \n"
        : /* no output */
        : "r"(buffer)
        : "memory");
    return;
  }

  const int kCacheLineSize = CpuFeatures::icache_line_size();
  intptr_t mask = kCacheLineSize - 1;
  byte* start =
      reinterpret_cast<byte*>(reinterpret_cast<intptr_t>(buffer) & ~mask);
  byte* end = static_cast<byte*>(buffer) + size;
  for (byte* pointer = start; pointer < end; pointer += kCacheLineSize) {
    __asm__(
        "dcbf 0, %0  \n"
        "sync        \n"
        "icbi 0, %0  \n"
        "isync       \n"
        : /* no output */
        : "r"(pointer));
  }

#endif  // !USE_SIMULATOR
}
}  // namespace internal
}  // namespace v8

#undef INSTR_AND_DATA_CACHE_COHERENCY
#endif  // V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64
