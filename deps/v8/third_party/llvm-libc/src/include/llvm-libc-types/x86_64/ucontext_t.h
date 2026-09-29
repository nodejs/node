//===-- Definition of type ucontext_t -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// Note: Definitions in this file are based on the Linux kernel ABI.

#ifndef LLVM_LIBC_TYPES_X86_64_UCONTEXT_T_H
#define LLVM_LIBC_TYPES_X86_64_UCONTEXT_T_H

#include "../sigset_t.h"
#include "../stack_t.h"
#include "mcontext_t.h"

typedef struct ucontext_t {
  // The following fields must match the Linux kernel's struct ucontext
  // on x86_64 to ensure ABI compatibility for signal handling.
  unsigned long uc_flags;
  struct ucontext_t *uc_link;
  stack_t uc_stack;
  mcontext_t uc_mcontext;
  sigset_t uc_sigmask;

  // Additional fields appended by the C library. These are not part of the
  // kernel's struct ucontext, but are needed for user-space context management.
  // Since they are at the end, they do not break ABI compatibility with the
  // kernel.

  // On x86_64, uc_mcontext contains a pointer to the floating point state
  // rather than the state itself. To make ucontext_t self-contained, we
  // provide space here for the FP state, and the pointer in uc_mcontext
  // can be set to point here. 64 long ints provide 512 bytes, which is
  // the size required for FXSAVE.
  _Alignas(16) long int __fpregs_mem[64];

  // Support for Shadow Stack Pointer (Intel CET).
  unsigned long long __ssp[4];
} ucontext_t;

#endif // LLVM_LIBC_TYPES_X86_64_UCONTEXT_T_H
