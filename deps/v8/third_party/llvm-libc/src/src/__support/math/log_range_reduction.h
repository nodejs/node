//===-- Extra range reduction steps for accurate pass of logarithms -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_MATH_LOG_RANGE_REDUCTION_H
#define LLVM_LIBC_SRC___SUPPORT_MATH_LOG_RANGE_REDUCTION_H

#include "src/__support/FPUtil/dyadic_float.h"
#include "src/__support/macros/config.h"
#include "src/__support/math/common_constants.h"
#include "src/__support/uint128.h"

namespace LIBC_NAMESPACE_DECL {
namespace math {
namespace log_range_reduction_internal {

// Struct to store -log*(r) for 4 range reduction steps.
struct LogRR {
  fputil::DyadicFloat<128> step_1[128];
  fputil::DyadicFloat<128> step_2[193];
  fputil::DyadicFloat<128> step_3[161];
  fputil::DyadicFloat<128> step_4[130];
};

// Perform logarithm range reduction steps 2-4.
// Inputs from the first step of range reduction:
//   m_x : the reduced argument after the first step of range reduction
//         satisfying  -2^-8 <= m_x < 2^-7  and  ulp(m_x) >= 2^-60.
//   idx1: index of the -log(r1) table from the first step.
// Outputs of the extra range reduction steps:
//   sum: adding -log(r1) - log(r2) - log(r3) - log(r4) to the resulted sum.
//   return value: the reduced argument v satisfying:
//                 -0x1.0002143p-29 <= v < 0x1p-29,  and  ulp(v) >= 2^(-125).
LIBC_INLINE constexpr fputil::DyadicFloat<128>
log_range_reduction(double m_x, const LogRR &log_table,
                    fputil::DyadicFloat<128> &sum) {
  using namespace common_constants_internal;
  using Float128 = typename fputil::DyadicFloat<128>;
  using MType = typename Float128::MantissaType;

  int64_t v = static_cast<int64_t>(m_x * 0x1.0p60); // ulp = 2^-60

  // Range reduction - Step 2
  // Output range: vv2 in [-0x1.3ffcp-15, 0x1.3e3dp-15].
  // idx2 = trunc(2^14 * (v + 2^-8 + 2^-15))
  size_t idx2 = static_cast<size_t>((v + 0x10'2000'0000'0000) >> 46);
  sum = fputil::quick_add(sum, log_table.step_2[idx2]);

  int64_t s2 = static_cast<int64_t>(S2[idx2]); // |s| <= 2^-7, ulp = 2^-16
  int64_t sv2 = s2 * v;             // |s*v| < 2^-14, ulp = 2^(-60-16) = 2^-76
  int64_t spv2 = (s2 << 44) + v;    // |s + v| < 2^-14, ulp = 2^-60
  int64_t vv2 = (spv2 << 16) + sv2; // |vv2| < 2^-14, ulp = 2^-76

  // Range reduction - Step 3
  // Output range: vv3 in [-0x1.01928p-22 , 0x1p-22]
  // idx3 = trunc(2^21 * (v + 80*2^-21 + 2^-22))
  size_t idx3 = static_cast<size_t>((vv2 + 0x2840'0000'0000'0000) >> 55);
  sum = fputil::quick_add(sum, log_table.step_3[idx3]);

  int64_t s3 = static_cast<int64_t>(S3[idx3]); // |s| < 2^-13, ulp = 2^-21
  int64_t spv3 = (s3 << 55) + vv2;             // |s + v| < 2^-21, ulp = 2^-76
  // |s*v| < 2^-27, ulp = 2^(-76-21) = 2^-97
  Int128 sv3 = static_cast<Int128>(s3) * static_cast<Int128>(vv2);
  // |vv3| < 2^-21, ulp = 2^-97
  Int128 vv3 = (static_cast<Int128>(spv3) << 21) + sv3;

  // Range reduction - Step 4
  // Output range: vv4 in [-0x1.0002143p-29 , 0x1p-29]
  // idx4 = trunc(2^21 * (v + 65*2^-28 + 2^-29))
  size_t idx4 = static_cast<size_t>((static_cast<int>(vv3 >> 68) + 131) >> 1);

  sum = fputil::quick_add(sum, log_table.step_4[idx4]);

  Int128 s4 = static_cast<Int128>(S4[idx4]); // |s| < 2^-21, ulp = 2^-28
  // |s + v| < 2^-28, ulp = 2^-97
  Int128 spv4 = (s4 << 69) + vv3;
  // |s*v| < 2^-42, ulp = 2^(-97-28) = 2^-125
  Int128 sv4 = s4 * vv3;
  // |vv4| < 2^-28, ulp = 2^-125
  Int128 vv4 = (spv4 << 28) + sv4;

  return (vv4 < 0) ? Float128(Sign::NEG, -125,
                              MType({static_cast<uint64_t>(-vv4),
                                     static_cast<uint64_t>((-vv4) >> 64)}))
                   : Float128(Sign::POS, -125,
                              MType({static_cast<uint64_t>(vv4),
                                     static_cast<uint64_t>(vv4 >> 64)}));
}

} // namespace log_range_reduction_internal
} // namespace math
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_MATH_LOG_RANGE_REDUCTION_H
