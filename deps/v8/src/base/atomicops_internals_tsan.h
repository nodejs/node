// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


// This file is an internal atomic implementation for compiler-based
// ThreadSanitizer. Use base/atomicops.h instead.

#ifndef V8_BASE_ATOMICOPS_INTERNALS_TSAN_H_
#define V8_BASE_ATOMICOPS_INTERNALS_TSAN_H_

namespace v8 {
namespace base {

#ifndef TSAN_INTERFACE_ATOMIC_H
#define TSAN_INTERFACE_ATOMIC_H


extern "C" {
typedef char  __tsan_atomic8;
typedef short __tsan_atomic16;  // NOLINT
typedef int   __tsan_atomic32;
typedef long  __tsan_atomic64;  // NOLINT

#if defined(__SIZEOF_INT128__) \
    || (__clang_major__ * 100 + __clang_minor__ >= 302)
typedef __int128 __tsan_atomic128;
#define __TSAN_HAS_INT128 1
#else
typedef char     __tsan_atomic128;
#define __TSAN_HAS_INT128 0
#endif

typedef enum {
  __tsan_memory_order_relaxed,
  __tsan_memory_order_consume,
  __tsan_memory_order_acquire,
  __tsan_memory_order_release,
  __tsan_memory_order_acq_rel,
  __tsan_memory_order_seq_cst,
} __tsan_memory_order;

__tsan_atomic8 __tsan_atomic8_load(const volatile __tsan_atomic8* a,
    __tsan_memory_order mo);
__tsan_atomic16 __tsan_atomic16_load(const volatile __tsan_atomic16* a,
    __tsan_memory_order mo);
__tsan_atomic32 __tsan_atomic32_load(const volatile __tsan_atomic32* a,
    __tsan_memory_order mo);
__tsan_atomic64 __tsan_atomic64_load(const volatile __tsan_atomic64* a,
    __tsan_memory_order mo);
__tsan_atomic128 __tsan_atomic128_load(const volatile __tsan_atomic128* a,
    __tsan_memory_order mo);

void __tsan_atomic8_store(volatile __tsan_atomic8* a, __tsan_atomic8 v,
    __tsan_memory_order mo);
void __tsan_atomic16_store(volatile __tsan_atomic16* a, __tsan_atomic16 v,
    __tsan_memory_order mo);
void __tsan_atomic32_store(volatile __tsan_atomic32* a, __tsan_atomic32 v,
    __tsan_memory_order mo);
void __tsan_atomic64_store(volatile __tsan_atomic64* a, __tsan_atomic64 v,
    __tsan_memory_order mo);
void __tsan_atomic128_store(volatile __tsan_atomic128* a, __tsan_atomic128 v,
    __tsan_memory_order mo);

__tsan_atomic8 __tsan_atomic8_exchange(volatile __tsan_atomic8* a,
    __tsan_atomic8 v, __tsan_memory_order mo);
__tsan_atomic16 __tsan_atomic16_exchange(volatile __tsan_atomic16* a,
    __tsan_atomic16 v, __tsan_memory_order mo);
__tsan_atomic32 __tsan_atomic32_exchange(volatile __tsan_atomic32* a,
    __tsan_atomic32 v, __tsan_memory_order mo);
__tsan_atomic64 __tsan_atomic64_exchange(volatile __tsan_atomic64* a,
    __tsan_atomic64 v, __tsan_memory_order mo);
__tsan_atomic128 __tsan_atomic128_exchange(volatile __tsan_atomic128* a,
    __tsan_atomic128 v, __tsan_memory_order mo);

__tsan_atomic8 __tsan_atomic8_fetch_add(volatile __tsan_atomic8* a,
    __tsan_atomic8 v, __tsan_memory_order mo);
__tsan_atomic16 __tsan_atomic16_fetch_add(volatile __tsan_atomic16* a,
    __tsan_atomic16 v, __tsan_memory_order mo);
__tsan_atomic32 __tsan_atomic32_fetch_add(volatile __tsan_atomic32* a,
    __tsan_atomic32 v, __tsan_memory_order mo);
__tsan_atomic64 __tsan_atomic64_fetch_add(volatile __tsan_atomic64* a,
    __tsan_atomic64 v, __tsan_memory_order mo);
__tsan_atomic128 __tsan_atomic128_fetch_add(volatile __tsan_atomic128* a,
    __tsan_atomic128 v, __tsan_memory_order mo);

__tsan_atomic8 __tsan_atomic8_fetch_and(volatile __tsan_atomic8* a,
    __tsan_atomic8 v, __tsan_memory_order mo);
__tsan_atomic16 __tsan_atomic16_fetch_and(volatile __tsan_atomic16* a,
    __tsan_atomic16 v, __tsan_memory_order mo);
__tsan_atomic32 __tsan_atomic32_fetch_and(volatile __tsan_atomic32* a,
    __tsan_atomic32 v, __tsan_memory_order mo);
__tsan_atomic64 __tsan_atomic64_fetch_and(volatile __tsan_atomic64* a,
    __tsan_atomic64 v, __tsan_memory_order mo);
__tsan_atomic128 __tsan_atomic128_fetch_and(volatile __tsan_atomic128* a,
    __tsan_atomic128 v, __tsan_memory_order mo);

__tsan_atomic8 __tsan_atomic8_fetch_or(volatile __tsan_atomic8* a,
    __tsan_atomic8 v, __tsan_memory_order mo);
__tsan_atomic16 __tsan_atomic16_fetch_or(volatile __tsan_atomic16* a,
    __tsan_atomic16 v, __tsan_memory_order mo);
__tsan_atomic32 __tsan_atomic32_fetch_or(volatile __tsan_atomic32* a,
    __tsan_atomic32 v, __tsan_memory_order mo);
__tsan_atomic64 __tsan_atomic64_fetch_or(volatile __tsan_atomic64* a,
    __tsan_atomic64 v, __tsan_memory_order mo);
__tsan_atomic128 __tsan_atomic128_fetch_or(volatile __tsan_atomic128* a,
    __tsan_atomic128 v, __tsan_memory_order mo);

__tsan_atomic8 __tsan_atomic8_fetch_xor(volatile __tsan_atomic8* a,
    __tsan_atomic8 v, __tsan_memory_order mo);
__tsan_atomic16 __tsan_atomic16_fetch_xor(volatile __tsan_atomic16* a,
    __tsan_atomic16 v, __tsan_memory_order mo);
__tsan_atomic32 __tsan_atomic32_fetch_xor(volatile __tsan_atomic32* a,
    __tsan_atomic32 v, __tsan_memory_order mo);
__tsan_atomic64 __tsan_atomic64_fetch_xor(volatile __tsan_atomic64* a,
    __tsan_atomic64 v, __tsan_memory_order mo);
__tsan_atomic128 __tsan_atomic128_fetch_xor(volatile __tsan_atomic128* a,
    __tsan_atomic128 v, __tsan_memory_order mo);

__tsan_atomic8 __tsan_atomic8_fetch_nand(volatile __tsan_atomic8* a,
    __tsan_atomic8 v, __tsan_memory_order mo);
__tsan_atomic16 __tsan_atomic16_fetch_nand(volatile __tsan_atomic16* a,
    __tsan_atomic16 v, __tsan_memory_order mo);
__tsan_atomic32 __tsan_atomic32_fetch_nand(volatile __tsan_atomic32* a,
    __tsan_atomic32 v, __tsan_memory_order mo);
__tsan_atomic64 __tsan_atomic64_fetch_nand(volatile __tsan_atomic64* a,
    __tsan_atomic64 v, __tsan_memory_order mo);
__tsan_atomic128 __tsan_atomic128_fetch_nand(volatile __tsan_atomic128* a,
    __tsan_atomic128 v, __tsan_memory_order mo);

int __tsan_atomic8_compare_exchange_weak(volatile __tsan_atomic8* a,
    __tsan_atomic8* c, __tsan_atomic8 v, __tsan_memory_order mo,
    __tsan_memory_order fail_mo);
int __tsan_atomic16_compare_exchange_weak(volatile __tsan_atomic16* a,
    __tsan_atomic16* c, __tsan_atomic16 v, __tsan_memory_order mo,
    __tsan_memory_order fail_mo);
int __tsan_atomic32_compare_exchange_weak(volatile __tsan_atomic32* a,
    __tsan_atomic32* c, __tsan_atomic32 v, __tsan_memory_order mo,
    __tsan_memory_order fail_mo);
int __tsan_atomic64_compare_exchange_weak(volatile __tsan_atomic64* a,
    __tsan_atomic64* c, __tsan_atomic64 v, __tsan_memory_order mo,
    __tsan_memory_order fail_mo);
int __tsan_atomic128_compare_exchange_weak(volatile __tsan_atomic128* a,
    __tsan_atomic128* c, __tsan_atomic128 v, __tsan_memory_order mo,
    __tsan_memory_order fail_mo);

int __tsan_atomic8_compare_exchange_strong(volatile __tsan_atomic8* a,
    __tsan_atomic8* c, __tsan_atomic8 v, __tsan_memory_order mo,
    __tsan_memory_order fail_mo);
int __tsan_atomic16_compare_exchange_strong(volatile __tsan_atomic16* a,
    __tsan_atomic16* c, __tsan_atomic16 v, __tsan_memory_order mo,
    __tsan_memory_order fail_mo);
int __tsan_atomic32_compare_exchange_strong(volatile __tsan_atomic32* a,
    __tsan_atomic32* c, __tsan_atomic32 v, __tsan_memory_order mo,
    __tsan_memory_order fail_mo);
int __tsan_atomic64_compare_exchange_strong(volatile __tsan_atomic64* a,
    __tsan_atomic64* c, __tsan_atomic64 v, __tsan_memory_order mo,
    __tsan_memory_order fail_mo);
int __tsan_atomic128_compare_exchange_strong(volatile __tsan_atomic128* a,
    __tsan_atomic128* c, __tsan_atomic128 v, __tsan_memory_order mo,
    __tsan_memory_order fail_mo);

__tsan_atomic8 __tsan_atomic8_compare_exchange_val(
    volatile __tsan_atomic8* a, __tsan_atomic8 c, __tsan_atomic8 v,
    __tsan_memory_order mo, __tsan_memory_order fail_mo);
__tsan_atomic16 __tsan_atomic16_compare_exchange_val(
    volatile __tsan_atomic16* a, __tsan_atomic16 c, __tsan_atomic16 v,
    __tsan_memory_order mo, __tsan_memory_order fail_mo);
__tsan_atomic32 __tsan_atomic32_compare_exchange_val(
    volatile __tsan_atomic32* a, __tsan_atomic32 c, __tsan_atomic32 v,
    __tsan_memory_order mo, __tsan_memory_order fail_mo);
__tsan_atomic64 __tsan_atomic64_compare_exchange_val(
    volatile __tsan_atomic64* a, __tsan_atomic64 c, __tsan_atomic64 v,
    __tsan_memory_order mo, __tsan_memory_order fail_mo);
__tsan_atomic128 __tsan_atomic128_compare_exchange_val(
    volatile __tsan_atomic128* a, __tsan_atomic128 c, __tsan_atomic128 v,
    __tsan_memory_order mo, __tsan_memory_order fail_mo);

void __tsan_atomic_thread_fence(__tsan_memory_order mo);
void __tsan_atomic_signal_fence(__tsan_memory_order mo);
}  // extern "C"

#endif  // #ifndef TSAN_INTERFACE_ATOMIC_H

inline Atomic32 NoBarrier_CompareAndSwap(volatile Atomic32* ptr,
                                         Atomic32 old_value,
                                         Atomic32 new_value) {
  Atomic32 cmp = old_value;
  __tsan_atomic32_compare_exchange_strong(ptr, &cmp, new_value,
      __tsan_memory_order_relaxed, __tsan_memory_order_relaxed);
  return cmp;
}

inline Atomic32 NoBarrier_AtomicExchange(volatile Atomic32* ptr,
                                         Atomic32 new_value) {
  return __tsan_atomic32_exchange(ptr, new_value,
      __tsan_memory_order_relaxed);
}

inline Atomic32 Acquire_AtomicExchange(volatile Atomic32* ptr,
                                       Atomic32 new_value) {
  return __tsan_atomic32_exchange(ptr, new_value,
      __tsan_memory_order_acquire);
}

inline Atomic32 Release_AtomicExchange(volatile Atomic32* ptr,
                                       Atomic32 new_value) {
  return __tsan_atomic32_exchange(ptr, new_value,
      __tsan_memory_order_release);
}

inline Atomic32 NoBarrier_AtomicIncrement(volatile Atomic32* ptr,
                                          Atomic32 increment) {
  return increment + __tsan_atomic32_fetch_add(ptr, increment,
      __tsan_memory_order_relaxed);
}

inline Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  return increment + __tsan_atomic32_fetch_add(ptr, increment,
      __tsan_memory_order_acq_rel);
}

inline Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  Atomic32 cmp = old_value;
  __tsan_atomic32_compare_exchange_strong(ptr, &cmp, new_value,
      __tsan_memory_order_acquire, __tsan_memory_order_acquire);
  return cmp;
}

