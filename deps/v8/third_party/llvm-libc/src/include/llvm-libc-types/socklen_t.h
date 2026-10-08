//===-- Definition of socklen_t type ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_SOCKLEN_T_H
#define LLVM_LIBC_TYPES_SOCKLEN_T_H

// The posix standard only says of socklen_t that it must be an integer type of
// width of at least 32 bits. The long type is defined as being at least 32
// bits, so an unsigned long should be fine.

typedef unsigned long socklen_t;

#endif // LLVM_LIBC_TYPES_SOCKLEN_T_H
