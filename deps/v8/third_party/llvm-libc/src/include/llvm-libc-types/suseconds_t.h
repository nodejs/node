//===-- Definition of suseconds_t type ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_SUSECONDS_T_H
#define LLVM_LIBC_TYPES_SUSECONDS_T_H

// Per posix: suseconds_t shall be a signed integer type capable of storing
// values at least in the range [-1, 1000000]. [...] the widths of [other
// types...] and suseconds_t are no greater than the width of type long.

// The kernel expects 64 bit suseconds_t at least on x86_64.
#if defined(__APPLE__)
// Darwin provides its own definition for suseconds_t. Include it directly
// to ensure type compatibility and avoid redefinition errors.
#include <sys/_types/_suseconds_t.h>
#else
typedef long suseconds_t;
#endif // __APPLE__

#endif // LLVM_LIBC_TYPES_SUSECONDS_T_H
