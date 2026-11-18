//===-- Definition of char8_t type ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_CHAR8_T_H
#define LLVM_LIBC_TYPES_CHAR8_T_H

#ifndef __cpp_char8_t
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 10) &&            \
    defined(__cplusplus)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wc++20-compat"
#endif
typedef unsigned char char8_t;
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ >= 10) &&            \
    defined(__cplusplus)
#pragma GCC diagnostic pop
#endif
#endif // !__cpp_char8_t

#endif // LLVM_LIBC_TYPES_CHAR8_T_H
