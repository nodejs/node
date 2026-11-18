//===-- Common header for helpers to set exceptional values -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_EXCEPT_VALUE_UTILS_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_EXCEPT_VALUE_UTILS_H

#include "hdr/errno_macros.h"
#include "hdr/fenv_macros.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/FPUtil/FEnvImpl.h"
#include "src/__support/FPUtil/FPBits.h"
#include "src/__support/FPUtil/cast.h"
#include "src/__support/FPUtil/rounding_mode.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY
#include "src/__support/macros/properties/cpu_features.h"
#include "src/__support/macros/properties/types.h"
#include "src/__support/sign.h"

namespace LIBC_NAMESPACE_DECL {

namespace fputil {

// This file contains utility functions and classes to manage exceptional values
// when there are many of them.
//
// Example usage:
//
// Define list of exceptional inputs and outputs:
//   static constexpr int N = ...;  // Number of exceptional values.
//   static constexpr fputil::ExceptValues<StorageType, N> Excepts {
//     <list of input bits, output bits and offsets>
//   };
//
// Check for exceptional inputs:
//   if (auto r = Excepts.lookup(x_bits); LIBC_UNLIKELY(r.has_value()))
//     return r.value();

template <typename T, size_t N> struct ExceptValues {
  static_assert(cpp::is_floating_point_v<T>, "Must be a floating point type.");

  using StorageType = typename FPBits<T>::StorageType;

  struct Mapping {
    StorageType input;
    StorageType rnd_towardzero_result;
    StorageType rnd_upward_offset;
    StorageType rnd_downward_offset;
    StorageType rnd_tonearest_offset;
  };

  Mapping values[N];

  LIBC_INLINE LIBC_CONSTEXPR_DEFAULT cpp::optional<T>
  lookup(StorageType x_bits) const {
    for (size_t i = 0; i < N; ++i) {
      if (LIBC_UNLIKELY(x_bits == values[i].input)) {
        StorageType out_bits = values[i].rnd_towardzero_result;
#ifdef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
        out_bits += values[i].rnd_tonearest_offset;
#else
        switch (fputil::quick_get_round()) {
        case FE_UPWARD:
          out_bits += values[i].rnd_upward_offset;
          break;
        case FE_DOWNWARD:
          out_bits += values[i].rnd_downward_offset;
          break;
        case FE_TONEAREST:
          out_bits += values[i].rnd_tonearest_offset;
          break;
        }
#endif // LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
        return FPBits<T>(out_bits).get_val();
      }
    }
    return cpp::nullopt;
  }

  LIBC_INLINE LIBC_CONSTEXPR_DEFAULT cpp::optional<T>
  lookup_odd(StorageType x_abs, bool sign) const {
    for (size_t i = 0; i < N; ++i) {
      if (LIBC_UNLIKELY(x_abs == values[i].input)) {
        StorageType out_bits = values[i].rnd_towardzero_result;
#ifdef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
        out_bits += values[i].rnd_tonearest_offset;
#else
        switch (fputil::quick_get_round()) {
        case FE_UPWARD:
          if (sign)
            out_bits += values[i].rnd_downward_offset;
          else
            out_bits += values[i].rnd_upward_offset;
          break;
        case FE_DOWNWARD:
          // Use conditionals instead of ternary operator to work around gcc's
          // -Wconversion false positive bug:
          // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=101537
          if (sign)
            out_bits += values[i].rnd_upward_offset;
          else
            out_bits += values[i].rnd_downward_offset;
          break;
        case FE_TONEAREST:
          out_bits += values[i].rnd_tonearest_offset;
          break;
        }
#endif // LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
        T result = FPBits<T>(out_bits).get_val();
        if (sign)
          result = -result;

        return result;
      }
    }
    return cpp::nullopt;
  }
};

// Helper functions to set results for exceptional cases.
template <typename T>
LIBC_INLINE LIBC_CONSTEXPR_DEFAULT T round_result_slightly_down(T value_rn) {
#ifdef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
  fputil::raise_except_if_required(FE_INEXACT);
  return value_rn;
#else
  if (cpp::is_constant_evaluated()) {
    fputil::raise_except_if_required(FE_INEXACT);
    return value_rn;
  } else {
    volatile T tmp = value_rn;
    tmp -= FPBits<T>::min_normal().get_val();
    fputil::raise_except_if_required(FE_INEXACT);
    return tmp;
  }
#endif // LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
}

template <typename T>
LIBC_INLINE LIBC_CONSTEXPR_DEFAULT T round_result_slightly_up(T value_rn) {
#ifdef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
  fputil::raise_except_if_required(FE_INEXACT);
  return value_rn;
#else
  if (cpp::is_constant_evaluated()) {
    fputil::raise_except_if_required(FE_INEXACT);
    return value_rn;
  } else {
    volatile T tmp = value_rn;
    tmp += FPBits<T>::min_normal().get_val();
    fputil::raise_except_if_required(FE_INEXACT);
    return tmp;
  }
#endif // LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
}

#if defined(LIBC_TYPES_HAS_FLOAT16) &&                                         \
    !defined(LIBC_TARGET_CPU_HAS_FAST_FLOAT16_OPS)
template <>
LIBC_INLINE LIBC_CONSTEXPR_DEFAULT float16
round_result_slightly_down(float16 value_rn) {
#ifdef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
  fputil::raise_except_if_required(FE_INEXACT);
  return value_rn;
#else
  if (cpp::is_constant_evaluated()) {
    return value_rn;
  } else {
    volatile float tmp = value_rn;
    tmp -= FPBits<float16>::min_normal().get_val();
    fputil::raise_except_if_required(FE_INEXACT);
    return cast<float16>(tmp);
  }
#endif // LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
}

template <>
LIBC_INLINE LIBC_CONSTEXPR_DEFAULT float16
round_result_slightly_up(float16 value_rn) {
#ifdef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
  fputil::raise_except_if_required(FE_INEXACT);
  return value_rn;
#else
  if (cpp::is_constant_evaluated()) {
    return value_rn;
  } else {
    volatile float tmp = value_rn;
    tmp += FPBits<float16>::min_normal().get_val();
    fputil::raise_except_if_required(FE_INEXACT);
    return cast<float16>(tmp);
  }
#endif // LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
}
#endif

// Set errno = ERANGE, raise FE_OVERFLOW | FE_INEXACT if required, and return
// the rounded overflow result (+/-inf or +/-max_normal) for the active
// rounding mode.
template <typename T>
LIBC_INLINE LIBC_CONSTEXPR_DEFAULT T set_overflow(Sign sign = Sign::POS) {
  static_assert(cpp::is_floating_point_v<T>, "Must be a floating point type.");
  if (cpp::is_constant_evaluated())
    return FPBits<T>::inf(sign).get_val();

  fputil::set_errno_if_required(ERANGE);
#if defined(LIBC_MATH_HAS_NO_EXCEPT) &&                                        \
    defined(LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY)
  return FPBits<T>::inf(sign).get_val();
#else
  if constexpr (internal::HAS_HW_FPU<T>) {
    return internal::OVERFLOW_FACTOR<T> *
           internal::OVERFLOW_VAL<T>[sign.is_neg()];
  } else {
    fputil::raise_overflow_except_if_required<T>();
#ifndef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
    int rounding = fputil::quick_get_round();
    if (rounding == FE_TOWARDZERO ||
        (rounding == FE_DOWNWARD && sign.is_pos()) ||
        (rounding == FE_UPWARD && sign.is_neg()))
      return FPBits<T>::max_normal(sign).get_val();
#endif // !LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
    return FPBits<T>::inf(sign).get_val();
  }
#endif
}

// Set errno = ERANGE, raise FE_UNDERFLOW | FE_INEXACT if required, and return
// the rounded underflow result (+/-0.0 or +/-min_subnormal) for the active
// rounding mode.
template <typename T>
LIBC_INLINE LIBC_CONSTEXPR_DEFAULT T set_underflow(Sign sign = Sign::POS) {
  static_assert(cpp::is_floating_point_v<T>, "Must be a floating point type.");
  if (cpp::is_constant_evaluated())
    return FPBits<T>::zero(sign).get_val();

  fputil::set_errno_if_required(ERANGE);
#if defined(LIBC_MATH_HAS_NO_EXCEPT) &&                                        \
    defined(LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY)
  return FPBits<T>::zero(sign).get_val();
#else
  if constexpr (internal::HAS_HW_FPU<T>) {
    return internal::UNDERFLOW_FACTOR<T> *
           internal::UNDERFLOW_VAL<T>[sign.is_neg()];
  } else {
    fputil::raise_underflow_except_if_required<T>();
#ifndef LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
    int rounding = fputil::quick_get_round();
    if ((rounding == FE_UPWARD && sign.is_pos()) ||
        (rounding == FE_DOWNWARD && sign.is_neg()))
      return FPBits<T>::min_subnormal(sign).get_val();
#endif // !LIBC_MATH_HAS_ASSUME_ROUND_NEAREST_ONLY
    return FPBits<T>::zero(sign).get_val();
  }
#endif
}

} // namespace fputil

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_EXCEPT_VALUE_UTILS_H
