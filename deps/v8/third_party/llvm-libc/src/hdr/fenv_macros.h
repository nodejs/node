//===-- Definition of macros from fenv.h ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_FENV_MACROS_H
#define LLVM_LIBC_HDR_FENV_MACROS_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-macros/fenv-macros.h"

#else // Overlay mode

#include <fenv.h>

// In some environment, FE_ALL_EXCEPT is set to 0 and the remaining exceptions
// FE_* are missing.
#ifndef FE_DIVBYZERO
#define FE_DIVBYZERO 0
#endif // FE_DIVBYZERO

#ifndef FE_INEXACT
#define FE_INEXACT 0
#endif // FE_INEXACT

#ifndef FE_INVALID
#define FE_INVALID 0
#endif // FE_INVALID

#ifndef FE_OVERFLOW
#define FE_OVERFLOW 0
#endif // FE_OVERFLOW

#ifndef FE_UNDERFLOW
#define FE_UNDERFLOW 0
#endif // FE_UNDERFLOW

// Rounding mode macros might be missing.
#ifndef FE_DOWNWARD
#define FE_DOWNWARD 0x400
#endif // FE_DOWNWARD

#ifndef FE_TONEAREST
#define FE_TONEAREST 0
#endif // FE_TONEAREST

#ifndef FE_TOWARDZERO
#define FE_TOWARDZERO 0xC00
#endif // FE_TOWARDZERO

#ifndef FE_UPWARD
#define FE_UPWARD 0x800
#endif // FE_UPWARD

#endif // LLVM_LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_FENV_MACROS_H
