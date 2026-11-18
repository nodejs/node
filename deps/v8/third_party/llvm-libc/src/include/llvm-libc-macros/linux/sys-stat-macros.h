//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux specific declarations of macros from sys/stat.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_SYS_STAT_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_SYS_STAT_MACROS_H

#include <linux/stat.h>

#define S_IREAD S_IRUSR
#define S_IWRITE S_IWUSR
#define S_IEXEC S_IXUSR

#define ACCESSPERMS (S_IRWXU | S_IRWXG | S_IRWXO)

#define UTIME_NOW ((1L << 30) - 1L)
#define UTIME_OMIT ((1L << 30) - 2L)

#endif // LLVM_LIBC_MACROS_LINUX_SYS_STAT_MACROS_H
