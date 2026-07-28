//===-- Definition of macros for stdckdint.h ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_STDCKDINT_MACROS_H
#define LLVM_LIBC_MACROS_STDCKDINT_MACROS_H

// We need to use __builtin_*_overflow from GCC/Clang to implement the overflow
// macros. Check __GNUC__ or __clang__ for availability of such builtins.
// Note that clang-cl defines __clang__ only and does not define __GNUC__ so we
// have to check for both.
#if defined(__GNUC__) || defined(__clang__)
// clang/gcc overlay may provides similar macros, we need to avoid redefining
// them.
#ifndef __STDC_VERSION_STDCKDINT_H__
#define __STDC_VERSION_STDCKDINT_H__ 202311L

#define ckd_add(R, A, B) __builtin_add_overflow((A), (B), (R))
#define ckd_sub(R, A, B) __builtin_sub_overflow((A), (B), (R))
#define ckd_mul(R, A, B) __builtin_mul_overflow((A), (B), (R))
#endif // __STDC_VERSION_STDCKDINT_H__
#endif // __GNUC__
#endif // LLVM_LIBC_MACROS_STDCKDINT_MACROS_H
