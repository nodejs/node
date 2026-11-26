//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of type mcontext_t for RISC-V. The definitions in this file are
/// based on the Linux kernel ABI.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_RISCV_MCONTEXT_T_H
#define LLVM_LIBC_TYPES_RISCV_MCONTEXT_T_H

#include "../../__llvm-libc-common.h"

// The general purpose registers. The layout of gregset_t must match the layout
// of 'struct user_regs_struct' in the Linux kernel (see
// arch/riscv/include/uapi/asm/ptrace.h): element 0 holds the program counter
// and element N (N > 0) holds the value of register xN.
#define NGREG 32

#define REG_PC 0
#define REG_RA 1
#define REG_SP 2
#define REG_TP 4
#define REG_S0 8
#define REG_S1 9
#define REG_A0 10
#define REG_S2 18
#define REG_NARGS 8

typedef unsigned long greg_t;
typedef greg_t gregset_t[NGREG];

// The floating point registers. The layout of these types must match the layout
// of 'union __riscv_fp_state' in the Linux kernel (see
// arch/riscv/include/uapi/asm/ptrace.h). The kernel saves the floating point
// state in the format of the D extension.
struct __riscv_mc_f_ext_state {
  unsigned int __f[32];
  unsigned int __fcsr;
};

struct __riscv_mc_d_ext_state {
  unsigned long long __f[32];
  unsigned int __fcsr;
};

struct __riscv_mc_q_ext_state {
  _Alignas(16) unsigned long long __f[64];
  unsigned int __fcsr;
  // Reserved for extending the structure. The kernel uses this space to
  // describe additional (e.g. vector) state, which it stores after the end of
  // the mcontext_t structure.
  unsigned int __reserved[3];
};

typedef union __riscv_mc_fp_state {
  struct __riscv_mc_f_ext_state __f;
  struct __riscv_mc_d_ext_state __d;
  struct __riscv_mc_q_ext_state __q;
} fpregset_t;

// mcontext_t represents the machine state. Its layout must match the layout of
// 'struct sigcontext' in the Linux kernel (see
// arch/riscv/include/uapi/asm/sigcontext.h).
typedef struct mcontext_t {
  gregset_t __gregs;
  fpregset_t __fpregs;
} mcontext_t;

#endif // LLVM_LIBC_TYPES_RISCV_MCONTEXT_T_H
