//===-- Definition of char8_t.h -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_HDR_TYPES_CHAR8_T_H
#define LLVM_LIBC_HDR_TYPES_CHAR8_T_H

#ifdef LIBC_FULL_BUILD

#include "include/llvm-libc-types/char8_t.h"

#else // Overlay mode

// MacOS doesn't provide uchar.h so we use the types provided by LLVM-libc.
#ifndef __APPLE__
#include "hdr/uchar_overlay.h"
#endif // !__APPLE__

// Define char8_t in C++ for internal usage if it is not provided by compiler
// or system uchar.h header.
#ifndef __cpp_char8_t
using char8_t = unsigned char;
#endif // !__cpp_char8_t

#endif // LIBC_FULL_BUILD

#endif // LLVM_LIBC_HDR_TYPES_CHAR8_T_H
