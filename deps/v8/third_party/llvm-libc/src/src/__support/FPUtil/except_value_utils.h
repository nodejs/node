//===-- Common header for helpers to set exceptional values -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_FPUTIL_EXCEPT_VALUE_UTILS_H
#define LLVM_LIBC_SRC___SUPPORT_FPUTIL_EXCEPT_VALUE_UTILS_H

#include "FEnvImpl.h"
#include "FPBits.h"
#include "cast.h"
#include "rounding_mode.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h" // LIBC_UNLIKELY
#include "src/__support/macros/properties/cpu_features.h"
#include "src/__support/macros/properties/types.h"

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

  LIBC_INLINE constexpr cpp::optional<T> lookup(StorageType x_bits) const {
    for (size_t i = 0; i < N; ++i) {
      if (LIBC_UNLIKELY(x_bits == values[i].input)) {
        StorageType out_bits = values[i].rnd_towardzero_result;
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
        return FPBits<T>(out_bits).get_val();
      }
    }
    return cpp::nullopt;
  }

  LIBC_INLINE constexpr cpp::optional<T> lookup_odd(StorageType x_abs,
                                                    bool sign) const {
    for (size_t i = 0; i < N; ++i) {
      if (LIBC_UNLIKELY(x_abs == values[i].input)) {
        StorageType out_bits = values[i].rnd_towardzero_result;
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
template <typename T> LIBC_INLINE T round_result_slightly_down(T value_rn) {
  volatile T tmp = value_rn;
  tmp -= FPBits<T>::min_normal().get_val();
  return tmp;
}

template <typename T> LIBC_INLINE T round_result_slightly_up(T value_rn) {
  volatile T tmp = value_rn;
  tmp += FPBits<T>::min_normal().get_val();
  return tmp;
}

#if defined(LIBC_TYPES_HAS_FLOAT16) &&                                         \
    !defined(LIBC_TARGET_CPU_HAS_FAST_FLOAT16_OPS)
template <> LIBC_INLINE float16 round_result_slightly_down(float16 value_rn) {
  volatile float tmp = value_rn;
  tmp -= FPBits<float16>::min_normal().get_val();
  return cast<float16>(tmp);
}

template <> LIBC_INLINE float16 round_result_slightly_up(float16 value_rn) {
  volatile float tmp = value_rn;
  tmp += FPBits<float16>::min_normal().get_val();
  return cast<float16>(tmp);
}
#endif

} // namespace fputil

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_FPUTIL_EXCEPT_VALUE_UTILS_H
