//===-- Definition of wchar_t types ---------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_WCHAR_T_H
#define LLVM_LIBC_TYPES_WCHAR_T_H

// wchar_t is a fundamental type in C++.
#ifndef __cplusplus

typedef __WCHAR_TYPE__ wchar_t;

#endif

#endif // LLVM_LIBC_TYPES_WCHAR_T_H
