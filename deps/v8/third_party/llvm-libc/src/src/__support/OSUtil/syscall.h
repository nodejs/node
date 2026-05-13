//===--------------- Internal syscall declarations --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_H
#define LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_H

#ifdef __APPLE__
#include "darwin/syscall.h"
#elif defined(__linux__)
#include "linux/syscall.h"
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_OSUTIL_SYSCALL_H