inline Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  Atomic32 cmp = old_value;
  __tsan_atomic32_compare_exchange_strong(ptr, &cmp, new_value,
      __tsan_memory_order_release, __tsan_memory_order_relaxed);
  return cmp;
}

inline void NoBarrier_Store(volatile Atomic8* ptr, Atomic8 value) {
  __tsan_atomic8_store(ptr, value, __tsan_memory_order_relaxed);
}

inline void NoBarrier_Store(volatile Atomic32* ptr, Atomic32 value) {
  __tsan_atomic32_store(ptr, value, __tsan_memory_order_relaxed);
}

inline void Acquire_Store(volatile Atomic32* ptr, Atomic32 value) {
  __tsan_atomic32_store(ptr, value, __tsan_memory_order_relaxed);
  __tsan_atomic_thread_fence(__tsan_memory_order_seq_cst);
}

inline void Release_Store(volatile Atomic32* ptr, Atomic32 value) {
  __tsan_atomic32_store(ptr, value, __tsan_memory_order_release);
}

inline Atomic8 NoBarrier_Load(volatile const Atomic8* ptr) {
  return __tsan_atomic8_load(ptr, __tsan_memory_order_relaxed);
}

inline Atomic32 NoBarrier_Load(volatile const Atomic32* ptr) {
  return __tsan_atomic32_load(ptr, __tsan_memory_order_relaxed);
}

