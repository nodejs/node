//===-- Definition of sighandler_t ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_SIGHANDLER_T_H
#define LLVM_LIBC_TYPES_SIGHANDLER_T_H

#ifdef __linux__
// For compatibility with glibc.
typedef void (*sighandler_t)(int);
#endif

#endif // LLVM_LIBC_TYPES_SIGHANDLER_T_H
