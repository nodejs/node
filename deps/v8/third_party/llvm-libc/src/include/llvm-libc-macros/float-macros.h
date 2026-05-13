//===-- Definition of macros from float.h ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_MACROS_FLOAT_MACROS_H
#define LLVM_LIBC_MACROS_FLOAT_MACROS_H

#ifndef FLT_RADIX
#define FLT_RADIX __FLT_RADIX__
#endif // FLT_RADIX

#ifndef FLT_EVAL_METHOD
#define FLT_EVAL_METHOD __FLT_EVAL_METHOD__
#endif // FLT_EVAL_METHOD

#ifndef FLT_ROUNDS
#if __has_builtin(__builtin_flt_rounds)
#define FLT_ROUNDS __builtin_flt_rounds()
#else
#define FLT_ROUNDS 1
#endif
#endif // FLT_ROUNDS

#ifndef FLT_DECIMAL_DIG
#define FLT_DECIMAL_DIG __FLT_DECIMAL_DIG__
#endif // FLT_DECIMAL_DIG

#ifndef DBL_DECIMAL_DIG
#define DBL_DECIMAL_DIG __DBL_DECIMAL_DIG__
#endif // DBL_DECIMAL_DIG

#ifndef LDBL_DECIMAL_DIG
#define LDBL_DECIMAL_DIG __LDBL_DECIMAL_DIG__
#endif // LDBL_DECIMAL_DIG

#ifndef DECIMAL_DIG
#define DECIMAL_DIG __DECIMAL_DIG__
#endif // DECIMAL_DIG

#ifndef FLT_DIG
#define FLT_DIG __FLT_DIG__
#endif // FLT_DIG

#ifndef DBL_DIG
#define DBL_DIG __DBL_DIG__
#endif // DBL_DIG

#ifndef LDBL_DIG
#define LDBL_DIG __LDBL_DIG__
#endif // LDBL_DIG

#ifndef FLT_MANT_DIG
#define FLT_MANT_DIG __FLT_MANT_DIG__
#endif // FLT_MANT_DIG

#ifndef DBL_MANT_DIG
#define DBL_MANT_DIG __DBL_MANT_DIG__
#endif // DBL_MANT_DIG

#ifndef LDBL_MANT_DIG
#define LDBL_MANT_DIG __LDBL_MANT_DIG__
#endif // LDBL_MANT_DIG

#ifndef FLT_MIN
#define FLT_MIN __FLT_MIN__
#endif // FLT_MIN

#ifndef DBL_MIN
#define DBL_MIN __DBL_MIN__
#endif // DBL_MIN

#ifndef LDBL_MIN
#define LDBL_MIN __LDBL_MIN__
#endif // LDBL_MIN

#ifndef FLT_MAX
#define FLT_MAX __FLT_MAX__
#endif // FLT_MAX

#ifndef DBL_MAX
#define DBL_MAX __DBL_MAX__
#endif // DBL_MAX

#ifndef LDBL_MAX
#define LDBL_MAX __LDBL_MAX__
#endif // LDBL_MAX

#ifndef FLT_TRUE_MIN
#define FLT_TRUE_MIN __FLT_DENORM_MIN__
#endif // FLT_TRUE_MIN

#ifndef DBL_TRUE_MIN
#define DBL_TRUE_MIN __DBL_DENORM_MIN__
#endif // DBL_TRUE_MIN

#ifndef LDBL_TRUE_MIN
#define LDBL_TRUE_MIN __LDBL_DENORM_MIN__
#endif // LDBL_TRUE_MIN

#ifndef FLT_EPSILON
#define FLT_EPSILON __FLT_EPSILON__
#endif // FLT_EPSILON

#ifndef DBL_EPSILON
#define DBL_EPSILON __DBL_EPSILON__
#endif // DBL_EPSILON

#ifndef LDBL_EPSILON
#define LDBL_EPSILON __LDBL_EPSILON__
#endif // LDBL_EPSILON

#ifndef FLT_MIN_EXP
#define FLT_MIN_EXP __FLT_MIN_EXP__
#endif // FLT_MIN_EXP

#ifndef DBL_MIN_EXP
#define DBL_MIN_EXP __DBL_MIN_EXP__
#endif // DBL_MIN_EXP

#ifndef LDBL_MIN_EXP
#define LDBL_MIN_EXP __LDBL_MIN_EXP__
#endif // LDBL_MIN_EXP

#ifndef FLT_MIN_10_EXP
#define FLT_MIN_10_EXP __FLT_MIN_10_EXP__
#endif // FLT_MIN_10_EXP

#ifndef DBL_MIN_10_EXP
#define DBL_MIN_10_EXP __DBL_MIN_10_EXP__
#endif // DBL_MIN_10_EXP

#ifndef LDBL_MIN_10_EXP
#define LDBL_MIN_10_EXP __LDBL_MIN_10_EXP__
#endif // LDBL_MIN_10_EXP

#ifndef FLT_MAX_EXP
#define FLT_MAX_EXP __FLT_MAX_EXP__
#endif // FLT_MAX_EXP

#ifndef DBL_MAX_EXP
#define DBL_MAX_EXP __DBL_MAX_EXP__
#endif // DBL_MAX_EXP

#ifndef LDBL_MAX_EXP
#define LDBL_MAX_EXP __LDBL_MAX_EXP__
#endif // LDBL_MAX_EXP

#ifndef FLT_MAX_10_EXP
#define FLT_MAX_10_EXP __FLT_MAX_10_EXP__
#endif // FLT_MAX_10_EXP

#ifndef DBL_MAX_10_EXP
#define DBL_MAX_10_EXP __DBL_MAX_10_EXP__
#endif // DBL_MAX_10_EXP

#ifndef LDBL_MAX_10_EXP
#define LDBL_MAX_10_EXP __LDBL_MAX_10_EXP__
#endif // LDBL_MAX_10_EXP

#ifndef FLT_HAS_SUBNORM
#define FLT_HAS_SUBNORM __FLT_HAS_DENORM__
#endif // FLT_HAS_SUBNORM

#ifndef DBL_HAS_SUBNORM
#define DBL_HAS_SUBNORM __DBL_HAS_DENORM__
#endif // DBL_HAS_SUBNORM

#ifndef LDBL_HAS_SUBNORM
#define LDBL_HAS_SUBNORM __LDBL_HAS_DENORM__
#endif // LDBL_HAS_SUBNORM

// TODO: Add FLT16 and FLT128 constants.

#endif // LLVM_LIBC_MACROS_FLOAT_MACROS_H
