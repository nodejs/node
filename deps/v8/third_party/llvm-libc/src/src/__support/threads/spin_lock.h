//===-- TTAS Spin Lock ----------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_SPIN_LOCK_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_SPIN_LOCK_H

#include "src/__support/CPP/atomic.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/threads/sleep.h"

namespace LIBC_NAMESPACE_DECL {

class SpinLock {
  cpp::Atomic<unsigned char> flag;

public:
  LIBC_INLINE constexpr SpinLock() : flag{0} {}
  LIBC_INLINE bool try_lock() {
    return !flag.exchange(1u, cpp::MemoryOrder::ACQUIRE);
  }
  LIBC_INLINE void lock() {
    // clang-format off
    // For normal TTAS, this compiles to the following on armv9a and x86_64:
    //         mov     w8, #1            |          .LBB0_1:
    // .LBB0_1:                          |                  mov     al, 1
    //         swpab   w8, w9, [x0]      |                  xchg    byte ptr [rdi], al
    //         tbnz    w9, #0, .LBB0_3   |                  test    al, 1
    //         b       .LBB0_4           |                  jne     .LBB0_3
    // .LBB0_2:                          |                  jmp     .LBB0_4
    //         isb                       |         .LBB0_2:
    // .LBB0_3:                          |                  pause
    //         ldrb    w9, [x0]          |         .LBB0_3:
    //         tbnz    w9, #0, .LBB0_2   |                  movzx   eax, byte ptr [rdi]
    //         b       .LBB0_1           |                  test    al, 1
    // .LBB0_4:                          |                  jne     .LBB0_2
    //         ret                       |                  jmp     .LBB0_1
    //                                   |          .LBB0_4:
    //                                   |                  ret
    // clang-format on
    // Notice that inside the busy loop .LBB0_2 and .LBB0_3, only instructions
    // with load semantics are used. swpab/xchg is only issued in outer loop
    // .LBB0_1. This is useful to avoid extra write traffic. The cache
    // coherence guarantees "write propagation", so even if the inner loop only
    // reads with relaxed ordering, the thread will evetually see the write.
    while (!try_lock())
      while (flag.load(cpp::MemoryOrder::RELAXED))
        sleep_briefly();
  }
  LIBC_INLINE void unlock() { flag.store(0u, cpp::MemoryOrder::RELEASE); }
  LIBC_INLINE bool is_locked() { return flag.load(cpp::MemoryOrder::ACQUIRE); }
  LIBC_INLINE bool is_invalid() {
    return flag.load(cpp::MemoryOrder::ACQUIRE) > 1;
  }
  // poison the lock
  LIBC_INLINE ~SpinLock() { flag.store(0xffu, cpp::MemoryOrder::RELEASE); }
};

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_SPIN_LOCK_H
