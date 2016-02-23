// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is an internal atomic implementation, use atomicops.h instead.

#ifndef V8_BASE_ATOMICOPS_INTERNALS_MAC_H_
#define V8_BASE_ATOMICOPS_INTERNALS_MAC_H_

#include <libkern/OSAtomic.h>

namespace v8 {
namespace base {

#define ATOMICOPS_COMPILER_BARRIER() __asm__ __volatile__("" : : : "memory")

inline void MemoryBarrier() { OSMemoryBarrier(); }

inline void AcquireMemoryBarrier() {
// On x86 processors, loads already have acquire semantics, so
// there is no need to put a full barrier here.
#if V8_HOST_ARCH_IA32 || V8_HOST_ARCH_X64
  ATOMICOPS_COMPILER_BARRIER();
#else
  MemoryBarrier();
#endif
}

inline Atomic32 NoBarrier_CompareAndSwap(volatile Atomic32* ptr,
                                         Atomic32 old_value,
                                         Atomic32 new_value) {
  Atomic32 prev_value;
  do {
    if (OSAtomicCompareAndSwap32(old_value, new_value,
                                 const_cast<Atomic32*>(ptr))) {
      return old_value;
    }
    prev_value = *ptr;
  } while (prev_value == old_value);
  return prev_value;
}

inline Atomic32 NoBarrier_AtomicExchange(volatile Atomic32* ptr,
                                         Atomic32 new_value) {
  Atomic32 old_value;
  do {
    old_value = *ptr;
  } while (!OSAtomicCompareAndSwap32(old_value, new_value,
                                     const_cast<Atomic32*>(ptr)));
  return old_value;
}

inline Atomic32 NoBarrier_AtomicIncrement(volatile Atomic32* ptr,
                                          Atomic32 increment) {
  return OSAtomicAdd32(increment, const_cast<Atomic32*>(ptr));
}

inline Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  return OSAtomicAdd32Barrier(increment, const_cast<Atomic32*>(ptr));
}

inline Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  Atomic32 prev_value;
  do {
    if (OSAtomicCompareAndSwap32Barrier(old_value, new_value,
                                        const_cast<Atomic32*>(ptr))) {
      return old_value;
    }
    prev_value = *ptr;
  } while (prev_value == old_value);
  return prev_value;
}

inline Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  return Acquire_CompareAndSwap(ptr, old_value, new_value);
}

inline void NoBarrier_Store(volatile Atomic8* ptr, Atomic8 value) {
  *ptr = value;
}

inline void NoBarrier_Store(volatile Atomic32* ptr, Atomic32 value) {
  *ptr = value;
}

inline void Acquire_Store(volatile Atomic32* ptr, Atomic32 value) {
  *ptr = value;
  MemoryBarrier();
}

inline void Release_Store(volatile Atomic32* ptr, Atomic32 value) {
  MemoryBarrier();
  *ptr = value;
}

inline Atomic8 NoBarrier_Load(volatile const Atomic8* ptr) {
  return *ptr;
}

inline Atomic32 NoBarrier_Load(volatile const Atomic32* ptr) {
  return *ptr;
}

inline Atomic32 Acquire_Load(volatile const Atomic32* ptr) {
  Atomic32 value = *ptr;
  AcquireMemoryBarrier();
  return value;
}

inline Atomic32 Release_Load(volatile const Atomic32* ptr) {
  MemoryBarrier();
  return *ptr;
}

#ifdef __LP64__

// 64-bit implementation on 64-bit platform

inline Atomic64 NoBarrier_CompareAndSwap(volatile Atomic64* ptr,
                                         Atomic64 old_value,
                                         Atomic64 new_value) {
  Atomic64 prev_value;
  do {
    if (OSAtomicCompareAndSwap64(old_value, new_value,
                                 reinterpret_cast<volatile int64_t*>(ptr))) {
      return old_value;
    }
    prev_value = *ptr;
  } while (prev_value == old_value);
  return prev_value;
}

inline Atomic64 NoBarrier_AtomicExchange(volatile Atomic64* ptr,
                                         Atomic64 new_value) {
  Atomic64 old_value;
  do {
    old_value = *ptr;
  } while (!OSAtomicCompareAndSwap64(old_value, new_value,
                                     reinterpret_cast<volatile int64_t*>(ptr)));
  return old_value;
}

inline Atomic64 NoBarrier_AtomicIncrement(volatile Atomic64* ptr,
                                          Atomic64 increment) {
  return OSAtomicAdd64(increment, reinterpret_cast<volatile int64_t*>(ptr));
}

inline Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  return OSAtomicAdd64Barrier(increment,
                              reinterpret_cast<volatile int64_t*>(ptr));
}

inline Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  Atomic64 prev_value;
  do {
    if (OSAtomicCompareAndSwap64Barrier(
        old_value, new_value, reinterpret_cast<volatile int64_t*>(ptr))) {
      return old_value;
    }
    prev_value = *ptr;
  } while (prev_value == old_value);
  return prev_value;
}

inline Atomic64 Release_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  // The lib kern interface does not distinguish between
  // Acquire and Release memory barriers; they are equivalent.
  return Acquire_CompareAndSwap(ptr, old_value, new_value);
}

inline void NoBarrier_Store(volatile Atomic64* ptr, Atomic64 value) {
  *ptr = value;
}

inline void Acquire_Store(volatile Atomic64* ptr, Atomic64 value) {
  *ptr = value;
  MemoryBarrier();
}

inline void Release_Store(volatile Atomic64* ptr, Atomic64 value) {
  MemoryBarrier();
  *ptr = value;
}

inline Atomic64 NoBarrier_Load(volatile const Atomic64* ptr) {
  return *ptr;
}

inline Atomic64 Acquire_Load(volatile const Atomic64* ptr) {
  Atomic64 value = *ptr;
  AcquireMemoryBarrier();
  return value;
}

inline Atomic64 Release_Load(volatile const Atomic64* ptr) {
  MemoryBarrier();
  return *ptr;
}

#endif  // defined(__LP64__)

#undef ATOMICOPS_COMPILER_BARRIER
}  // namespace base
}  // namespace v8

#endif  // V8_BASE_ATOMICOPS_INTERNALS_MAC_H_
