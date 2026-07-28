//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Defines the alloca macro.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_ALLOCA_MACROS_H
#define LLVM_LIBC_MACROS_ALLOCA_MACROS_H

#define alloca(size) __builtin_alloca(size)

#endif // LLVM_LIBC_MACROS_ALLOCA_MACROS_H
