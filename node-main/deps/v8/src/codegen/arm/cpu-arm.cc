// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CPU specific code for arm independent of OS goes here.
#ifdef __arm__
#ifdef __QNXNTO__
#include <sys/mman.h>  // for cache flushing.
#undef MAP_TYPE
#elif V8_OS_FREEBSD
#include <machine/sysarch.h>  // for cache flushing
#include <sys/types.h>
#elif V8_OS_STARBOARD
#define __ARM_NR_cacheflush 0x0f0002
#else
#include <sys/syscall.h>  // for cache flushing.
#endif
#endif

#if V8_TARGET_ARCH_ARM

#include "src/codegen/cpu-features.h"

namespace v8 {
namespace internal {

// The inlining of this seems to trigger an LTO bug that clobbers a register,
// see https://crbug.com/952759 and https://bugs.llvm.org/show_bug.cgi?id=41575.
V8_NOINLINE void CpuFeatures::FlushICache(void* start, size_t size) {
#if !defined(USE_SIMULATOR)
#if V8_OS_QNX
  msync(start, size, MS_SYNC | MS_INVALIDATE_ICACHE);
#elif V8_OS_FREEBSD
  struct arm_sync_icache_args args = {
      .addr = reinterpret_cast<uintptr_t>(start), .len = size};
  sysarch(ARM_SYNC_ICACHE, reinterpret_cast<void*>(&args));
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
      "  ldr r7, =%c[scno]\n"  // r7 = syscall number
      "  svc 0\n"

      "  pop {r7}\n"
      :
      : "r"(beg), "r"(end), "r"(flg), [scno] "i"(__ARM_NR_cacheflush)
      : "memory");
#endif
#endif  // !USE_SIMULATOR
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM
