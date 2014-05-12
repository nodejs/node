// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is an internal atomic implementation, use atomicops.h instead.

#ifndef V8_ATOMICOPS_INTERNALS_ATOMICWORD_COMPAT_H_
#define V8_ATOMICOPS_INTERNALS_ATOMICWORD_COMPAT_H_

// AtomicWord is a synonym for intptr_t, and Atomic32 is a synonym for int32,
// which in turn means int. On some LP32 platforms, intptr_t is an int, but
// on others, it's a long. When AtomicWord and Atomic32 are based on different
// fundamental types, their pointers are incompatible.
//
// This file defines function overloads to allow both AtomicWord and Atomic32
// data to be used with this interface.
//
// On LP64 platforms, AtomicWord and Atomic64 are both always long,
// so this problem doesn't occur.

#if !defined(V8_HOST_ARCH_64_BIT)

namespace v8 {
namespace internal {

inline AtomicWord NoBarrier_CompareAndSwap(volatile AtomicWord* ptr,
                                           AtomicWord old_value,
                                           AtomicWord new_value) {
  return NoBarrier_CompareAndSwap(
      reinterpret_cast<volatile Atomic32*>(ptr), old_value, new_value);
}

inline AtomicWord NoBarrier_AtomicExchange(volatile AtomicWord* ptr,
                                           AtomicWord new_value) {
  return NoBarrier_AtomicExchange(
      reinterpret_cast<volatile Atomic32*>(ptr), new_value);
}

inline AtomicWord NoBarrier_AtomicIncrement(volatile AtomicWord* ptr,
                                            AtomicWord increment) {
  return NoBarrier_AtomicIncrement(
      reinterpret_cast<volatile Atomic32*>(ptr), increment);
}

inline AtomicWord Barrier_AtomicIncrement(volatile AtomicWord* ptr,
                                          AtomicWord increment) {
  return Barrier_AtomicIncrement(
      reinterpret_cast<volatile Atomic32*>(ptr), increment);
}

inline AtomicWord Acquire_CompareAndSwap(volatile AtomicWord* ptr,
                                         AtomicWord old_value,
                                         AtomicWord new_value) {
  return v8::internal::Acquire_CompareAndSwap(
      reinterpret_cast<volatile Atomic32*>(ptr), old_value, new_value);
}

inline AtomicWord Release_CompareAndSwap(volatile AtomicWord* ptr,
                                         AtomicWord old_value,
                                         AtomicWord new_value) {
  return v8::internal::Release_CompareAndSwap(
      reinterpret_cast<volatile Atomic32*>(ptr), old_value, new_value);
}

inline void NoBarrier_Store(volatile AtomicWord *ptr, AtomicWord value) {
  NoBarrier_Store(
      reinterpret_cast<volatile Atomic32*>(ptr), value);
}

inline void Acquire_Store(volatile AtomicWord* ptr, AtomicWord value) {
  return v8::internal::Acquire_Store(
      reinterpret_cast<volatile Atomic32*>(ptr), value);
}

inline void Release_Store(volatile AtomicWord* ptr, AtomicWord value) {
  return v8::internal::Release_Store(
      reinterpret_cast<volatile Atomic32*>(ptr), value);
}

inline AtomicWord NoBarrier_Load(volatile const AtomicWord *ptr) {
  return NoBarrier_Load(
      reinterpret_cast<volatile const Atomic32*>(ptr));
}

inline AtomicWord Acquire_Load(volatile const AtomicWord* ptr) {
  return v8::internal::Acquire_Load(
      reinterpret_cast<volatile const Atomic32*>(ptr));
}

inline AtomicWord Release_Load(volatile const AtomicWord* ptr) {
  return v8::internal::Release_Load(
      reinterpret_cast<volatile const Atomic32*>(ptr));
}

} }  // namespace v8::internal

#endif  // !defined(V8_HOST_ARCH_64_BIT)

#endif  // V8_ATOMICOPS_INTERNALS_ATOMICWORD_COMPAT_H_
