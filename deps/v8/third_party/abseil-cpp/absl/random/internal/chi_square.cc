// Copyright 2017 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/random/internal/chi_square.h"

#include <algorithm>
#include <cmath>

#include "absl/base/config.h"
#include "absl/random/internal/distribution_test_util.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace random_internal {
namespace {

constexpr int kLargeDOF = 150;

// 1 / sqrt(2). Defined explicitly instead of non-standard M_SQRT1_2 for
// portability (e.g. Windows/MSVC).
constexpr double kSqrt1_2 = 0.70710678118654752440084436210485;

// Returns the survival function of the standard normal distribution:
// P(Z > z) = 1 - Phi(z) = 0.5 * erfc(z / sqrt(2)).
double normal_survival(double z) { return 0.5 * std::erfc(z * kSqrt1_2); }

}  // namespace

// Calculates the critical chi-square value x given degrees of freedom and
// cumulative probability p = P(X <= x) (upper-tail p-value 1 - p), using
// Wilson-Hilferty for large dof and bisection otherwise. Also known as CRITCHI.
double ChiSquareValue(int dof, double p) {
  static constexpr double kChiEpsilon =
      0.000001;                               // Accuracy of the approximation.
  static constexpr double kChiMax = 99999.0;  // Maximum chi-squared value.

  const double p_value = 1.0 - p;
  if (dof < 1 || p_value >= 1.0) {
    return 0.0;
  }

  if (dof > kLargeDOF) {
    // For large degrees of freedom, use the Wilson-Hilferty (1931) cube-root
    // normal approximation:
    //             (chi^2 / dof)^(1/3) - mean
    //        Z = ----------------------------
    //                       stddev
    const double z = InverseNormalSurvival(p_value);
    const double mean = 1 - 2.0 / (9.0 * dof);
    const double variance = 2.0 / (9.0 * dof);
    // Cannot use this method if the variance is 0.
    if (variance != 0) {
      double term = z * std::sqrt(variance) + mean;
      return dof * (term * term * term);
    }
  }

  if (p_value <= 0.0) return kChiMax;

  // Otherwise search for the p value by bisection
  double min_chisq = 0.0;
  double max_chisq = kChiMax;
  double current = std::min(dof / std::sqrt(p_value), max_chisq);
  while ((max_chisq - min_chisq) > kChiEpsilon) {
    if (ChiSquarePValue(current, dof) < p_value) {
      max_chisq = current;
    } else {
      min_chisq = current;
    }
    current = (max_chisq + min_chisq) * 0.5;
  }
  return current;
}

// Calculates the p-value (probability) of a given chi-square value
// and degrees of freedom.
//
// Adapted from the POCHISQ function from:
//     Hill, I. D. and Pike, M. C.  Algorithm 299
//     Collected Algorithms of the CACM 1963 p. 243
//
double ChiSquarePValue(double chi_square, int dof) {
  static constexpr double kLogSqrtPi =
      0.5723649429247000870717135;  // Log[Sqrt[Pi]]
  static constexpr double kInverseSqrtPi =
      0.5641895835477562869480795;  // 1/(Sqrt[Pi])

  // The chi square function is >= 0 for any degrees of freedom.
  // In other words, probability that the chi square function >= 0 is 1.
  if (chi_square <= 0.0) return 1.0;

  // If the degrees of freedom is zero, the chi square function is always 0 by
  // definition. In other words, the probability that the chi square function
  // is > 0 is zero (chi square values <= 0 have been filtered above).
  if (dof < 1) return 0.0;

  // For large degrees of freedom, use the Wilson-Hilferty (1931) cube-root
  // normal approximation: (chi^2 / dof)^(1/3) ~ N(1 - 2/(9*dof), 2/(9*dof)).
  if (dof > kLargeDOF) {
    // Re-scale everything.
    const double chi_square_scaled = std::pow(chi_square / dof, 1.0 / 3);
    const double mean = 1 - 2.0 / (9.0 * dof);
    const double variance = 2.0 / (9.0 * dof);
    // If variance is 0, this method cannot be used.
    if (variance != 0) {
      const double z = (chi_square_scaled - mean) / std::sqrt(variance);
      return normal_survival(z);
    }
  }

  if (std::isinf(chi_square)) return 0.0;

  // Above kBigX, sum terms in log-space to avoid a^k / k! overflow in the
  // direct recurrence below.
  static constexpr double kBigX = 20;

  double a = 0.5 * chi_square;
  if (a == 0.0) return 1.0;
  const bool even = !(dof & 1);  // True if dof is an even number.
  const double y = std::exp(-a);
  // For odd dof, P(chi^2_1 > chi_square) = erfc(sqrt(chi_square / 2)).
  double s = even ? y : std::erfc(std::sqrt(a));

  if (dof <= 2) {
    return s;
  }

  chi_square = 0.5 * (dof - 1.0);
  double z = (even ? 1.0 : 0.5);
  if (a > kBigX) {
    double e = (even ? 0.0 : kLogSqrtPi);
    double c = std::log(a);
    while (z <= chi_square) {
      e = std::log(z) + e;
      s += std::exp(c * z - a - e);
      z += 1.0;
    }
    return s;
  }

  double e = (even ? 1.0 : (kInverseSqrtPi / std::sqrt(a)));
  double c = 0.0;
  while (z <= chi_square) {
    e = e * (a / z);
    c = c + e;
    z += 1.0;
  }
  return c * y + s;
}

}  // namespace random_internal
ABSL_NAMESPACE_END
}  // namespace absl
