//===-- Definition of char32_t.h ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_TYPES_CHAR32_T_H
#define LLVM_LIBC_HDR_TYPES_CHAR32_T_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/char32_t.h"

#else // overlay mode

// MacOS doesn't provide uchar.h so we use the types provided by LLVM-libc.
#ifdef __APPLE__
#include "include/llvm-libc-types/char32_t.h"
#else
#include "hdr/uchar_overlay.h"
#endif

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_CHAR32_T_H
