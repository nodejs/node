//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of loff_t type for Linux.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_LINUX_LOFF_T_H
#define LLVM_LIBC_TYPES_LINUX_LOFF_T_H

#include <asm/posix_types.h>

typedef __kernel_loff_t loff_t;

#endif // LLVM_LIBC_TYPES_LINUX_LOFF_T_H
