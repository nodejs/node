//===-- Definition of fexcept_t type --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_FEXCEPT_T_H
#define LLVM_LIBC_TYPES_FEXCEPT_T_H

#if defined(__x86_64__) || defined(__i386__)
typedef unsigned short int fexcept_t;
#else
typedef unsigned int fexcept_t;
#endif

#endif // LLVM_LIBC_TYPES_FEXCEPT_T_H
