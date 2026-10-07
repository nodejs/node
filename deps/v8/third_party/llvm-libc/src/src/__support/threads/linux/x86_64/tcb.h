//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines the Thread Control Block (TCB) for Linux x86_64.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_X86_64_TCB_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_X86_64_TCB_H

#include "hdr/offsetof_macros.h"
#include "hdr/stdint_proxy.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/threads/thread_attributes.h"

namespace LIBC_NAMESPACE_DECL {

struct ThreadControlBlock {
  uintptr_t self;
  uintptr_t dtv;
  ThreadAttributes *attrib;
  uintptr_t reserved[2];
  uintptr_t stack_guard;
};
static_assert(offsetof(ThreadControlBlock, stack_guard) == 0x28,
              "Offset defined by the ABI");

LIBC_INLINE ThreadControlBlock *get_tcb() {
  ThreadControlBlock *tcb;
  asm("mov %%fs:0, %0" : "=r"(tcb));
  return tcb;
}

LIBC_INLINE ThreadControlBlock *get_tcb(uintptr_t tp) {
  return reinterpret_cast<ThreadControlBlock *>(tp);
}

LIBC_INLINE ThreadAttributes *get_current_thread_attrib() {
  ThreadAttributes *attrib;
  asm("mov %%fs:%c1, %0"
      : "=r"(attrib)
      : "i"(offsetof(ThreadControlBlock, attrib)));
  return attrib;
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_LINUX_X86_64_TCB_H
