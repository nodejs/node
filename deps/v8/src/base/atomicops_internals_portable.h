// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is an internal atomic implementation, use atomicops.h instead.

#ifndef V8_BASE_ATOMICOPS_INTERNALS_PORTABLE_H_
#define V8_BASE_ATOMICOPS_INTERNALS_PORTABLE_H_

namespace v8 {
namespace base {

inline void MemoryBarrier() { __sync_synchronize(); }

inline Atomic32 NoBarrier_CompareAndSwap(volatile Atomic32* ptr,
                                         Atomic32 old_value,
                                         Atomic32 new_value) {
  return __sync_val_compare_and_swap(ptr, old_value, new_value);
}

inline Atomic32 NoBarrier_AtomicExchange(volatile Atomic32* ptr,
                                         Atomic32 new_value) {
  return __sync_lock_test_and_set(ptr, new_value);
}

inline Atomic32 NoBarrier_AtomicIncrement(volatile Atomic32* ptr,
                                          Atomic32 increment) {
  return __sync_add_and_fetch(ptr, increment);
}

inline Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  return __sync_add_and_fetch(ptr, increment);
}

inline Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value, Atomic32 new_value) {
  return __sync_val_compare_and_swap(ptr, old_value, new_value);
}

inline Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value, Atomic32 new_value) {
  return __sync_val_compare_and_swap(ptr, old_value, new_value);
}

inline void NoBarrier_Store(volatile Atomic8* ptr, Atomic8 value) {
  __sync_lock_test_and_set(ptr, value);
}

inline void NoBarrier_Store(volatile Atomic32* ptr, Atomic32 value) {
  __sync_lock_test_and_set(ptr, value);
}

inline void Acquire_Store(volatile Atomic32* ptr, Atomic32 value) {
  __sync_lock_test_and_set(ptr, value);
}

inline void Release_Store(volatile Atomic32* ptr, Atomic32 value) {
  __sync_lock_test_and_set(ptr, value);
}

inline Atomic8 NoBarrier_Load(volatile const Atomic8* ptr) {
  return __sync_add_and_fetch(ptr, 0);
}

inline Atomic32 NoBarrier_Load(volatile const Atomic32* ptr) {
  return __sync_add_and_fetch(ptr, 0);
}

inline Atomic32 Acquire_Load(volatile const Atomic32* ptr) {
  return __sync_add_and_fetch(ptr, 0);
}

inline Atomic32 Release_Load(volatile const Atomic32* ptr) {
  return __sync_add_and_fetch(ptr, 0);
}

// 64-bit versions of the operations.
// See the 32-bit versions for comments.

inline Atomic64 NoBarrier_CompareAndSwap(volatile Atomic64* ptr,
                                         Atomic64 old_value,
                                         Atomic64 new_value) {
  return __sync_val_compare_and_swap(ptr, old_value, new_value);
}

inline Atomic64 NoBarrier_AtomicExchange(volatile Atomic64* ptr,
                                         Atomic64 new_value) {
  return __sync_lock_test_and_set(ptr, new_value);
}

inline Atomic64 NoBarrier_AtomicIncrement(volatile Atomic64* ptr,
                                          Atomic64 increment) {
  return __sync_add_and_fetch(ptr, increment);
}

inline Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  return __sync_add_and_fetch(ptr, increment);
}

inline Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value, Atomic64 new_value) {
  return __sync_val_compare_and_swap(ptr, old_value, new_value);
}

inline Atomic64 Release_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value, Atomic64 new_value) {
  return __sync_val_compare_and_swap(ptr, old_value, new_value);
}

inline void NoBarrier_Store(volatile Atomic64* ptr, Atomic64 value) {
  __sync_lock_test_and_set(ptr, value);
}

inline void Acquire_Store(volatile Atomic64* ptr, Atomic64 value) {
  __sync_lock_test_and_set(ptr, value);
}

inline void Release_Store(volatile Atomic64* ptr, Atomic64 value) {
  __sync_lock_test_and_set(ptr, value);
}

inline Atomic64 NoBarrier_Load(volatile const Atomic64* ptr) {
  return __sync_add_and_fetch(ptr, 0);
}

inline Atomic64 Acquire_Load(volatile const Atomic64* ptr) {
  return __sync_add_and_fetch(ptr, 0);
}

inline Atomic64 Release_Load(volatile const Atomic64* ptr) {
  return __sync_add_and_fetch(ptr, 0);
}
}  // namespace base
}  // namespace v8

#endif  // V8_BASE_ATOMICOPS_INTERNALS_PORTABLE_H_
