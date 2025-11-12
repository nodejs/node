// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_ATOMICOPS_H_
#define V8_BASE_ATOMICOPS_H_

#include <stdint.h>

#include <atomic>

#include "src/base/base-export.h"
#include "src/base/macros.h"

#if defined(V8_OS_STARBOARD)
#include "starboard/atomic.h"
#endif  // V8_OS_STARBOARD

namespace v8::base {

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

// Use AtomicWord for a machine-sized pointer. It will use the Atomic32 or
// Atomic64 routines below, depending on your architecture.
#if defined(V8_HOST_ARCH_64_BIT)
using AtomicWord = Atomic64;
#else
using AtomicWord = Atomic32;
#endif
static_assert(sizeof(void*) == sizeof(AtomicWord));

inline void SeqCst_MemoryFence() {
  std::atomic_thread_fence(std::memory_order_seq_cst);
}

template <typename T>
concept AtomicTypeForTrivialOperations =
#if defined(V8_HOST_ARCH_64_BIT)
    std::is_same_v<T, Atomic64> ||
#endif
    std::is_same_v<T, Atomic8> || std::is_same_v<T, Atomic16> ||
    std::is_same_v<T, Atomic32>;

// Atomically execute:
//   result = *ptr;
//   if (result == old_value)
//     *ptr = new_value;
//   return result;
//
// I.e. replace |*ptr| with |new_value| if |*ptr| used to be |old_value|.
// Always return the value of |*ptr| before the operation.
// Acquire, Relaxed, Release correspond to standard C++ memory orders.
template <AtomicTypeForTrivialOperations T>
inline std::type_identity_t<T> Relaxed_CompareAndSwap(
    T* ptr, std::type_identity_t<T> old_value,
    std::type_identity_t<T> new_value) {
  std::atomic_ref<T>(*ptr).compare_exchange_strong(old_value, new_value,
                                                   std::memory_order_relaxed);
  return old_value;
}

template <AtomicTypeForTrivialOperations T>
inline std::type_identity_t<T> AcquireRelease_CompareAndSwap(
    T* ptr, std::type_identity_t<T> old_value,
    std::type_identity_t<T> new_value) {
  std::atomic_ref<T>(*ptr).compare_exchange_strong(old_value, new_value,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire);
  return old_value;
}

template <AtomicTypeForTrivialOperations T>
inline std::type_identity_t<T> Release_CompareAndSwap(
    T* ptr, std::type_identity_t<T> old_value,
    std::type_identity_t<T> new_value) {
  std::atomic_ref<T>(*ptr).compare_exchange_strong(old_value, new_value,
                                                   std::memory_order_release,
                                                   std::memory_order_relaxed);
  return old_value;
}

template <AtomicTypeForTrivialOperations T>
inline std::type_identity_t<T> SeqCst_CompareAndSwap(
    T* ptr, std::type_identity_t<T> old_value,
    std::type_identity_t<T> new_value) {
  std::atomic_ref<T>(*ptr).compare_exchange_strong(old_value, new_value,
                                                   std::memory_order_seq_cst,
                                                   std::memory_order_seq_cst);
  return old_value;
}

template <AtomicTypeForTrivialOperations T>
inline std::type_identity_t<T> Relaxed_AtomicExchange(
    T* ptr, std::type_identity_t<T> new_value) {
  return std::atomic_ref<T>(*ptr).exchange(new_value,
                                           std::memory_order_relaxed);
}

template <AtomicTypeForTrivialOperations T>
inline std::type_identity_t<T> SeqCst_AtomicExchange(
    T* ptr, std::type_identity_t<T> new_value) {
  return std::atomic_ref<T>(*ptr).exchange(new_value,
                                           std::memory_order_seq_cst);
}

template <AtomicTypeForTrivialOperations T>
inline std::type_identity_t<T> Relaxed_FetchOr(T* ptr,
                                               std::type_identity_t<T> bits) {
  return std::atomic_ref<T>(*ptr).fetch_or(bits, std::memory_order_relaxed);
}

template <AtomicTypeForTrivialOperations T>
inline std::type_identity_t<T> Relaxed_AtomicIncrement(
    T* ptr, std::type_identity_t<T> increment) {
  return increment + std::atomic_ref<T>(*ptr).fetch_add(
                         increment, std::memory_order_relaxed);
}

template <AtomicTypeForTrivialOperations T>
inline void Relaxed_Store(T* ptr, std::type_identity_t<T> value) {
  std::atomic_ref<T>(*ptr).store(value, std::memory_order_relaxed);
}

template <AtomicTypeForTrivialOperations T>
inline void Release_Store(T* ptr, std::type_identity_t<T> value) {
  std::atomic_ref<T>(*ptr).store(value, std::memory_order_release);
}

template <AtomicTypeForTrivialOperations T>
inline void SeqCst_Store(T* ptr, std::type_identity_t<T> value) {
  std::atomic_ref<T>(*ptr).store(value, std::memory_order_seq_cst);
}

template <AtomicTypeForTrivialOperations T>
inline T Relaxed_Load(const T* ptr) {
  return std::atomic_ref<T>(*const_cast<T*>(ptr))
      .load(std::memory_order_relaxed);
}

template <AtomicTypeForTrivialOperations T>
inline T Acquire_Load(const T* ptr) {
  return std::atomic_ref<T>(*const_cast<T*>(ptr))
      .load(std::memory_order_acquire);
}

template <AtomicTypeForTrivialOperations T>
inline T SeqCst_Load(const T* ptr) {
  return std::atomic_ref<T>(*const_cast<T*>(ptr))
      .load(std::memory_order_seq_cst);
}

inline void Relaxed_Memcpy(Atomic8* dst, const Atomic8* src, size_t bytes) {
  constexpr size_t kAtomicWordSize = sizeof(AtomicWord);
  while (bytes > 0 &&
         !IsAligned(reinterpret_cast<uintptr_t>(dst), kAtomicWordSize)) {
    Relaxed_Store(dst++, Relaxed_Load(src++));
    --bytes;
  }
  if (IsAligned(reinterpret_cast<uintptr_t>(src), kAtomicWordSize) &&
      IsAligned(reinterpret_cast<uintptr_t>(dst), kAtomicWordSize)) {
    while (bytes >= kAtomicWordSize) {
      Relaxed_Store(reinterpret_cast<AtomicWord*>(dst),
                    Relaxed_Load(reinterpret_cast<const AtomicWord*>(src)));
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

inline void Relaxed_Memmove(Atomic8* dst, const Atomic8* src, size_t bytes) {
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
      Relaxed_Store(reinterpret_cast<AtomicWord*>(dst),
                    Relaxed_Load(reinterpret_cast<const AtomicWord*>(src)));
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

inline int Relaxed_Memcmp(const Atomic8* s1, const Atomic8* s2, size_t len) {
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
      AtomicWord u1 = Relaxed_Load(reinterpret_cast<const AtomicWord*>(s1));
      AtomicWord u2 = Relaxed_Load(reinterpret_cast<const AtomicWord*>(s2));
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

}  // namespace v8::base

#endif  // V8_BASE_ATOMICOPS_H_
