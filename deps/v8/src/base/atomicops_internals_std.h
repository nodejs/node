// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_ATOMICOPS_INTERNALS_STD_H_
#define V8_BASE_ATOMICOPS_INTERNALS_STD_H_

#include <atomic>

#include "src/base/build_config.h"
#include "src/base/macros.h"

namespace v8 {
namespace base {

namespace helper {
template <typename T>
volatile std::atomic<T>* to_std_atomic(volatile T* ptr) {
  return reinterpret_cast<volatile std::atomic<T>*>(ptr);
}
template <typename T>
volatile const std::atomic<T>* to_std_atomic_const(volatile const T* ptr) {
  return reinterpret_cast<volatile const std::atomic<T>*>(ptr);
}
}  // namespace helper

inline void SeqCst_MemoryFence() {
  std::atomic_thread_fence(std::memory_order_seq_cst);
}

inline Atomic32 Relaxed_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value, Atomic32 new_value) {
  std::atomic_compare_exchange_strong_explicit(
      helper::to_std_atomic(ptr), &old_value, new_value,
      std::memory_order_relaxed, std::memory_order_relaxed);
  return old_value;
}

inline Atomic32 Relaxed_AtomicExchange(volatile Atomic32* ptr,
                                       Atomic32 new_value) {
  return std::atomic_exchange_explicit(helper::to_std_atomic(ptr), new_value,
                                       std::memory_order_relaxed);
}

inline Atomic32 Relaxed_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  return increment + std::atomic_fetch_add_explicit(helper::to_std_atomic(ptr),
                                                    increment,
                                                    std::memory_order_relaxed);
}

inline Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  return increment + std::atomic_fetch_add_explicit(helper::to_std_atomic(ptr),
                                                    increment,
                                                    std::memory_order_seq_cst);
}

inline Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value, Atomic32 new_value) {
  atomic_compare_exchange_strong_explicit(
      helper::to_std_atomic(ptr), &old_value, new_value,
      std::memory_order_acquire, std::memory_order_acquire);
  return old_value;
}

inline Atomic8 Release_CompareAndSwap(volatile Atomic8* ptr, Atomic8 old_value,
                                      Atomic8 new_value) {
  bool result = atomic_compare_exchange_strong_explicit(
      helper::to_std_atomic(ptr), &old_value, new_value,
      std::memory_order_release, std::memory_order_relaxed);
  USE(result);  // Make gcc compiler happy.
  return old_value;
}

inline Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value, Atomic32 new_value) {
  atomic_compare_exchange_strong_explicit(
      helper::to_std_atomic(ptr), &old_value, new_value,
      std::memory_order_release, std::memory_order_relaxed);
  return old_value;
}

inline void Relaxed_Store(volatile Atomic8* ptr, Atomic8 value) {
  std::atomic_store_explicit(helper::to_std_atomic(ptr), value,
                             std::memory_order_relaxed);
}

inline void Relaxed_Store(volatile Atomic32* ptr, Atomic32 value) {
  std::atomic_store_explicit(helper::to_std_atomic(ptr), value,
                             std::memory_order_relaxed);
}

inline void Release_Store(volatile Atomic32* ptr, Atomic32 value) {
  std::atomic_store_explicit(helper::to_std_atomic(ptr), value,
                             std::memory_order_release);
}

inline Atomic8 Relaxed_Load(volatile const Atomic8* ptr) {
  return std::atomic_load_explicit(helper::to_std_atomic_const(ptr),
                                   std::memory_order_relaxed);
}

inline Atomic32 Relaxed_Load(volatile const Atomic32* ptr) {
  return std::atomic_load_explicit(helper::to_std_atomic_const(ptr),
                                   std::memory_order_relaxed);
}

inline Atomic32 Acquire_Load(volatile const Atomic32* ptr) {
  return std::atomic_load_explicit(helper::to_std_atomic_const(ptr),
                                   std::memory_order_acquire);
}

#if defined(V8_HOST_ARCH_64_BIT)

inline Atomic64 Relaxed_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value, Atomic64 new_value) {
  std::atomic_compare_exchange_strong_explicit(
      helper::to_std_atomic(ptr), &old_value, new_value,
      std::memory_order_relaxed, std::memory_order_relaxed);
  return old_value;
}

inline Atomic64 Relaxed_AtomicExchange(volatile Atomic64* ptr,
                                       Atomic64 new_value) {
  return std::atomic_exchange_explicit(helper::to_std_atomic(ptr), new_value,
                                       std::memory_order_relaxed);
}

inline Atomic64 Relaxed_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  return increment + std::atomic_fetch_add_explicit(helper::to_std_atomic(ptr),
                                                    increment,
                                                    std::memory_order_relaxed);
}

inline Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  return increment + std::atomic_fetch_add_explicit(helper::to_std_atomic(ptr),
                                                    increment,
                                                    std::memory_order_seq_cst);
}

inline Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value, Atomic64 new_value) {
  std::atomic_compare_exchange_strong_explicit(
      helper::to_std_atomic(ptr), &old_value, new_value,
      std::memory_order_acquire, std::memory_order_acquire);
  return old_value;
}

inline Atomic64 Release_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value, Atomic64 new_value) {
  std::atomic_compare_exchange_strong_explicit(
      helper::to_std_atomic(ptr), &old_value, new_value,
      std::memory_order_release, std::memory_order_relaxed);
  return old_value;
}

inline void Relaxed_Store(volatile Atomic64* ptr, Atomic64 value) {
  std::atomic_store_explicit(helper::to_std_atomic(ptr), value,
                             std::memory_order_relaxed);
}

inline void Release_Store(volatile Atomic64* ptr, Atomic64 value) {
  std::atomic_store_explicit(helper::to_std_atomic(ptr), value,
                             std::memory_order_release);
}

inline Atomic64 Relaxed_Load(volatile const Atomic64* ptr) {
  return std::atomic_load_explicit(helper::to_std_atomic_const(ptr),
                                   std::memory_order_relaxed);
}

inline Atomic64 Acquire_Load(volatile const Atomic64* ptr) {
  return std::atomic_load_explicit(helper::to_std_atomic_const(ptr),
                                   std::memory_order_acquire);
}

#endif  // defined(V8_HOST_ARCH_64_BIT)
}  // namespace base
}  // namespace v8

#endif  // V8_BASE_ATOMICOPS_INTERNALS_STD_H_
