//===-- Definition of macros from sys/random.h ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_SYS_RANDOM_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_SYS_RANDOM_MACROS_H

// Getrandom flags
#define GRND_RANDOM 0x0001
#define GRND_NONBLOCK 0x0002
#define GRND_INSECURE 0x0004

#endif // LLVM_LIBC_MACROS_LINUX_SYS_RANDOM_MACROS_H
