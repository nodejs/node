// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The routines exported by this module are subtle.  If you use them, even if
// you get the code right, it will depend on careful reasoning about atomicity
// and memory ordering; it will be less readable, and harder to maintain.  If
// you plan to use these routines, you should have a good reason, such as solid
// evidence that performance would otherwise suffer, or there being no
// alternative.  You should assume only properties explicitly guaranteed by the
// specifications in this file.  You are almost certainly _not_ writing code
// just for the x86; if you assume x86 semantics, x86 hardware bugs and
// implementations on other archtectures will cause your code to break.  If you
// do not know what you are doing, avoid these routines, and use a Mutex.
//
// It is incorrect to make direct assignments to/from an atomic variable.
// You should use one of the Load or Store routines.  The Relaxed  versions
// are provided when no fences are needed:
//   Relaxed_Store()
//   Relaxed_Load()
// Although there are currently no compiler enforcement, you are encouraged
// to use these.
//

#ifndef V8_BASE_ATOMICOPS_H_
#define V8_BASE_ATOMICOPS_H_

#include <stdint.h>

// Small C++ header which defines implementation specific macros used to
// identify the STL implementation.
// - libc++: captures __config for _LIBCPP_VERSION
// - libstdc++: captures bits/c++config.h for __GLIBCXX__
#include <cstddef>

#include "src/base/base-export.h"
#include "src/base/build_config.h"

namespace v8 {
namespace base {

typedef char Atomic8;
typedef int16_t Atomic16;
typedef int32_t Atomic32;
#if defined(V8_HOST_ARCH_64_BIT)
// We need to be able to go between Atomic64 and AtomicWord implicitly.  This
// means Atomic64 and AtomicWord should be the same type on 64-bit.
#if defined(__ILP32__)
typedef int64_t Atomic64;
#else
typedef intptr_t Atomic64;
#endif  // defined(__ILP32__)
#endif  // defined(V8_HOST_ARCH_64_BIT)

// Use AtomicWord for a machine-sized pointer.  It will use the Atomic32 or
// Atomic64 routines below, depending on your architecture.
typedef intptr_t AtomicWord;

// Atomically execute:
//      result = *ptr;
//      if (*ptr == old_value)
//        *ptr = new_value;
//      return result;
//
// I.e., replace "*ptr" with "new_value" if "*ptr" used to be "old_value".
// Always return the old value of "*ptr"
//
// This routine implies no memory barriers.
Atomic16 Relaxed_CompareAndSwap(volatile Atomic16* ptr, Atomic16 old_value,
                                Atomic16 new_value);
Atomic32 Relaxed_CompareAndSwap(volatile Atomic32* ptr, Atomic32 old_value,
                                Atomic32 new_value);

// Atomically store new_value into *ptr, returning the previous value held in
// *ptr.  This routine implies no memory barriers.
Atomic32 Relaxed_AtomicExchange(volatile Atomic32* ptr, Atomic32 new_value);

// Atomically increment *ptr by "increment".  Returns the new value of
// *ptr with the increment applied.  This routine implies no memory barriers.
Atomic32 Relaxed_AtomicIncrement(volatile Atomic32* ptr, Atomic32 increment);

Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                 Atomic32 increment);

// These following lower-level operations are typically useful only to people
// implementing higher-level synchronization operations like spinlocks,
// mutexes, and condition-variables.  They combine CompareAndSwap(), a load,
// or a store with appropriate memory-ordering instructions.  "Acquire"
// operations ensure that no later memory access can be reordered ahead of the
// operation. "Release" operations ensure that no previous memory access can
// be reordered after the operation.  "Fence" operations have both "Acquire"
// and "Release" semantics. A SeqCst_MemoryFence() has "Fence" semantics, but
// does no memory access.
Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                Atomic32 old_value,
                                Atomic32 new_value);
Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                Atomic32 old_value,
                                Atomic32 new_value);

void SeqCst_MemoryFence();
void Relaxed_Store(volatile Atomic8* ptr, Atomic8 value);
void Relaxed_Store(volatile Atomic16* ptr, Atomic16 value);
void Relaxed_Store(volatile Atomic32* ptr, Atomic32 value);
void Release_Store(volatile Atomic32* ptr, Atomic32 value);

Atomic8 Relaxed_Load(volatile const Atomic8* ptr);
Atomic16 Relaxed_Load(volatile const Atomic16* ptr);
Atomic32 Relaxed_Load(volatile const Atomic32* ptr);
Atomic32 Acquire_Load(volatile const Atomic32* ptr);

// 64-bit atomic operations (only available on 64-bit processors).
#ifdef V8_HOST_ARCH_64_BIT
Atomic64 Relaxed_CompareAndSwap(volatile Atomic64* ptr, Atomic64 old_value,
                                Atomic64 new_value);
Atomic64 Relaxed_AtomicExchange(volatile Atomic64* ptr, Atomic64 new_value);
Atomic64 Relaxed_AtomicIncrement(volatile Atomic64* ptr, Atomic64 increment);
Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr, Atomic64 increment);

Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                Atomic64 old_value,
                                Atomic64 new_value);
Atomic64 Release_CompareAndSwap(volatile Atomic64* ptr,
                                Atomic64 old_value,
                                Atomic64 new_value);
void Relaxed_Store(volatile Atomic64* ptr, Atomic64 value);
void Release_Store(volatile Atomic64* ptr, Atomic64 value);
Atomic64 Relaxed_Load(volatile const Atomic64* ptr);
Atomic64 Acquire_Load(volatile const Atomic64* ptr);
#endif  // V8_HOST_ARCH_64_BIT

}  // namespace base
}  // namespace v8

#if defined(V8_OS_WIN)
#include "src/base/atomicops_internals_std.h"
#else
// TODO(ulan): Switch to std version after performance regression with Wheezy
// sysroot is no longer relevant. Debian Wheezy LTS ends on 31st of May 2018.
#include "src/base/atomicops_internals_portable.h"
#endif

// On some platforms we need additional declarations to make
// AtomicWord compatible with our other Atomic* types.
#if defined(V8_OS_MACOSX) || defined(V8_OS_OPENBSD) || defined(V8_OS_AIX)
#include "src/base/atomicops_internals_atomicword_compat.h"
#endif

#endif  // V8_BASE_ATOMICOPS_H_
