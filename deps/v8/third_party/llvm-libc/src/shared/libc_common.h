//===-- Common defines for sharing LLVM libc with LLVM projects -*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SHARED_LIBC_COMMON_H
#define LLVM_LIBC_SHARED_LIBC_COMMON_H

// Use system errno by default in non-full builds.
#if !defined(LIBC_ERRNO_MODE) && !defined(LIBC_FULL_BUILD)
#define LIBC_ERRNO_MODE LIBC_ERRNO_MODE_SYSTEM_INLINE
#endif // !defined(LIBC_ERRNO_MODE) && !defined(LIBC_FULL_BUILD)

// Use system fenv functions in math implementations.
#ifndef LIBC_MATH_USE_SYSTEM_FENV
#define LIBC_MATH_USE_SYSTEM_FENV
#endif // LIBC_MATH_USE_SYSTEM_FENV

#ifndef LIBC_NAMESPACE
#define LIBC_NAMESPACE __llvm_libc
#endif // LIBC_NAMESPACE

#endif // LLVM_LIBC_SHARED_LIBC_COMMON_H
