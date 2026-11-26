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

#ifndef ABSL_RANDOM_INTERNAL_DISTRIBUTION_TEST_UTIL_H_
#define ABSL_RANDOM_INTERNAL_DISTRIBUTION_TEST_UTIL_H_

#include <cstddef>
#include <ostream>

#include "absl/base/config.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

// NOTE: The functions in this file are test only, and should not be used in
// non-test code.

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace random_internal {

// http://webspace.ship.edu/pgmarr/Geo441/Lectures/Lec%205%20-%20Normality%20Testing.pdf

// Compute the 1st to 4th standard moments:
// mean, variance, skewness, and kurtosis.
// http://www.itl.nist.gov/div898/handbook/eda/section3/eda35b.htm
struct DistributionMoments {
  size_t n = 0;
  double mean = 0.0;
  double variance = 0.0;
  double skewness = 0.0;
  double kurtosis = 0.0;
};
DistributionMoments ComputeDistributionMoments(
    absl::Span<const double> data_points);

std::ostream& operator<<(std::ostream& os, const DistributionMoments& moments);

// Computes the Z-score for a set of data with the given distribution moments
// compared against `expected_mean`.
double ZScore(double expected_mean, const DistributionMoments& moments);

// Returns the probability of success required for a single trial to ensure that
// after `num_trials` trials, the probability of at least one failure is no more
// than `p_fail`.
double RequiredSuccessProbability(double p_fail, int num_trials);

// Computes the maximum tolerable Z-score magnitude (in standard errors) for
// two-sided Z-tests expected to pass with `acceptance_probability`. Asserts in
// debug builds that the resulting tolerance is positive.
//
// For example:
// MaxErrorTolerance(0.001) ~= 0.00125
// MaxErrorTolerance(0.5)   ~= 0.6745
// MaxErrorTolerance(1.0)    = inf
double MaxErrorTolerance(double acceptance_probability);

// Approximation to inverse of the Error Function in double precision.
// (http://people.maths.ox.ac.uk/gilesm/files/gems_erfinv.pdf)
double erfinv(double x);

// Beta(p, q) = Gamma(p) * Gamma(q) / Gamma(p+q)
double beta(double p, double q);

// Inverse of the standard normal survival function: returns z such that
// P(Z > z) = x for x in [0, 1], or NaN outside [0, 1].
double InverseNormalSurvival(double x);

// Returns whether actual is "near" expected, based on the bound.
bool Near(absl::string_view msg, double actual, double expected, double bound);

// Implements the incomplete regularized beta function, AS63, BETAIN.
//    https://www.jstor.org/stable/2346797
//
// BetaIncomplete(x, p, q), where
//   `x` is the value of the upper limit
//   `p` is beta parameter p, `q` is beta parameter q.
//
// NOTE: This is a test-only function which is only accurate to within, at most,
// 1e-13 of the actual value.
//
double BetaIncomplete(double x, double p, double q);

// Implements the inverse of the incomplete regularized beta function, adapted
// from AS109, XINBTA.
//   https://www.jstor.org/stable/2346798
//   https://www.jstor.org/stable/2346887
//
// BetaIncompleteInv(p, q, alpha)
//   `p` is beta parameter p, `q` is beta parameter q.
//   `alpha` is the value of the lower tail area.
//
// NOTE: This is a test-only function using modified Newton-Raphson iteration
// over BetaIncomplete(), targeting ~1e-13 relative accuracy where the CDF is
// not flat to double precision.
double BetaIncompleteInv(double p, double q, double alpha);

}  // namespace random_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_RANDOM_INTERNAL_DISTRIBUTION_TEST_UTIL_H_
