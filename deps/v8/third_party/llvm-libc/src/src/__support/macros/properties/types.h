//===-- Types support -------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// Types detection and support.

#ifndef LLVM_LIBC_SRC___SUPPORT_MACROS_PROPERTIES_TYPES_H
#define LLVM_LIBC_SRC___SUPPORT_MACROS_PROPERTIES_TYPES_H

#include "hdr/float_macros.h" // LDBL_MANT_DIG
#include "hdr/stdint_proxy.h" // UINT64_MAX, __SIZEOF_INT128__
#include "include/llvm-libc-macros/float16-macros.h" // LIBC_TYPES_HAS_FLOAT16
#include "include/llvm-libc-types/float128.h"        // float128
#include "src/__support/macros/config.h"             // LIBC_NAMESPACE_DECL
#include "src/__support/macros/properties/architectures.h"
#include "src/__support/macros/properties/compiler.h"
#include "src/__support/macros/properties/cpu_features.h"
#include "src/__support/macros/properties/os.h"

// 'long double' properties.
#if (LDBL_MANT_DIG == 53)
#define LIBC_TYPES_LONG_DOUBLE_IS_FLOAT64
#elif (LDBL_MANT_DIG == 64)
#define LIBC_TYPES_LONG_DOUBLE_IS_X86_FLOAT80
#elif (LDBL_MANT_DIG == 113)
#define LIBC_TYPES_LONG_DOUBLE_IS_FLOAT128
#elif (LDBL_MANT_DIG == 106)
#define LIBC_TYPES_LONG_DOUBLE_IS_DOUBLE_DOUBLE
#endif

#if defined(LIBC_TYPES_HAS_FLOAT128) &&                                        \
    !defined(LIBC_TYPES_LONG_DOUBLE_IS_FLOAT128)
#define LIBC_TYPES_FLOAT128_IS_NOT_LONG_DOUBLE
#endif

// int64 / uint64 support
#if defined(UINT64_MAX)
#define LIBC_TYPES_HAS_INT64
#endif // UINT64_MAX

// int128 / uint128 support
#if defined(__SIZEOF_INT128__) && !defined(LIBC_TARGET_OS_IS_WINDOWS)
#define LIBC_TYPES_HAS_INT128
#endif // defined(__SIZEOF_INT128__)

// -- float16 support ---------------------------------------------------------
// LIBC_TYPES_HAS_FLOAT16 is provided by
// "include/llvm-libc-macros/float16-macros.h"
#ifdef LIBC_TYPES_HAS_FLOAT16
// Type alias for internal use.
using float16 = _Float16;
#endif // LIBC_TYPES_HAS_FLOAT16

// -- float128 support --------------------------------------------------------
// LIBC_TYPES_HAS_FLOAT128 and 'float128' type are provided by
// "include/llvm-libc-types/float128.h"

// -- bfloat16 support ---------------------------------------------------------

namespace LIBC_NAMESPACE_DECL {
namespace fputil {
struct BFloat16;
}
} // namespace LIBC_NAMESPACE_DECL

using bfloat16 = LIBC_NAMESPACE::fputil::BFloat16;

#endif // LLVM_LIBC_SRC___SUPPORT_MACROS_PROPERTIES_TYPES_H
