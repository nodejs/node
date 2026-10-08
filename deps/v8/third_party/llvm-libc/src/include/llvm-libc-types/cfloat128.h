//===-- Definition of cfloat128 type --------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_TYPES_CFLOAT128_H
#define LLVM_LIBC_TYPES_CFLOAT128_H

#include "../llvm-libc-macros/cfloat128-macros.h"

#ifdef LIBC_TYPES_HAS_CFLOAT128
#ifndef LIBC_TYPES_CFLOAT128_IS_COMPLEX_LONG_DOUBLE
#if defined(__GNUC__) && !defined(__clang__)
// Remove the workaround when https://gcc.gnu.org/PR32187 gets fixed.
typedef __typeof__(_Complex __float128) cfloat128;
#else  // ^^^ workaround / no workaround vvv
typedef _Complex __float128 cfloat128;
#endif // ^^^ no workaround ^^^
#else
typedef _Complex long double cfloat128;
#endif // LIBC_TYPES_CFLOAT128_IS_COMPLEX_LONG_DOUBLE
#endif // LIBC_TYPES_HAS_CFLOAT128

#endif // LLVM_LIBC_TYPES_CFLOAT128_H
