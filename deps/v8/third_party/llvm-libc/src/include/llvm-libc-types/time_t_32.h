//===-- Definition of the type time_t -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_TIME_T_32_H
#define LLVM_LIBC_TYPES_TIME_T_32_H

#if defined(__APPLE__)
// Darwin provides its own definition for time_t. Include it directly
// to ensure type compatibility and avoid redefinition errors.
#include <sys/_types/_time_t.h>
#else
typedef __INT32_TYPE__ time_t;
#endif // __APPLE__

#endif // LLVM_LIBC_TYPES_TIME_T_32_H
