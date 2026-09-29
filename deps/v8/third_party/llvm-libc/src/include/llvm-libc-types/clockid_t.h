//===-- Definition of the type clockid_t ----------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_CLOCKID_T_H
#define LLVM_LIBC_TYPES_CLOCKID_T_H

#if defined(__APPLE__)
// Darwin provides its own defintion for clockid_t . Use that to prevent
// redeclaration errors and correctness.
#include <_time.h>
#else
typedef int clockid_t;
#endif // __APPLE__

#endif // LLVM_LIBC_TYPES_CLOCKID_T_H
