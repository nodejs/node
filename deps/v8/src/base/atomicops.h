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

#include <atomic>

// Small C++ header which defines implementation specific macros used to
// identify the STL implementation.
// - libc++: captures __config for _LIBCPP_VERSION
// - libstdc++: captures bits/c++config.h for __GLIBCXX__
#include <cstddef>

#include "src/base/base-export.h"
#include "src/base/build_config.h"
#include "src/base/macros.h"

#if defined(V8_OS_STARBOARD)
#include "starboard/atomic.h"
#endif  // V8_OS_STARBOARD

namespace v8 {
namespace base {

#ifdef V8_OS_STARBOARD
using Atomic8 = SbAtomic8;
using Atomic16 = int16_t;
using Atomic32 = SbAtomic32;
#if SB_IS_64_BIT
using Atomic64 = SbAtomic64;
#endif
#else
using Atomic8 = char;
using Atomic16 = int16_t;
using Atomic32 = int32_t;
#if defined(V8_HOST_ARCH_64_BIT)
// We need to be able to go between Atomic64 and AtomicWord implicitly.  This
// means Atomic64 and AtomicWord should be the same type on 64-bit.
#if defined(__ILP32__)
using Atomic64 = int64_t;
#else
using Atomic64 = intptr_t;
#endif  // defined(__ILP32__)
#endif  // defined(V8_HOST_ARCH_64_BIT)
#endif  // V8_OS_STARBOARD

// Use AtomicWord for a machine-sized pointer.  It will use the Atomic32 or
// Atomic64 routines below, depending on your architecture.
#if defined(V8_OS_STARBOARD)
using AtomicWord = SbAtomicPtr;
#else
using AtomicWord = intptr_t;
#endif

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

// Atomically execute:
//   result = *ptr;
//   if (result == old_value)
//     *ptr = new_value;
//   return result;
//
// I.e. replace |*ptr| with |new_value| if |*ptr| used to be |old_value|.
// Always return the value of |*ptr| before the operation.
// Acquire, Relaxed, Release correspond to standard C++ memory orders.
inline Atomic8 Relaxed_CompareAndSwap(volatile Atomic8* ptr, Atomic8 old_value,
                                      Atomic8 new_value) {
  std::atomic_compare_exchange_strong_explicit(
      helper::to_std_atomic(ptr), &old_value, new_value,
      std::memory_order_relaxed, std::memory_order_relaxed);
  return old_value;
}

inline Atomic16 Relaxed_CompareAndSwap(volatile Atomic16* ptr,
                                       Atomic16 old_value, Atomic16 new_value) {
  std::atomic_compare_exchange_strong_explicit(
      helper::to_std_atomic(ptr), &old_value, new_value,
      std::memory_order_relaxed, std::memory_order_relaxed);
  return old_value;
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

inline Atomic32 SeqCst_AtomicExchange(volatile Atomic32* ptr,
                                      Atomic32 new_value) {
  return std::atomic_exchange_explicit(helper::to_std_atomic(ptr), new_value,
                                       std::memory_order_seq_cst);
}

inline Atomic32 Relaxed_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  return increment + std::atomic_fetch_add_explicit(helper::to_std_atomic(ptr),
                                                    increment,
                                                    std::memory_order_relaxed);
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

inline Atomic32 AcquireRelease_CompareAndSwap(volatile Atomic32* ptr,
                                              Atomic32 old_value,
                                              Atomic32 new_value) {
  atomic_compare_exchange_strong_explicit(
      helper::to_std_atomic(ptr), &old_value, new_value,
      std::memory_order_acq_rel, std::memory_order_acquire);
  return old_value;
}

inline void Relaxed_Store(volatile Atomic8* ptr, Atomic8 value) {
  std::atomic_store_explicit(helper::to_std_atomic(ptr), value,
                             std::memory_order_relaxed);
}

inline void Relaxed_Store(volatile Atomic16* ptr, Atomic16 value) {
  std::atomic_store_explicit(helper::to_std_atomic(ptr), value,
                             std::memory_order_relaxed);
}

inline void Relaxed_Store(volatile Atomic32* ptr, Atomic32 value) {
  std::atomic_store_explicit(helper::to_std_atomic(ptr), value,
                             std::memory_order_relaxed);
}

inline void Release_Store(volatile Atomic8* ptr, Atomic8 value) {
  std::atomic_store_explicit(helper::to_std_atomic(ptr), value,
                             std::memory_order_release);
}

inline void Release_Store(volatile Atomic16* ptr, Atomic16 value) {
  std::atomic_store_explicit(helper::to_std_atomic(ptr), value,
                             std::memory_order_release);
}

inline void Release_Store(volatile Atomic32* ptr, Atomic32 value) {
  std::atomic_store_explicit(helper::to_std_atomic(ptr), value,
                             std::memory_order_release);
}

inline void SeqCst_Store(volatile Atomic8* ptr, Atomic8 value) {
  std::atomic_store_explicit(helper::to_std_atomic(ptr), value,
                             std::memory_order_seq_cst);
}

inline void SeqCst_Store(volatile Atomic16* ptr, Atomic16 value) {
  std::atomic_store_explicit(helper::to_std_atomic(ptr), value,
                             std::memory_order_seq_cst);
}

inline void SeqCst_Store(volatile Atomic32* ptr, Atomic32 value) {
  std::atomic_store_explicit(helper::to_std_atomic(ptr), value,
                             std::memory_order_seq_cst);
}

inline Atomic8 Relaxed_Load(volatile const Atomic8* ptr) {
  return std::atomic_load_explicit(helper::to_std_atomic_const(ptr),
                                   std::memory_order_relaxed);
}

inline Atomic16 Relaxed_Load(volatile const Atomic16* ptr) {
  return std::atomic_load_explicit(helper::to_std_atomic_const(ptr),
                                   std::memory_order_relaxed);
}

inline Atomic32 Relaxed_Load(volatile const Atomic32* ptr) {
  return std::atomic_load_explicit(helper::to_std_atomic_const(ptr),
                                   std::memory_order_relaxed);
}

inline Atomic8 Acquire_Load(volatile const Atomic8* ptr) {
  return std::atomic_load_explicit(helper::to_std_atomic_const(ptr),
                                   std::memory_order_acquire);
}

inline Atomic32 Acquire_Load(volatile const Atomic32* ptr) {
  return std::atomic_load_explicit(helper::to_std_atomic_const(ptr),
                                   std::memory_order_acquire);
}

inline Atomic8 SeqCst_Load(volatile const Atomic8* ptr) {
  return std::atomic_load_explicit(helper::to_std_atomic_const(ptr),
                                   std::memory_order_seq_cst);
}

inline Atomic32 SeqCst_Load(volatile const Atomic32* ptr) {
  return std::atomic_load_explicit(helper::to_std_atomic_const(ptr),
                                   std::memory_order_seq_cst);
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

inline Atomic64 SeqCst_AtomicExchange(volatile Atomic64* ptr,
                                      Atomic64 new_value) {
  return std::atomic_exchange_explicit(helper::to_std_atomic(ptr), new_value,
                                       std::memory_order_seq_cst);
}

inline Atomic64 Relaxed_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  return increment + std::atomic_fetch_add_explicit(helper::to_std_atomic(ptr),
                                                    increment,
                                                    std::memory_order_relaxed);
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

inline Atomic64 AcquireRelease_CompareAndSwap(volatile Atomic64* ptr,
                                              Atomic64 old_value,
                                              Atomic64 new_value) {
  std::atomic_compare_exchange_strong_explicit(
      helper::to_std_atomic(ptr), &old_value, new_value,
      std::memory_order_acq_rel, std::memory_order_acquire);
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

inline void SeqCst_Store(volatile Atomic64* ptr, Atomic64 value) {
  std::atomic_store_explicit(helper::to_std_atomic(ptr), value,
                             std::memory_order_seq_cst);
}

inline Atomic64 Relaxed_Load(volatile const Atomic64* ptr) {
  return std::atomic_load_explicit(helper::to_std_atomic_const(ptr),
                                   std::memory_order_relaxed);
}

inline Atomic64 Acquire_Load(volatile const Atomic64* ptr) {
  return std::atomic_load_explicit(helper::to_std_atomic_const(ptr),
                                   std::memory_order_acquire);
}

inline Atomic64 SeqCst_Load(volatile const Atomic64* ptr) {
  return std::atomic_load_explicit(helper::to_std_atomic_const(ptr),
                                   std::memory_order_seq_cst);
}

#endif  // defined(V8_HOST_ARCH_64_BIT)

inline void Relaxed_Memcpy(volatile Atomic8* dst, volatile const Atomic8* src,
                           size_t bytes) {
  constexpr size_t kAtomicWordSize = sizeof(AtomicWord);
  while (bytes > 0 &&
         !IsAligned(reinterpret_cast<uintptr_t>(dst), kAtomicWordSize)) {
    Relaxed_Store(dst++, Relaxed_Load(src++));
    --bytes;
  }
  if (IsAligned(reinterpret_cast<uintptr_t>(src), kAtomicWordSize) &&
      IsAligned(reinterpret_cast<uintptr_t>(dst), kAtomicWordSize)) {
    while (bytes >= kAtomicWordSize) {
      Relaxed_Store(
          reinterpret_cast<volatile AtomicWord*>(dst),
          Relaxed_Load(reinterpret_cast<const volatile AtomicWord*>(src)));
      dst += kAtomicWordSize;
      src += kAtomicWordSize;
      bytes -= kAtomicWordSize;
    }
  }
  while (bytes > 0) {
    Relaxed_Store(dst++, Relaxed_Load(src++));
    --bytes;
  }
}

inline void Relaxed_Memmove(volatile Atomic8* dst, volatile const Atomic8* src,
                            size_t bytes) {
  // Use Relaxed_Memcpy if copying forwards is safe. This is the case if there
  // is no overlap, or {dst} lies before {src}.
  // This single check checks for both:
  if (reinterpret_cast<uintptr_t>(dst) - reinterpret_cast<uintptr_t>(src) >=
      bytes) {
    Relaxed_Memcpy(dst, src, bytes);
    return;
  }

  // Otherwise copy backwards.
  dst += bytes;
  src += bytes;
  constexpr size_t kAtomicWordSize = sizeof(AtomicWord);
  while (bytes > 0 &&
         !IsAligned(reinterpret_cast<uintptr_t>(dst), kAtomicWordSize)) {
    Relaxed_Store(--dst, Relaxed_Load(--src));
    --bytes;
  }
  if (IsAligned(reinterpret_cast<uintptr_t>(src), kAtomicWordSize) &&
      IsAligned(reinterpret_cast<uintptr_t>(dst), kAtomicWordSize)) {
    while (bytes >= kAtomicWordSize) {
      dst -= kAtomicWordSize;
      src -= kAtomicWordSize;
      bytes -= kAtomicWordSize;
      Relaxed_Store(
          reinterpret_cast<volatile AtomicWord*>(dst),
          Relaxed_Load(reinterpret_cast<const volatile AtomicWord*>(src)));
    }
  }
  while (bytes > 0) {
    Relaxed_Store(--dst, Relaxed_Load(--src));
    --bytes;
  }
}

namespace helper {
inline int MemcmpNotEqualFundamental(Atomic8 u1, Atomic8 u2) {
  DCHECK_NE(u1, u2);
  return u1 < u2 ? -1 : 1;
}
inline int MemcmpNotEqualFundamental(AtomicWord u1, AtomicWord u2) {
  DCHECK_NE(u1, u2);
#if defined(V8_TARGET_BIG_ENDIAN)
  return u1 < u2 ? -1 : 1;
#else
  for (size_t i = 0; i < sizeof(AtomicWord); ++i) {
    uint8_t byte1 = u1 & 0xFF;
    uint8_t byte2 = u2 & 0xFF;
    if (byte1 != byte2) return byte1 < byte2 ? -1 : 1;
    u1 >>= 8;
    u2 >>= 8;
  }
  UNREACHABLE();
#endif
}
}  // namespace helper

inline int Relaxed_Memcmp(volatile const Atomic8* s1,
                          volatile const Atomic8* s2, size_t len) {
  constexpr size_t kAtomicWordSize = sizeof(AtomicWord);
  while (len > 0 &&
         !(IsAligned(reinterpret_cast<uintptr_t>(s1), kAtomicWordSize) &&
           IsAligned(reinterpret_cast<uintptr_t>(s2), kAtomicWordSize))) {
    Atomic8 u1 = Relaxed_Load(s1++);
    Atomic8 u2 = Relaxed_Load(s2++);
    if (u1 != u2) return helper::MemcmpNotEqualFundamental(u1, u2);
    --len;
  }

  if (IsAligned(reinterpret_cast<uintptr_t>(s1), kAtomicWordSize) &&
      IsAligned(reinterpret_cast<uintptr_t>(s2), kAtomicWordSize)) {
    while (len >= kAtomicWordSize) {
      AtomicWord u1 =
          Relaxed_Load(reinterpret_cast<const volatile AtomicWord*>(s1));
      AtomicWord u2 =
          Relaxed_Load(reinterpret_cast<const volatile AtomicWord*>(s2));
      if (u1 != u2) return helper::MemcmpNotEqualFundamental(u1, u2);
      s1 += kAtomicWordSize;
      s2 += kAtomicWordSize;
      len -= kAtomicWordSize;
    }
  }

  while (len > 0) {
    Atomic8 u1 = Relaxed_Load(s1++);
    Atomic8 u2 = Relaxed_Load(s2++);
    if (u1 != u2) return helper::MemcmpNotEqualFundamental(u1, u2);
    --len;
  }

  return 0;
}

}  // namespace base
}  // namespace v8

// On some platforms we need additional declarations to make
// AtomicWord compatible with our other Atomic* types.
#if defined(V8_OS_DARWIN) || defined(V8_OS_OPENBSD) || defined(V8_OS_AIX)
#include "src/base/atomicops_internals_atomicword_compat.h"
#endif

#endif  // V8_BASE_ATOMICOPS_H_