inline Atomic32 Acquire_Load(volatile const Atomic32* ptr) {
  return __tsan_atomic32_load(ptr, __tsan_memory_order_acquire);
}

inline Atomic32 Release_Load(volatile const Atomic32* ptr) {
  __tsan_atomic_thread_fence(__tsan_memory_order_seq_cst);
  return __tsan_atomic32_load(ptr, __tsan_memory_order_relaxed);
}

inline Atomic64 NoBarrier_CompareAndSwap(volatile Atomic64* ptr,
                                         Atomic64 old_value,
                                         Atomic64 new_value) {
  Atomic64 cmp = old_value;
  __tsan_atomic64_compare_exchange_strong(ptr, &cmp, new_value,
      __tsan_memory_order_relaxed, __tsan_memory_order_relaxed);
  return cmp;
}

inline Atomic64 NoBarrier_AtomicExchange(volatile Atomic64* ptr,
                                         Atomic64 new_value) {
  return __tsan_atomic64_exchange(ptr, new_value, __tsan_memory_order_relaxed);
}

inline Atomic64 Acquire_AtomicExchange(volatile Atomic64* ptr,
                                       Atomic64 new_value) {
  return __tsan_atomic64_exchange(ptr, new_value, __tsan_memory_order_acquire);
}

inline Atomic64 Release_AtomicExchange(volatile Atomic64* ptr,
                                       Atomic64 new_value) {
  return __tsan_atomic64_exchange(ptr, new_value, __tsan_memory_order_release);
}

