//===-- Definition of GPU signal number macros ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_GPU_SIGNAL_MACROS_H
#define LLVM_LIBC_MACROS_GPU_SIGNAL_MACROS_H

#include "__llvm-libc-common.h"

#define SIGINT 2
#define SIGILL 4
#define SIGABRT 6
#define SIGFPE 8
#define SIGSEGV 11
#define SIGTERM 15

#define SIG_ERR __LLVM_LIBC_CAST(reinterpret_cast, void (*)(int), -1)
#define SIG_DFL __LLVM_LIBC_CAST(reinterpret_cast, void (*)(int), 0)
#define SIG_IGN __LLVM_LIBC_CAST(reinterpret_cast, void (*)(int), 1)
#define SIG_HOLD __LLVM_LIBC_CAST(reinterpret_cast, void (*)(int), 2)

// Max signal number
#define NSIG 64

#define __NSIGSET_WORDS NSIG

#endif // LLVM_LIBC_MACROS_GPU_SIGNAL_MACROS_H
