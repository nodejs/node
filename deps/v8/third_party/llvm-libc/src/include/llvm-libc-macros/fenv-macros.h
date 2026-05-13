//===-- Definition of macros from fenv.h ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_FENV_MACROS_H
#define LLVM_LIBC_MACROS_FENV_MACROS_H

#define FE_DIVBYZERO 0x1
#define FE_INEXACT 0x2
#define FE_INVALID 0x4
#define FE_OVERFLOW 0x8
#define FE_UNDERFLOW 0x10
#define FE_DENORM 0x20
#define FE_ALL_EXCEPT                                                          \
  (FE_DIVBYZERO | FE_INEXACT | FE_INVALID | FE_OVERFLOW | FE_UNDERFLOW |       \
   FE_DENORM)

#define FE_DOWNWARD 0x400
#define FE_TONEAREST 0
#define FE_TOWARDZERO 0xC00
#define FE_UPWARD 0x800

#define FE_DFL_ENV ((fenv_t *)-1)

#endif // LLVM_LIBC_MACROS_FENV_MACROS_H
