//===-- Definition of double_t type ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_DOUBLE_T_H
#define LLVM_LIBC_TYPES_DOUBLE_T_H

#if !defined(__FLT_EVAL_METHOD__) || __FLT_EVAL_METHOD__ == 0
#define __LLVM_LIBC_DOUBLE_T double
#elif __FLT_EVAL_METHOD__ == 1
#define __LLVM_LIBC_DOUBLE_T double
#elif __FLT_EVAL_METHOD__ == 2
#define __LLVM_LIBC_DOUBLE_T long double
#else
#error "Unsupported __FLT_EVAL_METHOD__ value."
#endif

typedef __LLVM_LIBC_DOUBLE_T double_t;

#endif // LLVM_LIBC_TYPES_DOUBLE_T_H
