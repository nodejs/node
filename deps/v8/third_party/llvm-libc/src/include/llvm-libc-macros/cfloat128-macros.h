//===-- Detection of _Complex _Float128 compiler builtin type -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_CFLOAT128_MACROS_H
#define LLVM_LIBC_MACROS_CFLOAT128_MACROS_H

#include "float-macros.h" // LDBL_MANT_DIG

// Currently, the complex variant of C23 `_Float128` type is only defined as a
// built-in type in GCC 7 or later, for C and in GCC 13 or later, for C++. For
// clang, the complex variant of `__float128` is defined instead, and only on
// x86-64 targets for clang 11 or later.
//
// TODO: Update the complex variant of C23 `_Float128` type detection again when
// clang supports it.
#ifdef __clang__
#if (__clang_major__ >= 11) &&                                                 \
    (defined(__FLOAT128__) || defined(__SIZEOF_FLOAT128__))
// Use _Complex __float128 type. clang uses __SIZEOF_FLOAT128__ or __FLOAT128__
// macro to notify the availability of __float128 type:
// https://reviews.llvm.org/D15120
#define LIBC_TYPES_HAS_CFLOAT128
#endif
#elif defined(__GNUC__)
#if (defined(__STDC_IEC_60559_COMPLEX__) && defined(__SIZEOF_FLOAT128__)) &&   \
    (__GNUC__ >= 13 || (!defined(__cplusplus)))
#define LIBC_TYPES_HAS_CFLOAT128
#endif
#endif

#if !defined(LIBC_TYPES_HAS_CFLOAT128) && (LDBL_MANT_DIG == 113)
#define LIBC_TYPES_HAS_CFLOAT128
#define LIBC_TYPES_CFLOAT128_IS_COMPLEX_LONG_DOUBLE
#endif

#endif // LLVM_LIBC_MACROS_CFLOAT128_MACROS_H
