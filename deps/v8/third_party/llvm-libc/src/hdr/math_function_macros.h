//===-- Definition of macros from math.h ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_MATH_FUNCTION_MACROS_H
#define LLVM_LIBC_HDR_MATH_FUNCTION_MACROS_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-macros/math-function-macros.h"

#else // Overlay mode

// GCC will include CXX headers when __cplusplus is defined. This behavior
// can be suppressed by defining _GLIBCXX_INCLUDE_NEXT_C_HEADERS.
#if defined(__GNUC__) && !defined(__clang__)
#define _GLIBCXX_INCLUDE_NEXT_C_HEADERS
#endif
#include <math.h>

#endif // LLVM_LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_MATH_FUNCTION_MACROS_H
