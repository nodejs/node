//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Definition of macros from byteswap.h.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_BYTESWAP_MACROS_H
#define LLVM_LIBC_MACROS_BYTESWAP_MACROS_H

#define bswap_16(x) __builtin_bswap16((x))
#define bswap_32(x) __builtin_bswap32((x))
#define bswap_64(x) __builtin_bswap64((x))

#endif // LLVM_LIBC_MACROS_BYTESWAP_MACROS_H
