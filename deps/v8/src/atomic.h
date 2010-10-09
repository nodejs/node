// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This module wraps compiler specific syncronisation related intrinsics.

#ifndef V8_ATOMIC_H_
#define V8_ATOMIC_H_

// Avoid warning when compiled with /Wp64.
#ifndef _MSC_VER
#define __w64
#endif
typedef __w64 int32_t Atomic32;
#ifdef V8_TARGET_ARCH_X64
// We need to be able to go between Atomic64 and AtomicWord implicitly.  This
// means Atomic64 and AtomicWord should be the same type on 64-bit.
typedef intptr_t Atomic64;
#endif

// Use AtomicWord for a machine-sized pointer.  It will use the Atomic32 or
// Atomic64 routines below, depending on your architecture.
typedef intptr_t AtomicWord;

inline void AtomicAdd(volatile Atomic32* ptr, Atomic32 value);
inline void AtomicOr(volatile Atomic32* ptr, Atomic32 value);
inline void AtomicAnd(volatile Atomic32* ptr, Atomic32 value);
inline bool AtomicCompareAndSwap(volatile Atomic32* ptr,
                                 Atomic32 old_value,
                                 Atomic32 new_value);

#if defined(V8_TARGET_ARCH_X64)
inline bool AtomicCompareAndSwap(volatile Atomic64* ptr,
                                 Atomic64 old_value,
                                 Atomic64 new_value);
#endif


#if defined(V8_TARGET_ARCH_IA32) || defined(V8_TARGET_ARCH_X64)

// Microsoft Visual C++ specific stuff.
#ifdef _MSC_VER
#if (_MSC_VER >= 1500)
#include <intrin.h>
#else
// For older versions we have to provide intrisic signatures.
long _InterlockedExchangeAdd (long volatile* Addend, long Value);
long _InterlockedOr (long volatile* Value, long Mask);
long _InterlockedAnd (long volatile *Value, long Mask);
long _InterlockedCompareExchange (long volatile* Destination,
                                  long Exchange,
                                  long Comperand);

#pragma intrinsic(_InterlockedExchangeAdd)
#pragma intrinsic(_InterlockedOr)
#pragma intrinsic(_InterlockedAnd)
#pragma intrinsic(_InterlockedCompareExchange)
#endif

inline void AtomicAdd(volatile Atomic32* ptr, Atomic32 value) {
  _InterlockedExchangeAdd(reinterpret_cast<long volatile*>(ptr),
                          static_cast<long>(value));
}

inline void AtomicOr(volatile Atomic32* ptr, Atomic32 value)  {
  _InterlockedOr(reinterpret_cast<long volatile*>(ptr),
                 static_cast<long>(value));
}

inline void AtomicAnd(volatile Atomic32* ptr, Atomic32 value)  {
  _InterlockedAnd(reinterpret_cast<long volatile*>(ptr),
                  static_cast<long>(value));
}

inline bool AtomicCompareAndSwap(volatile Atomic32* ptr,
                                 Atomic32 old_value,
                                 Atomic32 new_value) {
  long result = _InterlockedCompareExchange(
      reinterpret_cast<long volatile*>(ptr),
      static_cast<long>(new_value),
      static_cast<long>(old_value));
  return result == static_cast<long>(old_value);
}

#if defined(V8_TARGET_ARCH_X64)
inline bool AtomicCompareAndSwap(volatile Atomic64* ptr,
                                 Atomic64 old_value,
                                 Atomic64 new_value) {

  __int64 result = _InterlockedCompareExchange_64(
      reinterpret_cast<__int64 volatile*>(ptr),
      static_cast<__int64>(new_value),
      static_cast<__int64>(old_value));
  return result == static_cast<__int64>(old_value);
}
#endif

#define ATOMIC_SUPPORTED 1

#endif  // _MSC_VER

// GCC specific stuff
#ifdef __GNUC__
inline void AtomicAdd(volatile Atomic32* ptr, Atomic32 value) {
  __sync_fetch_and_add(ptr, value);
}

inline void AtomicOr(volatile Atomic32* ptr, Atomic32 value)  {
  __sync_fetch_and_or(ptr, value);
}

inline void AtomicAnd(volatile Atomic32* ptr, Atomic32 value)  {
  __sync_fetch_and_and(ptr, value);
}

inline bool AtomicCompareAndSwap(volatile Atomic32* ptr,
                                 Atomic32 old_value,
                                 Atomic32 new_value) {
  return __sync_bool_compare_and_swap(ptr, old_value, new_value);
}

#if defined(V8_TARGET_ARCH_X64)
inline bool AtomicCompareAndSwap(volatile Atomic64* ptr,
                                 Atomic64 old_value,
                                 Atomic64 new_value) {
  return __sync_bool_compare_and_swap(ptr, old_value, new_value);
}
#endif

#define ATOMIC_SUPPORTED 1
#endif

#endif  // defined(V8_TARGET_ARCH_IA32) || defined(V8_TARGET_ARCH_X64)

#ifndef ATOMIC_SUPPORTED
inline void AtomicAdd(volatile Atomic32* ptr, Atomic32 value) {
  *ptr += value;
}

inline void AtomicOr(volatile Atomic32* ptr, Atomic32 value)  {
  *ptr |= value;
}

inline void AtomicAnd(volatile Atomic32* ptr, Atomic32 value)  {
  *ptr &= value;
}

inline bool AtomicCompareAndSwap(volatile Atomic32* ptr,
                                 Atomic32 old_value,
                                 Atomic32 new_value) {
  if (*ptr == old_value) {
    *ptr = new_value;
    return true;
  }
  return false;
}

#if defined(V8_TARGET_ARCH_X64)
inline bool AtomicCompareAndSwap(volatile Atomic64* ptr,
                                 Atomic64 old_value,
                                 Atomic64 new_value) {
  if (*ptr == old_value) {
    *ptr = new_value;
    return true;
  }
  return false;
}
#endif

#define ATOMIC_SUPPORTED 0
#endif


#endif
