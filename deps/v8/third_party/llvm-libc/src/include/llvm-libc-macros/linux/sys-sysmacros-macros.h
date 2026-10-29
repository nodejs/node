//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Linux specific declarations of macros from sys/sysmacros.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_LINUX_SYS_SYSMACROS_MACROS_H
#define LLVM_LIBC_MACROS_LINUX_SYS_SYSMACROS_MACROS_H

#define major(dev) major(dev)
#define minor(dev) minor(dev)
#define makedev(maj, min) makedev(maj, min)

#endif // LLVM_LIBC_MACROS_LINUX_SYS_SYSMACROS_MACROS_H
