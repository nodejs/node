//===-- Definition of type fenv_t -----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_FENV_T_H
#define LLVM_LIBC_TYPES_FENV_T_H

#ifdef __aarch64__
typedef struct {
  unsigned char __control_word[4];
  unsigned char __status_word[4];
} fenv_t;
#elif defined(__x86_64__)
typedef struct {
  unsigned char __x86_status[28];
  unsigned char __mxcsr[4];
} fenv_t;
#elif defined(__arm__) || defined(_M_ARM)
typedef struct {
  unsigned int __fpscr;
} fenv_t;
#elif defined(__riscv)
typedef unsigned int fenv_t;
#elif defined(__AMDGPU__) || defined(__NVPTX__) || defined(__SPIRV__)
typedef struct {
  unsigned int __fpc;
} fenv_t;
#else
#error "fenv_t not defined for your platform"
#endif

#endif // LLVM_LIBC_TYPES_FENV_T_H
