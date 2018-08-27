// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is an internal atomic implementation, use atomicops.h instead.
//
// This implementation uses C++11 atomics' member functions. The code base is
// currently written assuming atomicity revolves around accesses instead of
// C++11's memory locations. The burden is on the programmer to ensure that all
// memory locations accessed atomically are never accessed non-atomically (tsan
// should help with this).
//
// Of note in this implementation:
//  * All NoBarrier variants are implemented as relaxed.
//  * All Barrier variants are implemented as sequentially-consistent.
//  * Compare exchange's failure ordering is always the same as the success one
//    (except for release, which fails as relaxed): using a weaker ordering is
//    only valid under certain uses of compare exchange.
//  * Acquire store doesn't exist in the C11 memory model, it is instead
//    implemented as a relaxed store followed by a sequentially consistent
//    fence.
//  * Release load doesn't exist in the C11 memory model, it is instead
//    implemented as sequentially consistent fence followed by a relaxed load.
//  * Atomic increment is expected to return the post-incremented value, whereas
//    C11 fetch add returns the previous value. The implementation therefore
//    needs to increment twice (which the compiler should be able to detect and
//    optimize).

#ifndef V8_BASE_ATOMICOPS_INTERNALS_PORTABLE_H_
#define V8_BASE_ATOMICOPS_INTERNALS_PORTABLE_H_

#include <atomic>

#include "src/base/build_config.h"
#include "src/base/macros.h"

namespace v8 {
namespace base {

// This implementation is transitional and maintains the original API for
// atomicops.h.

inline void SeqCst_MemoryFence() {
#if defined(__GLIBCXX__)
  // Work around libstdc++ bug 51038 where atomic_thread_fence was declared but
  // not defined, leading to the linker complaining about undefined references.
  __atomic_thread_fence(std::memory_order_seq_cst);
#else
  std::atomic_thread_fence(std::memory_order_seq_cst);
#endif
}

inline Atomic32 Relaxed_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value, Atomic32 new_value) {
  __atomic_compare_exchange_n(ptr, &old_value, new_value, false,
                              __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  return old_value;
}

inline Atomic32 Relaxed_AtomicExchange(volatile Atomic32* ptr,
                                       Atomic32 new_value) {
  return __atomic_exchange_n(ptr, new_value, __ATOMIC_RELAXED);
}

inline Atomic32 Relaxed_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  return increment + __atomic_fetch_add(ptr, increment, __ATOMIC_RELAXED);
}

inline Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  return increment + __atomic_fetch_add(ptr, increment, __ATOMIC_SEQ_CST);
}

inline Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value, Atomic32 new_value) {
  __atomic_compare_exchange_n(ptr, &old_value, new_value, false,
                              __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE);
  return old_value;
}

inline Atomic8 Release_CompareAndSwap(volatile Atomic8* ptr, Atomic8 old_value,
                                      Atomic8 new_value) {
  bool result = __atomic_compare_exchange_n(ptr, &old_value, new_value, false,
                                            __ATOMIC_RELEASE, __ATOMIC_RELAXED);
  USE(result);  // Make gcc compiler happy.
  return old_value;
}

inline Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value, Atomic32 new_value) {
  __atomic_compare_exchange_n(ptr, &old_value, new_value, false,
                              __ATOMIC_RELEASE, __ATOMIC_RELAXED);
  return old_value;
}

inline void Relaxed_Store(volatile Atomic8* ptr, Atomic8 value) {
  __atomic_store_n(ptr, value, __ATOMIC_RELAXED);
}

inline void Relaxed_Store(volatile Atomic32* ptr, Atomic32 value) {
  __atomic_store_n(ptr, value, __ATOMIC_RELAXED);
}

inline void Release_Store(volatile Atomic32* ptr, Atomic32 value) {
  __atomic_store_n(ptr, value, __ATOMIC_RELEASE);
}

inline Atomic8 Relaxed_Load(volatile const Atomic8* ptr) {
  return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}

inline Atomic32 Relaxed_Load(volatile const Atomic32* ptr) {
  return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}

inline Atomic32 Acquire_Load(volatile const Atomic32* ptr) {
  return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}

#if defined(V8_HOST_ARCH_64_BIT)

inline Atomic64 Relaxed_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value, Atomic64 new_value) {
  __atomic_compare_exchange_n(ptr, &old_value, new_value, false,
                              __ATOMIC_RELAXED, __ATOMIC_RELAXED);
  return old_value;
}

inline Atomic64 Relaxed_AtomicExchange(volatile Atomic64* ptr,
                                       Atomic64 new_value) {
  return __atomic_exchange_n(ptr, new_value, __ATOMIC_RELAXED);
}

inline Atomic64 Relaxed_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  return increment + __atomic_fetch_add(ptr, increment, __ATOMIC_RELAXED);
}

inline Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  return increment + __atomic_fetch_add(ptr, increment, __ATOMIC_SEQ_CST);
}

inline Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value, Atomic64 new_value) {
  __atomic_compare_exchange_n(ptr, &old_value, new_value, false,
                              __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE);
  return old_value;
}

inline Atomic64 Release_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value, Atomic64 new_value) {
  __atomic_compare_exchange_n(ptr, &old_value, new_value, false,
                              __ATOMIC_RELEASE, __ATOMIC_RELAXED);
  return old_value;
}

inline void Relaxed_Store(volatile Atomic64* ptr, Atomic64 value) {
  __atomic_store_n(ptr, value, __ATOMIC_RELAXED);
}

inline void Release_Store(volatile Atomic64* ptr, Atomic64 value) {
  __atomic_store_n(ptr, value, __ATOMIC_RELEASE);
}

inline Atomic64 Relaxed_Load(volatile const Atomic64* ptr) {
  return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}

inline Atomic64 Acquire_Load(volatile const Atomic64* ptr) {
  return __atomic_load_n(ptr, __ATOMIC_ACQUIRE);
}

#endif  // defined(V8_HOST_ARCH_64_BIT)
}  // namespace base
}  // namespace v8

#endif  // V8_BASE_ATOMICOPS_INTERNALS_PORTABLE_H_
