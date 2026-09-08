//===-- Macros used by other macros ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MACROS_MACRO_UTILS_H
#define LLVM_LIBC_SRC___SUPPORT_MACROS_MACRO_UTILS_H

// Stringify the argument after an extra pass of macro expansion.
#define LLVM_LIBC_STRINGIFY(x) LLVM_LIBC_STRINGIFY_IMPL(x)
#define LLVM_LIBC_STRINGIFY_IMPL(x) #x

#endif // LLVM_LIBC_SRC___SUPPORT_MACROS_MACRO_UTILS_H