inline Atomic64 NoBarrier_AtomicIncrement(volatile Atomic64* ptr,
                                          Atomic64 increment) {
  return increment + __tsan_atomic64_fetch_add(ptr, increment,
      __tsan_memory_order_relaxed);
}

inline Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  return increment + __tsan_atomic64_fetch_add(ptr, increment,
      __tsan_memory_order_acq_rel);
}

inline void NoBarrier_Store(volatile Atomic64* ptr, Atomic64 value) {
  __tsan_atomic64_store(ptr, value, __tsan_memory_order_relaxed);
}

inline void Acquire_Store(volatile Atomic64* ptr, Atomic64 value) {
  __tsan_atomic64_store(ptr, value, __tsan_memory_order_relaxed);
  __tsan_atomic_thread_fence(__tsan_memory_order_seq_cst);
}

inline void Release_Store(volatile Atomic64* ptr, Atomic64 value) {
  __tsan_atomic64_store(ptr, value, __tsan_memory_order_release);
}

inline Atomic64 NoBarrier_Load(volatile const Atomic64* ptr) {
  return __tsan_atomic64_load(ptr, __tsan_memory_order_relaxed);
}

inline Atomic64 Acquire_Load(volatile const Atomic64* ptr) {
  return __tsan_atomic64_load(ptr, __tsan_memory_order_acquire);
}

inline Atomic64 Release_Load(volatile const Atomic64* ptr) {
  __tsan_atomic_thread_fence(__tsan_memory_order_seq_cst);
  return __tsan_atomic64_load(ptr, __tsan_memory_order_relaxed);
}

inline Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  Atomic64 cmp = old_value;
  __tsan_atomic64_compare_exchange_strong(ptr, &cmp, new_value,
      __tsan_memory_order_acquire, __tsan_memory_order_acquire);
  return cmp;
}

inline Atomic64 Release_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  Atomic64 cmp = old_value;
  __tsan_atomic64_compare_exchange_strong(ptr, &cmp, new_value,
      __tsan_memory_order_release, __tsan_memory_order_relaxed);
  return cmp;
}

inline void MemoryBarrier() {
  __tsan_atomic_thread_fence(__tsan_memory_order_seq_cst);
}

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_ATOMICOPS_INTERNALS_TSAN_H_
