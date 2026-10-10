//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Macros for POSIX glob.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_GLOB_MACROS_H
#define LLVM_LIBC_MACROS_GLOB_MACROS_H

// Flags controlling glob() behavior.
#define GLOB_ERR (1 << 0)
#define GLOB_MARK (1 << 1)
#define GLOB_NOSORT (1 << 2)
#define GLOB_DOOFFS (1 << 3)
#define GLOB_NOCHECK (1 << 4)
#define GLOB_APPEND (1 << 5)
#define GLOB_NOESCAPE (1 << 6)

// glob() error return values.
#define GLOB_NOSPACE 1
#define GLOB_ABORTED 2
#define GLOB_NOMATCH 3

#endif // LLVM_LIBC_MACROS_GLOB_MACROS_H
