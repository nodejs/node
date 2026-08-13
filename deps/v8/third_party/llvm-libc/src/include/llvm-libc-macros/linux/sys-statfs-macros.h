//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux specific declarations of macros from sys/statfs.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_SYS_STATFS_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_SYS_STATFS_MACROS_H

// There are numerous possible values for f_type field, defined in
// the Linux kernel header. Include it directly.
#include <linux/magic.h>

#endif // LLVM_LIBC_MACROS_LINUX_SYS_STATFS_MACROS_H
