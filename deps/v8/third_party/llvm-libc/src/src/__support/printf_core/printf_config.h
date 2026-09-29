//===-- Printf Configuration Handler ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_PRINTF_CONFIG_H
#define LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_PRINTF_CONFIG_H

// The index array buffer is always initialized when printf is called. In cases
// where index mode is necessary but memory is limited, or when index mode
// performance is important and memory is available, this compile option
// provides a knob to adjust memory usage to an appropriate level. 128 is picked
// as the default size since that's big enough to handle even extreme cases and
// the runtime penalty for not having enough space is severe.
// When an index mode argument is requested, if its index is before the most
// recently read index, then the arg list must be restarted from the beginning,
// and all of the arguments before the new index must be requested with the
// correct types. The index array caches the types of the values in the arg
// list. For every number between the last index cached in the array and the
// requested index, the format string must be parsed again to find the
// type of that index. As an example, if the format string has 20 indexes, and
// the index array is 10, then when the 20th index is requested the first 10
// types can be found immediately, and then the format string must be parsed 10
// times to find the types of the next 10 arguments.
#ifndef LIBC_COPT_PRINTF_INDEX_ARR_LEN
#define LIBC_COPT_PRINTF_INDEX_ARR_LEN 128
#endif

// If fixed point is available and the user hasn't explicitly opted out, then
// enable fixed point.
#if defined(LIBC_COMPILER_HAS_FIXED_POINT) &&                                  \
    !defined(LIBC_COPT_PRINTF_DISABLE_FIXED_POINT)
#define LIBC_INTERNAL_PRINTF_HAS_FIXED_POINT
#endif

#if defined(LIBC_TYPES_HAS_FLOAT128) &&                                        \
    !defined(LIBC_COPT_PRINTF_NO_CONVERT_FLOAT128)
#define LIBC_INTERNAL_PRINTF_CONVERT_FLOAT128
#endif

// TODO(michaelrj): Provide a proper interface for these options.
// LIBC_COPT_FLOAT_TO_STR_USE_MEGA_LONG_DOUBLE_TABLE
// LIBC_COPT_FLOAT_TO_STR_USE_DYADIC_FLOAT
// LIBC_COPT_FLOAT_TO_STR_USE_DYADIC_FLOAT_LD
// LIBC_COPT_FLOAT_TO_STR_USE_INT_CALC
// LIBC_COPT_FLOAT_TO_STR_NO_TABLE
// LIBC_COPT_PRINTF_HEX_LONG_DOUBLE

// TODO(michaelrj): Move the other printf configuration options into this file.

// LIBC_COPT_PRINTF_NO_NULLPTR_CHECKS

#ifdef LIBC_COPT_PRINTF_MODULAR
#define LIBC_PRINTF_MODULE_DECL __attribute__((weak))
#else
#define LIBC_PRINTF_MODULE_DECL LIBC_INLINE
#endif

// LIBC_PRINTF_MODULE: Defines/declares a printf module.
//
// Usage: LIBC_PRINTF_MODULE((<signature>), { <body> })
//
// Note that the signature is parenthesized, but the body is not.

#define LIBC_PRINTF_MODULE_UNWRAP(...) __VA_ARGS__
#if !defined(LIBC_COPT_PRINTF_MODULAR) || defined(LIBC_PRINTF_DEFINE_MODULES)
#define LIBC_PRINTF_MODULE(SIG, ...) LIBC_PRINTF_MODULE_UNWRAP SIG __VA_ARGS__
#else
#define LIBC_PRINTF_MODULE(SIG, ...)                                           \
  LIBC_PRINTF_MODULE_UNWRAP SIG LIBC_PRINTF_MODULE_DECL;
#endif

#endif // LLVM_LIBC_SRC___SUPPORT_PRINTF_CORE_PRINTF_CONFIG_H
