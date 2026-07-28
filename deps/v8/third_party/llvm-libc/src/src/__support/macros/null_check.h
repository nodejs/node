//===-- Safe nullptr check --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MACROS_NULL_CHECK_H
#define LLVM_LIBC_SRC___SUPPORT_MACROS_NULL_CHECK_H

#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

#if defined(LIBC_ADD_NULL_CHECKS)
#define LIBC_CRASH_ON_NULLPTR(ptr)                                             \
  do {                                                                         \
    if (LIBC_UNLIKELY((ptr) == nullptr))                                       \
      __builtin_trap();                                                        \
  } while (0)
#define LIBC_CRASH_ON_VALUE(var, value)                                        \
  do {                                                                         \
    if (LIBC_UNLIKELY((var) == (value)))                                       \
      __builtin_trap();                                                        \
  } while (0)

#else
#define LIBC_CRASH_ON_NULLPTR(ptr)                                             \
  do {                                                                         \
  } while (0)
#define LIBC_CRASH_ON_VALUE(var, value)                                        \
  do {                                                                         \
  } while (0)
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_MACROS_NULL_CHECK_H
