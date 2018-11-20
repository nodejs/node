// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is an internal atomic implementation, use atomicops.h instead.

#ifndef V8_BASE_ATOMICOPS_INTERNALS_MIPS_GCC_H_
#define V8_BASE_ATOMICOPS_INTERNALS_MIPS_GCC_H_

namespace v8 {
namespace base {

// Atomically execute:
//      result = *ptr;
//      if (*ptr == old_value)
//        *ptr = new_value;
//      return result;
//
// I.e., replace "*ptr" with "new_value" if "*ptr" used to be "old_value".
// Always return the old value of "*ptr"
//
// This routine implies no memory barriers.
inline Atomic32 NoBarrier_CompareAndSwap(volatile Atomic32* ptr,
                                         Atomic32 old_value,
                                         Atomic32 new_value) {
  Atomic32 prev, tmp;
  __asm__ __volatile__(".set push\n"
                       ".set noreorder\n"
                       "1:\n"
                       "ll %0, 0(%4)\n"  // prev = *ptr
                       "bne %0, %2, 2f\n"  // if (prev != old_value) goto 2
                       "move %1, %3\n"  // tmp = new_value
                       "sc %1, 0(%4)\n"  // *ptr = tmp (with atomic check)
                       "beqz %1, 1b\n"  // start again on atomic error
                       "nop\n"  // delay slot nop
                       "2:\n"
                       ".set pop\n"
                       : "=&r" (prev), "=&r" (tmp)
                       : "r" (old_value), "r" (new_value), "r" (ptr)
                       : "memory");
  return prev;
}

// Atomically store new_value into *ptr, returning the previous value held in
// *ptr.  This routine implies no memory barriers.
inline Atomic32 NoBarrier_AtomicExchange(volatile Atomic32* ptr,
                                         Atomic32 new_value) {
  Atomic32 temp, old;
  __asm__ __volatile__(".set push\n"
                       ".set noreorder\n"
                       ".set at\n"
                       "1:\n"
                       "ll %1, 0(%3)\n"  // old = *ptr
                       "move %0, %2\n"  // temp = new_value
                       "sc %0, 0(%3)\n"  // *ptr = temp (with atomic check)
                       "beqz %0, 1b\n"  // start again on atomic error
                       "nop\n"  // delay slot nop
                       ".set pop\n"
                       : "=&r" (temp), "=&r" (old)
                       : "r" (new_value), "r" (ptr)
                       : "memory");

  return old;
}

// Atomically increment *ptr by "increment".  Returns the new value of
// *ptr with the increment applied.  This routine implies no memory barriers.
inline Atomic32 NoBarrier_AtomicIncrement(volatile Atomic32* ptr,
                                          Atomic32 increment) {
  Atomic32 temp, temp2;

  __asm__ __volatile__(".set push\n"
                       ".set noreorder\n"
                       "1:\n"
                       "ll %0, 0(%3)\n"  // temp = *ptr
                       "addu %1, %0, %2\n"  // temp2 = temp + increment
                       "sc %1, 0(%3)\n"  // *ptr = temp2 (with atomic check)
                       "beqz %1, 1b\n"  // start again on atomic error
                       "addu %1, %0, %2\n"  // temp2 = temp + increment
                       ".set pop\n"
                       : "=&r" (temp), "=&r" (temp2)
                       : "Ir" (increment), "r" (ptr)
                       : "memory");
  // temp2 now holds the final value.
  return temp2;
}

inline Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  MemoryBarrier();
  Atomic32 res = NoBarrier_AtomicIncrement(ptr, increment);
  MemoryBarrier();
  return res;
}

// "Acquire" operations
// ensure that no later memory access can be reordered ahead of the operation.
// "Release" operations ensure that no previous memory access can be reordered
// after the operation.  "Barrier" operations have both "Acquire" and "Release"
// semantics.   A MemoryBarrier() has "Barrier" semantics, but does no memory
// access.
inline Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  Atomic32 res = NoBarrier_CompareAndSwap(ptr, old_value, new_value);
  MemoryBarrier();
  return res;
}

inline Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  MemoryBarrier();
  return NoBarrier_CompareAndSwap(ptr, old_value, new_value);
}

inline void NoBarrier_Store(volatile Atomic8* ptr, Atomic8 value) {
  *ptr = value;
}

inline void NoBarrier_Store(volatile Atomic32* ptr, Atomic32 value) {
  *ptr = value;
}

inline void MemoryBarrier() {
  __asm__ __volatile__("sync" : : : "memory");
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
  MemoryBarrier();
  return value;
}

inline Atomic32 Release_Load(volatile const Atomic32* ptr) {
  MemoryBarrier();
  return *ptr;
}

} }  // namespace v8::base

#endif  // V8_BASE_ATOMICOPS_INTERNALS_MIPS_GCC_H_
