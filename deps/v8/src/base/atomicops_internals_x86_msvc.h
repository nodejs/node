// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is an internal atomic implementation, use base/atomicops.h instead.

#ifndef V8_BASE_ATOMICOPS_INTERNALS_X86_MSVC_H_
#define V8_BASE_ATOMICOPS_INTERNALS_X86_MSVC_H_

#include "src/base/macros.h"
#include "src/base/win32-headers.h"

namespace v8 {
namespace base {

inline Atomic32 Relaxed_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value, Atomic32 new_value) {
  LONG result = InterlockedCompareExchange(
      reinterpret_cast<volatile LONG*>(ptr), static_cast<LONG>(new_value),
      static_cast<LONG>(old_value));
  return static_cast<Atomic32>(result);
}

inline Atomic32 Relaxed_AtomicExchange(volatile Atomic32* ptr,
                                       Atomic32 new_value) {
  LONG result = InterlockedExchange(reinterpret_cast<volatile LONG*>(ptr),
                                    static_cast<LONG>(new_value));
  return static_cast<Atomic32>(result);
}

inline Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  return InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(ptr),
                                static_cast<LONG>(increment)) +
         increment;
}

inline Atomic32 Relaxed_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  return Barrier_AtomicIncrement(ptr, increment);
}

inline void MemoryFence() { MemoryBarrier(); }

inline Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  return Relaxed_CompareAndSwap(ptr, old_value, new_value);
}

inline Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  return Relaxed_CompareAndSwap(ptr, old_value, new_value);
}

inline void Relaxed_Store(volatile Atomic8* ptr, Atomic8 value) {
  *ptr = value;
}

inline void Relaxed_Store(volatile Atomic32* ptr, Atomic32 value) {
  *ptr = value;
}

inline void Release_Store(volatile Atomic32* ptr, Atomic32 value) {
  *ptr = value;  // works w/o barrier for current Intel chips as of June 2005
  // See comments in Atomic64 version of Release_Store() below.
}

inline Atomic8 Relaxed_Load(volatile const Atomic8* ptr) { return *ptr; }

inline Atomic32 Relaxed_Load(volatile const Atomic32* ptr) { return *ptr; }

inline Atomic32 Acquire_Load(volatile const Atomic32* ptr) {
  Atomic32 value = *ptr;
  return value;
}

#if defined(_WIN64)

// 64-bit low-level operations on 64-bit platform.

static_assert(sizeof(Atomic64) == sizeof(PVOID), "atomic word is atomic");

inline Atomic64 Relaxed_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value, Atomic64 new_value) {
  PVOID result = InterlockedCompareExchangePointer(
    reinterpret_cast<volatile PVOID*>(ptr),
    reinterpret_cast<PVOID>(new_value), reinterpret_cast<PVOID>(old_value));
  return reinterpret_cast<Atomic64>(result);
}

inline Atomic64 Relaxed_AtomicExchange(volatile Atomic64* ptr,
                                       Atomic64 new_value) {
  PVOID result = InterlockedExchangePointer(
    reinterpret_cast<volatile PVOID*>(ptr),
    reinterpret_cast<PVOID>(new_value));
  return reinterpret_cast<Atomic64>(result);
}

inline Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  return InterlockedExchangeAdd64(
      reinterpret_cast<volatile LONGLONG*>(ptr),
      static_cast<LONGLONG>(increment)) + increment;
}

inline Atomic64 Relaxed_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  return Barrier_AtomicIncrement(ptr, increment);
}

inline void Relaxed_Store(volatile Atomic64* ptr, Atomic64 value) {
  *ptr = value;
}

inline void Release_Store(volatile Atomic64* ptr, Atomic64 value) {
  *ptr = value;  // works w/o barrier for current Intel chips as of June 2005

  // When new chips come out, check:
  //  IA-32 Intel Architecture Software Developer's Manual, Volume 3:
  //  System Programming Guide, Chatper 7: Multiple-processor management,
  //  Section 7.2, Memory Ordering.
  // Last seen at:
  //   http://developer.intel.com/design/pentium4/manuals/index_new.htm
}

inline Atomic64 Relaxed_Load(volatile const Atomic64* ptr) { return *ptr; }

inline Atomic64 Acquire_Load(volatile const Atomic64* ptr) {
  Atomic64 value = *ptr;
  return value;
}

inline Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  return Relaxed_CompareAndSwap(ptr, old_value, new_value);
}

inline Atomic64 Release_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  return Relaxed_CompareAndSwap(ptr, old_value, new_value);
}


#endif  // defined(_WIN64)

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_ATOMICOPS_INTERNALS_X86_MSVC_H_
