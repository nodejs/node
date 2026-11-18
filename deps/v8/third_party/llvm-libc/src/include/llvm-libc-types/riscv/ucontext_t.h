//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of type ucontext_t for RISC-V. The definitions in this file are
/// based on the Linux kernel ABI.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_RISCV_UCONTEXT_T_H
#define LLVM_LIBC_TYPES_RISCV_UCONTEXT_T_H

#include "../sigset_t.h"
#include "../stack_t.h"
#include "mcontext_t.h"

// The layout of ucontext_t must match the layout of 'struct ucontext' in the
// Linux kernel (see arch/riscv/include/uapi/asm/ucontext.h).
typedef struct ucontext_t {
  unsigned long uc_flags;
  struct ucontext_t *uc_link;
  stack_t uc_stack;
  sigset_t uc_sigmask;
  // The kernel reserves space for extending the signal mask to 1024 bits.
  unsigned char __reserved[1024 / 8 - sizeof(sigset_t)];
  // uc_mcontext is the last member, as the kernel may store additional (e.g.
  // vector) state after it. Its 16-byte alignment (inherited from
  // __riscv_mc_q_ext_state) leaves an implicit gap of 8 (RV64) or 12 (RV32)
  // bytes after __reserved.
  mcontext_t uc_mcontext;
} ucontext_t;

#endif // LLVM_LIBC_TYPES_RISCV_UCONTEXT_T_H
