//===-- Definition of type errno_t ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_INCLUDE_LLVM_LIBC_TYPES_ERRNO_T_H
#define LLVM_LIBC_INCLUDE_LLVM_LIBC_TYPES_ERRNO_T_H

#include "../llvm-libc-macros/annex-k-macros.h"

// LIBC_HAS_ANNEX_K is used to check whether C11 Annex K (the optional
// “Bounds-checking interfaces”) is enabled in this libc implementation. Annex K
// introduces additional types and functions, including `errno_t` (a typedef
// used by the *_s functions). Since `errno_t` is *not defined* in the standard
// library unless Annex K is enabled, we must guard any code that uses it with
// LIBC_HAS_ANNEX_K.
#ifdef LIBC_HAS_ANNEX_K

typedef int errno_t;

#endif // LIBC_HAS_ANNEX_K

#endif // LLVM_LIBC_INCLUDE_LLVM_LIBC_TYPES_ERRNO_T_H
