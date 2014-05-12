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

#include "v8.h"

#if V8_TARGET_ARCH_ARM

#include "cpu.h"
#include "macro-assembler.h"
#include "simulator.h"  // for cache flushing.

namespace v8 {
namespace internal {

void CPU::FlushICache(void* start, size_t size) {
  // Nothing to do flushing no instructions.
  if (size == 0) {
    return;
  }

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
  // Ideally, we would call
  //   syscall(__ARM_NR_cacheflush, start,
  //           reinterpret_cast<intptr_t>(start) + size, 0);
  // however, syscall(int, ...) is not supported on all platforms, especially
  // not when using EABI, so we call the __ARM_NR_cacheflush syscall directly.

  register uint32_t beg asm("a1") = reinterpret_cast<uint32_t>(start);
  register uint32_t end asm("a2") =
      reinterpret_cast<uint32_t>(start) + size;
  register uint32_t flg asm("a3") = 0;
  #if defined (__arm__) && !defined(__thumb__)
    // __arm__ may be defined in thumb mode.
    register uint32_t scno asm("r7") = __ARM_NR_cacheflush;
    asm volatile(
        "svc 0x0"
        : "=r" (beg)
        : "0" (beg), "r" (end), "r" (flg), "r" (scno));
  #else
    // r7 is reserved by the EABI in thumb mode.
    asm volatile(
    "@   Enter ARM Mode  \n\t"
        "adr r3, 1f      \n\t"
        "bx  r3          \n\t"
        ".ALIGN 4        \n\t"
        ".ARM            \n"
    "1:  push {r7}       \n\t"
        "mov r7, %4      \n\t"
        "svc 0x0         \n\t"
        "pop {r7}        \n\t"
    "@   Enter THUMB Mode\n\t"
        "adr r3, 2f+1    \n\t"
        "bx  r3          \n\t"
        ".THUMB          \n"
    "2:                  \n\t"
        : "=r" (beg)
        : "0" (beg), "r" (end), "r" (flg), "r" (__ARM_NR_cacheflush)
        : "r3");
  #endif
#endif
}

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
