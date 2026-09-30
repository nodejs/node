//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux specific declarations of macros from sys/file.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_SYS_FILE_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_SYS_FILE_MACROS_H

#define LOCK_SH 1 // Shared lock.
#define LOCK_EX 2 // Exclusive lock.
#define LOCK_NB 4 // Flag to make a nonblocking request.
#define LOCK_UN 8 // Remove an existing lock.

#endif // LLVM_LIBC_MACROS_LINUX_SYS_FILE_MACROS_H
