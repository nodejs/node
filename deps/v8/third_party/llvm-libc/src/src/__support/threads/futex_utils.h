//===--- Platform dispatch for futex utilities ------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_THREADS_FUTEX_UTILS_H
#define LLVM_LIBC_SRC___SUPPORT_THREADS_FUTEX_UTILS_H

#if defined(__linux__)
#include "src/__support/threads/linux/futex_utils.h"
#elif defined(__APPLE__)
#include "src/__support/threads/darwin/futex_utils.h"
#else
#error "futex_utils is not supported on this platform"
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_THREADS_FUTEX_UTILS_H
