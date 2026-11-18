//===-- Definition of socklen_t type ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_SOCKLEN_T_H
#define LLVM_LIBC_TYPES_SOCKLEN_T_H

// POSIX only requires an integer type of at least 32 bits, but socklen_t is
// effectively 32 bits wide in all supported operating systems, and system
// calls read and write it through a pointer to a 32 bit integer.
typedef unsigned int socklen_t;

#endif // LLVM_LIBC_TYPES_SOCKLEN_T_H
