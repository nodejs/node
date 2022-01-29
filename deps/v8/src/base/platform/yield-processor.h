// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_PLATFORM_YIELD_PROCESSOR_H_
#define V8_BASE_PLATFORM_YIELD_PROCESSOR_H_

// The YIELD_PROCESSOR macro wraps an architecture specific-instruction that
// informs the processor we're in a busy wait, so it can handle the branch more
// intelligently and e.g. reduce power to our core or give more resources to the
// other hyper-thread on this core. See the following for context:
// https://software.intel.com/en-us/articles/benefitting-power-and-performance-sleep-loops

#if defined(V8_CC_MSVC)
// MSVC does not support inline assembly via __asm__ and provides compiler
// intrinsics instead. Check if there is a usable intrinsic.
//
// intrin.h is an expensive header, so only include it if we're on a host
// architecture that has a usable intrinsic.
#if defined(V8_HOST_ARCH_IA32) || defined(V8_HOST_ARCH_X64)
#include <intrin.h>
#define YIELD_PROCESSOR _mm_pause()
#elif defined(V8_HOST_ARCH_ARM64) || \
    (defined(V8_HOST_ARCH_ARM) && __ARM_ARCH >= 6)
#include <intrin.h>
#define YIELD_PROCESSOR __yield()
#endif  // V8_HOST_ARCH

#else  // !V8_CC_MSVC

#if defined(V8_HOST_ARCH_IA32) || defined(V8_HOST_ARCH_X64)
#define YIELD_PROCESSOR __asm__ __volatile__("pause")
#elif defined(V8_HOST_ARCH_ARM64) || \
    (defined(V8_HOST_ARCH_ARM) && __ARM_ARCH >= 6)
#define YIELD_PROCESSOR __asm__ __volatile__("yield")
#elif defined(V8_HOST_ARCH_MIPS)
// The MIPS32 docs state that the PAUSE instruction is a no-op on older
// architectures (first added in MIPS32r2). To avoid assembler errors when
// targeting pre-r2, we must encode the instruction manually.
#define YIELD_PROCESSOR __asm__ __volatile__(".word 0x00000140")
#elif defined(V8_HOST_ARCH_MIPS64EL) && __mips_isa_rev >= 2
// Don't bother doing using .word here since r2 is the lowest supported mips64
// that Chromium supports.
#define YIELD_PROCESSOR __asm__ __volatile__("pause")
#elif defined(V8_HOST_ARCH_PPC64)
#define YIELD_PROCESSOR __asm__ __volatile__("or 31,31,31")
#endif  // V8_HOST_ARCH

#endif  // V8_CC_MSVC

#ifndef YIELD_PROCESSOR
#define YIELD_PROCESSOR ((void)0)
#endif

#endif  // V8_BASE_PLATFORM_YIELD_PROCESSOR_H_
