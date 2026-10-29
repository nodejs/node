//===-- LLVM_LIBC_IS_DEFINED macro -----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MACROS_IS_DEFINED_H
#define LLVM_LIBC_SRC___SUPPORT_MACROS_IS_DEFINED_H

#include "src/__support/CPP/string_view.h"
#include "src/__support/macros/macro-utils.h"

// LLVM_LIBC_IS_DEFINED checks whether a particular macro is defined.
// Usage: constexpr bool kUseAvx = LLVM_LIBC_IS_DEFINED(__AVX__);
//
// This works by comparing the stringified version of the macro with and
// without evaluation. If FOO is not undefined both stringifications yield
// "FOO". If FOO is defined, one stringification yields "FOO" while the other
// yields its stringified value such as "1".
#define LLVM_LIBC_IS_DEFINED(macro)                                            \
  (LIBC_NAMESPACE::cpp::string_view{LLVM_LIBC_STRINGIFY(macro)} !=             \
   LIBC_NAMESPACE::cpp::string_view{#macro})

#endif // LLVM_LIBC_SRC___SUPPORT_MACROS_IS_DEFINED_H
