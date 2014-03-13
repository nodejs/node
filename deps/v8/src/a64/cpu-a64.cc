// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// CPU specific code for arm independent of OS goes here.

#include "v8.h"

#if V8_TARGET_ARCH_A64

#include "a64/cpu-a64.h"
#include "a64/utils-a64.h"

namespace v8 {
namespace internal {

#ifdef DEBUG
bool CpuFeatures::initialized_ = false;
#endif
unsigned CpuFeatures::supported_ = 0;
unsigned CpuFeatures::found_by_runtime_probing_only_ = 0;
unsigned CpuFeatures::cross_compile_ = 0;

// Initialise to smallest possible cache size.
unsigned CpuFeatures::dcache_line_size_ = 1;
unsigned CpuFeatures::icache_line_size_ = 1;


void CPU::SetUp() {
  CpuFeatures::Probe();
}


bool CPU::SupportsCrankshaft() {
  return true;
}


void CPU::FlushICache(void* address, size_t length) {
  if (length == 0) {
    return;
  }

#ifdef USE_SIMULATOR
  // TODO(all): consider doing some cache simulation to ensure every address
  // run has been synced.
  USE(address);
  USE(length);
#else
  // The code below assumes user space cache operations are allowed. The goal
  // of this routine is to make sure the code generated is visible to the I
  // side of the CPU.

  uintptr_t start = reinterpret_cast<uintptr_t>(address);
  // Sizes will be used to generate a mask big enough to cover a pointer.
  uintptr_t dsize = static_cast<uintptr_t>(CpuFeatures::dcache_line_size());
  uintptr_t isize = static_cast<uintptr_t>(CpuFeatures::icache_line_size());
  // Cache line sizes are always a power of 2.
  ASSERT(CountSetBits(dsize, 64) == 1);
  ASSERT(CountSetBits(isize, 64) == 1);
  uintptr_t dstart = start & ~(dsize - 1);
  uintptr_t istart = start & ~(isize - 1);
  uintptr_t end = start + length;

  __asm__ __volatile__ (  // NOLINT
    // Clean every line of the D cache containing the target data.
    "0:                                \n\t"
    // dc      : Data Cache maintenance
    //    c    : Clean
    //     va  : by (Virtual) Address
    //       u : to the point of Unification
    // The point of unification for a processor is the point by which the
    // instruction and data caches are guaranteed to see the same copy of a
    // memory location. See ARM DDI 0406B page B2-12 for more information.
    "dc   cvau, %[dline]                \n\t"
    "add  %[dline], %[dline], %[dsize]  \n\t"
    "cmp  %[dline], %[end]              \n\t"
    "b.lt 0b                            \n\t"
    // Barrier to make sure the effect of the code above is visible to the rest
    // of the world.
    // dsb    : Data Synchronisation Barrier
    //    ish : Inner SHareable domain
    // The point of unification for an Inner Shareable shareability domain is
    // the point by which the instruction and data caches of all the processors
    // in that Inner Shareable shareability domain are guaranteed to see the
    // same copy of a memory location.  See ARM DDI 0406B page B2-12 for more
    // information.
    "dsb  ish                           \n\t"
    // Invalidate every line of the I cache containing the target data.
    "1:                                 \n\t"
    // ic      : instruction cache maintenance
    //    i    : invalidate
    //     va  : by address
    //       u : to the point of unification
    "ic   ivau, %[iline]                \n\t"
    "add  %[iline], %[iline], %[isize]  \n\t"
    "cmp  %[iline], %[end]              \n\t"
    "b.lt 1b                            \n\t"
    // Barrier to make sure the effect of the code above is visible to the rest
    // of the world.
    "dsb  ish                           \n\t"
    // Barrier to ensure any prefetching which happened before this code is
    // discarded.
    // isb : Instruction Synchronisation Barrier
    "isb                                \n\t"
    : [dline] "+r" (dstart),
      [iline] "+r" (istart)
    : [dsize] "r"  (dsize),
      [isize] "r"  (isize),
      [end]   "r"  (end)
    // This code does not write to memory but without the dependency gcc might
    // move this code before the code is generated.
    : "cc", "memory"
  );  // NOLINT
#endif
}


void CpuFeatures::Probe() {
  // Compute I and D cache line size. The cache type register holds
  // information about the caches.
  uint32_t cache_type_register = GetCacheType();

  static const int kDCacheLineSizeShift = 16;
  static const int kICacheLineSizeShift = 0;
  static const uint32_t kDCacheLineSizeMask = 0xf << kDCacheLineSizeShift;
  static const uint32_t kICacheLineSizeMask = 0xf << kICacheLineSizeShift;

  // The cache type register holds the size of the I and D caches as a power of
  // two.
  uint32_t dcache_line_size_power_of_two =
      (cache_type_register & kDCacheLineSizeMask) >> kDCacheLineSizeShift;
  uint32_t icache_line_size_power_of_two =
      (cache_type_register & kICacheLineSizeMask) >> kICacheLineSizeShift;

  dcache_line_size_ = 1 << dcache_line_size_power_of_two;
  icache_line_size_ = 1 << icache_line_size_power_of_two;

  // AArch64 has no configuration options, no further probing is required.
  supported_ = 0;

#ifdef DEBUG
  initialized_ = true;
#endif
}


unsigned CpuFeatures::dcache_line_size() {
  ASSERT(initialized_);
  return dcache_line_size_;
}


unsigned CpuFeatures::icache_line_size() {
  ASSERT(initialized_);
  return icache_line_size_;
}


uint32_t CpuFeatures::GetCacheType() {
#ifdef USE_SIMULATOR
  // This will lead to a cache with 1 byte long lines, which is fine since the
  // simulator will not need this information.
  return 0;
#else
  uint32_t cache_type_register;
  // Copy the content of the cache type register to a core register.
  __asm__ __volatile__ ("mrs %[ctr], ctr_el0"  // NOLINT
                        : [ctr] "=r" (cache_type_register));
  return cache_type_register;
#endif
}

} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_A64
