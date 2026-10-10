//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of struct user_regs_struct for x86_64.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_X86_64_STRUCT_USER_REGS_STRUCT_H
#define LLVM_LIBC_TYPES_X86_64_STRUCT_USER_REGS_STRUCT_H

struct user_regs_struct {
  unsigned long long r15;
  unsigned long long r14;
  unsigned long long r13;
  unsigned long long r12;
  unsigned long long rbp;
  unsigned long long rbx;
  unsigned long long r11;
  unsigned long long r10;
  unsigned long long r9;
  unsigned long long r8;
  unsigned long long rax;
  unsigned long long rcx;
  unsigned long long rdx;
  unsigned long long rsi;
  unsigned long long rdi;
  unsigned long long orig_rax;
  unsigned long long rip;
  unsigned long long cs;
  unsigned long long eflags;
  unsigned long long rsp;
  unsigned long long ss;
  unsigned long long fs_base;
  unsigned long long gs_base;
  unsigned long long ds;
  unsigned long long es;
  unsigned long long fs;
  unsigned long long gs;
};

#endif // LLVM_LIBC_TYPES_X86_64_STRUCT_USER_REGS_STRUCT_H
