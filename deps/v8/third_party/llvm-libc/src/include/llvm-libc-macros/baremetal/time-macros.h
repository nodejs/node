//===-- Definition of macros from time.h ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_BAREMETAL_TIME_MACROS_H
#define LLVM_LIBC_MACROS_BAREMETAL_TIME_MACROS_H

#ifdef __CLK_TCK
#define CLOCKS_PER_SEC __CLK_TCK
#else
#if defined(__arm__) || defined(_M_ARM) || defined(__aarch64__) ||             \
    defined(__arm64__) || defined(_M_ARM64)
// This default implementation of this function shall use semihosting
// Semihosting measures time in centiseconds
// https://github.com/ARM-software/abi-aa/blob/main/semihosting/semihosting.rst#sys-clock-0x10
#define CLOCKS_PER_SEC 100
#else
#define CLOCKS_PER_SEC 1000000
#endif
#endif

#endif // LLVM_LIBC_MACROS_BAREMETAL_TIME_MACROS_H
