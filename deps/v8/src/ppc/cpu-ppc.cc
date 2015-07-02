// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CPU specific code for ppc independent of OS goes here.
#include "src/v8.h"

#if V8_TARGET_ARCH_PPC

#include "src/assembler.h"
#include "src/macro-assembler.h"
#include "src/simulator.h"  // for cache flushing.

namespace v8 {
namespace internal {

void CpuFeatures::FlushICache(void* buffer, size_t size) {
  // Nothing to do flushing no instructions.
  if (size == 0) {
    return;
  }

#if defined(USE_SIMULATOR)
  // Not generating PPC instructions for C-code. This means that we are
  // building an PPC emulator based target.  We should notify the simulator
  // that the Icache was flushed.
  // None of this code ends up in the snapshot so there are no issues
  // around whether or not to generate the code when building snapshots.
  Simulator::FlushICache(Isolate::Current()->simulator_i_cache(), buffer, size);
#else

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

  const int kCacheLineSize = CpuFeatures::cache_line_size();
  intptr_t mask = kCacheLineSize - 1;
  byte *start =
      reinterpret_cast<byte *>(reinterpret_cast<intptr_t>(buffer) & ~mask);
  byte *end = static_cast<byte *>(buffer) + size;
  for (byte *pointer = start; pointer < end; pointer += kCacheLineSize) {
    __asm__(
        "dcbf 0, %0  \n"
        "sync        \n"
        "icbi 0, %0  \n"
        "isync       \n"
        : /* no output */
        : "r"(pointer));
  }

#endif  // USE_SIMULATOR
}
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC
