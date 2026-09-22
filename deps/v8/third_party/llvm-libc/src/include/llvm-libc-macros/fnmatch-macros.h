//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Macros for POSIX fnmatch.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_FNMATCH_MACROS_H
#define LLVM_LIBC_MACROS_FNMATCH_MACROS_H

#define FNM_NOMATCH 1

#define FNM_PATHNAME (1 << 0)
#define FNM_NOESCAPE (1 << 1)
#define FNM_PERIOD (1 << 2)
#define FNM_CASEFOLD (1 << 4)
#define FNM_IGNORECASE FNM_CASEFOLD

#endif // LLVM_LIBC_MACROS_FNMATCH_MACROS_H
