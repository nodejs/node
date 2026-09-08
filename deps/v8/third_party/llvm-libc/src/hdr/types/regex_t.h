//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Proxy header for regex_t.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_TYPES_REGEX_T_H
#define LLVM_LIBC_HDR_TYPES_REGEX_T_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/regex_t.h"

#else // overlay mode

#error "type not available in overlay mode"

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_REGEX_T_H
