//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Proxy for langinfo_macros.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_LANGINFO_MACROS_H
#define LLVM_LIBC_HDR_LANGINFO_MACROS_H

#ifdef LIBC_FULL_BUILD
#include "include/llvm-libc-macros/langinfo-macros.h"
#else
#include <langinfo.h>
#endif

#endif // LLVM_LIBC_HDR_LANGINFO_MACROS_H
