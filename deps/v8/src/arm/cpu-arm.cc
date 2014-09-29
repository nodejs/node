// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CPU specific code for arm independent of OS goes here.
#ifdef __arm__
#ifdef __QNXNTO__
#include <sys/mman.h>  // for cache flushing.
#undef MAP_TYPE
#else
#include <sys/syscall.h>  // for cache flushing.
#endif
#endif

#include "src/v8.h"

#if V8_TARGET_ARCH_ARM

#include "src/assembler.h"
#include "src/macro-assembler.h"
#include "src/simulator.h"  // for cache flushing.

namespace v8 {
namespace internal {


void CpuFeatures::FlushICache(void* start, size_t size) {
  if (size == 0) return;

#if defined(USE_SIMULATOR)
  // Not generating ARM instructions for C-code. This means that we are
  // building an ARM emulator based target.  We should notify the simulator
  // that the Icache was flushed.
  // None of this code ends up in the snapshot so there are no issues
  // around whether or not to generate the code when building snapshots.
  Simulator::FlushICache(Isolate::Current()->simulator_i_cache(), start, size);

#elif V8_OS_QNX
  msync(start, size, MS_SYNC | MS_INVALIDATE_ICACHE);

#else
  register uint32_t beg asm("r0") = reinterpret_cast<uint32_t>(start);
  register uint32_t end asm("r1") = beg + size;
  register uint32_t flg asm("r2") = 0;

  asm volatile(
    // This assembly works for both ARM and Thumb targets.

    // Preserve r7; it is callee-saved, and GCC uses it as a frame pointer for
    // Thumb targets.
    "  push {r7}\n"
                                  // r0 = beg
                                  // r1 = end
                                  // r2 = flags (0)
    "  ldr r7, =%c[scno]\n"       // r7 = syscall number
    "  svc 0\n"

    "  pop {r7}\n"
    :
    : "r" (beg), "r" (end), "r" (flg), [scno] "i" (__ARM_NR_cacheflush)
    : "memory");
#endif
}

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
