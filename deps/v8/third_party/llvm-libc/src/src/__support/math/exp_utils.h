//===-- Common utils for exp function ---------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_EXP_UTILS_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_EXP_UTILS_H

#include "src/__support/CPP/bit.h"
#include "src/__support/CPP/optional.h"
#include "src/__support/FPUtil/FPBits.h"

namespace LIBC_NAMESPACE_DECL {

// Rounding tests for 2^hi * (mid + lo) when the output might be denormal. We
// assume further that 1 <= mid < 2, mid + lo < 2, and |lo| << mid.
// Notice that, if 0 < x < 2^-1022,
//   double(2^-1022 + x) - 2^-1022 = double(x).
// So if we scale x up by 2^1022, we can use
//   double(1.0 + 2^1022 * x) - 1.0 to test how x is rounded in denormal range.
template <bool SKIP_ZIV_TEST = false>
LIBC_INLINE constexpr cpp::optional<double>
ziv_test_denorm(int hi, double mid, double lo, double err) {
  using FPBits = typename fputil::FPBits<double>;

  // Scaling factor = 1/(min normal number) = 2^1022
  int64_t exp_hi = static_cast<int64_t>(hi + 1022) << FPBits::FRACTION_LEN;
  double mid_hi = cpp::bit_cast<double>(exp_hi + cpp::bit_cast<int64_t>(mid));
  double lo_scaled =
      (lo != 0.0) ? cpp::bit_cast<double>(exp_hi + cpp::bit_cast<int64_t>(lo))
                  : 0.0;

  double extra_factor = 0.0;
  uint64_t scale_down = 0x3FE0'0000'0000'0000; // 1022 in the exponent field.

  // Result is denormal if (mid_hi + lo_scale < 1.0).
  if ((1.0 - mid_hi) > lo_scaled) {
    // Extra rounding step is needed, which adds more rounding errors.
    err += 0x1.0p-52;
    extra_factor = 1.0;
    scale_down = 0x3FF0'0000'0000'0000; // 1023 in the exponent field.
  }

  // By adding 1.0, the results will have similar rounding points as denormal
  // outputs.
  if constexpr (SKIP_ZIV_TEST) {
    double r = extra_factor + (mid_hi + lo_scaled);
    return cpp::bit_cast<double>(cpp::bit_cast<uint64_t>(r) - scale_down);
  } else {
    double err_scaled =
        cpp::bit_cast<double>(exp_hi + cpp::bit_cast<int64_t>(err));

    double lo_u = lo_scaled + err_scaled;
    double lo_l = lo_scaled - err_scaled;

    double upper = extra_factor + (mid_hi + lo_u);
    double lower = extra_factor + (mid_hi + lo_l);

    if (LIBC_LIKELY(upper == lower)) {
      return cpp::bit_cast<double>(cpp::bit_cast<uint64_t>(upper) - scale_down);
    }

    return cpp::nullopt;
  }
}

} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_EXP_UTILS_H
