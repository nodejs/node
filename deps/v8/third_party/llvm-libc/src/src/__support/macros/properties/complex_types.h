//===-- Complex Types support -----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Complex Types detection and support.

#ifndef LLVM_LIBC_SRC___SUPPORT_MACROS_PROPERTIES_CTYPES_H
#define LLVM_LIBC_SRC___SUPPORT_MACROS_PROPERTIES_CTYPES_H

#include "include/llvm-libc-types/cfloat128.h"
#include "include/llvm-libc-types/cfloat16.h"
#include "types.h"

// -- cfloat16 support --------------------------------------------------------
// LIBC_TYPES_HAS_CFLOAT16 and 'cfloat16' type is provided by
// "include/llvm-libc-types/cfloat16.h"

// -- cfloat128 support -------------------------------------------------------
// LIBC_TYPES_HAS_CFLOAT128 and 'cfloat128' type are provided by
// "include/llvm-libc-types/cfloat128.h"

#if defined(LIBC_TYPES_HAS_CFLOAT128) &&                                       \
    !defined(LIBC_TYPES_CFLOAT128_IS_COMPLEX_LONG_DOUBLE)
#define LIBC_TYPES_CFLOAT128_IS_NOT_COMPLEX_LONG_DOUBLE
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_MACROS_PROPERTIES_CTYPES_H
