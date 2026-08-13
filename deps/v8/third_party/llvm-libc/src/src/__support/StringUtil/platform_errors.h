//===-- The error table for the current platform ----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_STRINGUTIL_PLATFORM_ERRORS_H
#define LLVM_LIBC_SRC___SUPPORT_STRINGUTIL_PLATFORM_ERRORS_H

#if defined(__linux__) || defined(__Fuchsia__) || defined(__EMSCRIPTEN__)
#include "tables/linux_platform_errors.h"
#else
#include "tables/minimal_platform_errors.h"
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_STRINGUTIL_PLATFORM_ERRORS_H
