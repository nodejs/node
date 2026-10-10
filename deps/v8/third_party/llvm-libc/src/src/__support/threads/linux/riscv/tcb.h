//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines the Thread Control Block (TCB) for Linux RISC-V.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_RISCV_TCB_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_RISCV_TCB_H

#include "hdr/stdint_proxy.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/threads/thread_attributes.h"

namespace LIBC_NAMESPACE_DECL {

struct ThreadControlBlock {
  uintptr_t dtv;
  ThreadAttributes *attrib;
};

LIBC_INLINE ThreadControlBlock *get_tcb() {
  ThreadControlBlock *tp;
  asm("mv %0, tp" : "=r"(tp));
  return tp - 1;
}

LIBC_INLINE ThreadControlBlock *get_tcb(uintptr_t tp) {
  return reinterpret_cast<ThreadControlBlock *>(tp) - 1;
}

LIBC_INLINE ThreadAttributes *get_current_thread_attrib() {
  return get_tcb()->attrib;
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_RISCV_TCB_H
