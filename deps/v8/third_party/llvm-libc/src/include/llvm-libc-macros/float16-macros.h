//===-- Detection of _Float16 compiler builtin type -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_FLOAT16_MACROS_H
#define LLVM_LIBC_MACROS_FLOAT16_MACROS_H

#include "../llvm-libc-types/float128.h"

#if defined(__FLT16_MANT_DIG__) &&                                             \
    (!defined(__GNUC__) || __GNUC__ >= 13 ||                                   \
     (defined(__clang__) && __clang_major__ >= 12)) &&                         \
    !defined(__arm__) && !defined(_M_ARM) && !defined(__riscv) &&              \
    !defined(_WIN32)
#define LIBC_TYPES_HAS_FLOAT16

// TODO: This would no longer be required if HdrGen let us guard function
// declarations with multiple macros.
#ifdef LIBC_TYPES_HAS_FLOAT128
#define LIBC_TYPES_HAS_FLOAT16_AND_FLOAT128
#endif // LIBC_TYPES_HAS_FLOAT128
#endif

#endif // LLVM_LIBC_MACROS_FLOAT16_MACROS_H
