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

#include "absl/random/internal/distribution_test_util.h"

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using absl::random_internal::BetaIncomplete;
using absl::random_internal::BetaIncompleteInv;
using absl::random_internal::erfinv;
using absl::random_internal::InverseNormalSurvival;
using absl::random_internal::MaxErrorTolerance;
using absl::random_internal::RequiredSuccessProbability;
using ::testing::AllOf;
using ::testing::DoubleEq;
using ::testing::DoubleNear;
using ::testing::Gt;
using ::testing::IsNan;
using ::testing::Le;
using ::testing::Lt;

TEST(TestUtil, InverseErf) {
  const struct {
    const double z;
    const double value;
  } kErfInvTable[] = {
      {0.0000001, 8.86227e-8},
      {0.00001, 8.86227e-6},
      {0.5, 0.4769362762044},
      {0.6, 0.5951160814499},
      {0.99999, 3.1234132743},
      {0.9999999, 3.7665625816},
      {0.999999944, 3.8403850690566985},  // = log((1-x) * (1+x)) =~ 16.004
      {0.999999999, 4.3200053849134452},
  };

  for (const auto& data : kErfInvTable) {
    auto value = absl::random_internal::erfinv(data.z);

    // Log using the Wolfram-alpha function name & parameters.
    EXPECT_NEAR(value, data.value, 1e-8)
        << " InverseErf[" << data.z << "]  (expected=" << data.value << ")  -> "
        << value;
  }
}

const struct {
  const double p;
  const double q;
  const double x;
  const double alpha;
} kBetaTable[] = {
    {0.5, 0.5, 0.01, 0.06376856085851985},
    {0.5, 0.5, 0.1, 0.2048327646991335},
    {0.5, 0.5, 1, 1},
    {1, 0.5, 0, 0},
    {1, 0.5, 0.01, 0.005012562893380045},
    {1, 0.5, 0.1, 0.0513167019494862},
    {1, 0.5, 0.5, 0.2928932188134525},
    {1, 1, 0.5, 0.5},
    {2, 2, 0.1, 0.028},
    {2, 2, 0.2, 0.104},
    {2, 2, 0.3, 0.216},
    {2, 2, 0.4, 0.352},
    {2, 2, 0.5, 0.5},
    {2, 2, 0.6, 0.648},
    {2, 2, 0.7, 0.784},
    {2, 2, 0.8, 0.896},
    {2, 2, 0.9, 0.972},
    {5.5, 5, 0.5, 0.4361908850559777},
    {10, 0.5, 0.9, 0.1516409096346979},
    {10, 5, 0.5, 0.08978271484375},
    {10, 5, 1, 1},
    {10, 10, 0.5, 0.5},
    {20, 5, 0.8, 0.4598773297575791},
    {20, 10, 0.6, 0.2146816102371739},
    {20, 10, 0.8, 0.9507364826957875},
    {20, 20, 0.5, 0.5},
    {20, 20, 0.6, 0.8979413687105918},
    {30, 10, 0.7, 0.2241297491808366},
    {30, 10, 0.8, 0.7586405487192086},
    {40, 20, 0.7, 0.7001783247477069},
    {1, 0.5, 0.1, 0.0513167019494862},
    {1, 0.5, 0.2, 0.1055728090000841},
    {1, 0.5, 0.3, 0.1633399734659245},
    {1, 0.5, 0.4, 0.2254033307585166},
    {1, 2, 0.2, 0.36},
    {1, 3, 0.2, 0.488},
    {1, 4, 0.2, 0.5904},
    {1, 5, 0.2, 0.67232},
    {2, 2, 0.3, 0.216},
    {3, 2, 0.3, 0.0837},
    {4, 2, 0.3, 0.03078},
    {5, 2, 0.3, 0.010935},

    // These values test small & large points along the range of the Beta
    // function.
    //
    // When selecting test points, remember that if BetaIncomplete(x, p, q)
    // returns the same value to within the limits of precision over a large
    // domain of the input, x, then BetaIncompleteInv(alpha, p, q) may return an
    // essentially arbitrary value where BetaIncomplete(x, p, q) =~ alpha.

    // BetaRegularized[x, 0.00001, 0.00001],
    // For x in {~0.001 ... ~0.999}, => ~0.5
    {1e-5, 1e-5, 1e-5, 0.4999424388184638311},
    {1e-5, 1e-5, (1.0 - 1e-8), 0.5000920948389232964},

    // BetaRegularized[x, 0.00001, 10000].
    // For x in {~epsilon ... 1.0}, => ~1
    {1e-5, 1e5, 1e-6, 0.9999817708130066936},
    {1e-5, 1e5, (1.0 - 1e-7), 1.0},

    // BetaRegularized[x, 10000, 0.00001].
    // For x in {0 .. 1-epsilon}, => ~0
    {1e5, 1e-5, 1e-6, 0},
    {1e5, 1e-5, (1.0 - 1e-6), 1.8229186993306369e-5},
};

TEST(BetaTest, BetaIncomplete) {
  for (const auto& data : kBetaTable) {
    auto value = absl::random_internal::BetaIncomplete(data.x, data.p, data.q);

    // Log using the Wolfram-alpha function name & parameters.
    EXPECT_NEAR(value, data.alpha, 1e-12)
        << " BetaRegularized[" << data.x << ", " << data.p << ", " << data.q
        << "]  (expected=" << data.alpha << ")  -> " << value;
  }
}

TEST(BetaTest, BetaIncompleteInv) {
  for (const auto& data : kBetaTable) {
    auto value =
        absl::random_internal::BetaIncompleteInv(data.p, data.q, data.alpha);

    // Log using the Wolfram-alpha function name & parameters.
    EXPECT_NEAR(value, data.x, 1e-6)
        << " InverseBetaRegularized[" << data.alpha << ", " << data.p << ", "
        << data.q << "]  (expected=" << data.x << ")  -> " << value;
  }
}

TEST(MaxErrorTolerance, MaxErrorTolerance) {
  std::vector<std::pair<double, double>> cases = {
      {0.0000001, 8.86227e-8 * 1.41421356237},
      {0.00001, 8.86227e-6 * 1.41421356237},
      {0.5, 0.4769362762044 * 1.41421356237},
      {0.6, 0.5951160814499 * 1.41421356237},
      {0.99999, 3.1234132743 * 1.41421356237},
      {0.9999999, 3.7665625816 * 1.41421356237},
      {0.999999944, 3.8403850690566985 * 1.41421356237},
      {0.999999999, 4.3200053849134452 * 1.41421356237}};
  for (auto entry : cases) {
    EXPECT_NEAR(absl::random_internal::MaxErrorTolerance(entry.first),
                entry.second, 1e-8);
  }
}

TEST(ZScore, WithSameMean) {
  absl::random_internal::DistributionMoments m;
  m.n = 100;
  m.mean = 5;
  m.variance = 1;
  EXPECT_NEAR(absl::random_internal::ZScore(5, m), 0, 1e-12);

  m.n = 1;
  m.mean = 0;
  m.variance = 1;
  EXPECT_NEAR(absl::random_internal::ZScore(0, m), 0, 1e-12);

  m.n = 10000;
  m.mean = -5;
  m.variance = 100;
  EXPECT_NEAR(absl::random_internal::ZScore(-5, m), 0, 1e-12);
}

TEST(ZScore, DifferentMean) {
  absl::random_internal::DistributionMoments m;
  m.n = 100;
  m.mean = 5;
  m.variance = 1;
  EXPECT_NEAR(absl::random_internal::ZScore(4, m), 10, 1e-12);

  m.n = 1;
  m.mean = 0;
  m.variance = 1;
  EXPECT_NEAR(absl::random_internal::ZScore(-1, m), 1, 1e-12);

  m.n = 10000;
  m.mean = -5;
  m.variance = 100;
  EXPECT_NEAR(absl::random_internal::ZScore(-4, m), -10, 1e-12);
}

MATCHER_P4(InvertsBetaIncompleteNear, p, q, expected_alpha, tol, "") {
  const double actual_alpha = BetaIncomplete(arg, p, q);
  if (std::fabs(actual_alpha - expected_alpha) <= tol) {
    return true;
  }
  *result_listener << "BetaIncomplete(" << arg << ", " << p << ", " << q
                   << ") = " << actual_alpha << " (expected " << expected_alpha
                   << ", diff " << std::fabs(actual_alpha - expected_alpha)
                   << ")";
  return false;
}

MATCHER_P2(HasNormalSurvivalNear, expected_u, rel_tol, "") {
  constexpr double kSqrt1_2 = 0.70710678118654752440084436210485;
  const double actual_u = 0.5 * std::erfc(arg * kSqrt1_2);
  if (std::fabs(actual_u - expected_u) <= rel_tol * expected_u) {
    return true;
  }
  *result_listener << "normal_survival(" << arg << ") = " << actual_u
                   << " (expected " << expected_u << ")";
  return false;
}

TEST(BetaTest, BetaIncompleteInvHighPrecision) {
  // Exact closed-form Beta quantiles should invert to near double precision.
  EXPECT_THAT(BetaIncompleteInv(2.0, 2.0, 0.028), DoubleNear(0.1, 1e-13));
  EXPECT_THAT(BetaIncompleteInv(2.0, 2.0, 0.216), DoubleNear(0.3, 1e-13));
  EXPECT_THAT(BetaIncompleteInv(2.0, 2.0, 0.784), DoubleNear(0.7, 1e-13));
  EXPECT_THAT(BetaIncompleteInv(2.0, 2.0, 0.972), DoubleNear(0.9, 1e-13));
  EXPECT_THAT(BetaIncompleteInv(1.0, 5.0, 0.67232), DoubleNear(0.2, 1e-13));
  EXPECT_THAT(BetaIncompleteInv(5.0, 2.0, 0.010935), DoubleNear(0.3, 1e-13));
  EXPECT_THAT(BetaIncompleteInv(10.0, 5.0, 0.08978271484375),
              DoubleNear(0.5, 1e-13));

  // Skewed and symmetric distributions at interior and extreme-tail quantiles.
  EXPECT_THAT(BetaIncompleteInv(20.0, 5.0, 0.4598773297575791),
              DoubleNear(0.8, 1e-13));
  EXPECT_THAT(BetaIncompleteInv(30.0, 10.0, 0.2241297491808366),
              DoubleNear(0.7, 1e-13));
  EXPECT_THAT(BetaIncompleteInv(40.0, 20.0, 0.7001783247477069),
              DoubleNear(0.7, 1e-13));
  EXPECT_THAT(BetaIncompleteInv(0.5, 0.5, 1e-8),
              AllOf(DoubleNear(2.46740110027234e-16, 1e-28),
                    InvertsBetaIncompleteNear(0.5, 0.5, 1e-8, 1e-20)));
  EXPECT_THAT(BetaIncompleteInv(2.0, 5.0, 1e-12),
              InvertsBetaIncompleteNear(2.0, 5.0, 1e-12, 1e-24));
  EXPECT_THAT(BetaIncompleteInv(2.0, 5.0, 1.0 - 1e-12),
              InvertsBetaIncompleteNear(2.0, 5.0, 1.0 - 1e-12, 1e-13));
  EXPECT_THAT(BetaIncompleteInv(5.0, 2.0, 1e-12),
              InvertsBetaIncompleteNear(5.0, 2.0, 1e-12, 1e-24));
  EXPECT_THAT(BetaIncompleteInv(5.0, 2.0, 1.0 - 1e-12),
              InvertsBetaIncompleteNear(5.0, 2.0, 1.0 - 1e-12, 1e-13));
  EXPECT_THAT(BetaIncompleteInv(100.0, 100.0, 1e-12),
              InvertsBetaIncompleteNear(100.0, 100.0, 1e-12, 1e-24));
  EXPECT_THAT(BetaIncompleteInv(100.0, 100.0, 1.0 - 1e-12),
              InvertsBetaIncompleteNear(100.0, 100.0, 1.0 - 1e-12, 1e-13));
  EXPECT_THAT(BetaIncompleteInv(0.1, 10.0, 1e-10),
              InvertsBetaIncompleteNear(0.1, 10.0, 1e-10, 1e-22));
  EXPECT_THAT(BetaIncompleteInv(10.0, 0.1, 1e-10),
              InvertsBetaIncompleteNear(10.0, 0.1, 1e-10, 1e-22));
  EXPECT_THAT(BetaIncompleteInv(1000.0, 1000.0, 1e-12),
              InvertsBetaIncompleteNear(1000.0, 1000.0, 1e-12, 1e-24));
}

TEST(BetaTest, BetaIncompleteInvExtremeTailAndUnderflowRegressions) {
  const double kMinNormal = std::numeric_limits<double>::min();

  // Quantiles in [DBL_MIN, DBL_MIN / kErr) where tol = kErr * value < DBL_MIN.
  EXPECT_THAT(BetaIncompleteInv(0.001, 1.0, 0.5),
              AllOf(DoubleNear(std::ldexp(1.0, -1000), 1e-312),
                    InvertsBetaIncompleteNear(0.001, 1.0, 0.5, 1e-13)));
  EXPECT_THAT(BetaIncompleteInv(0.05, 1.0, 1e-15),
              AllOf(DoubleNear(1e-300, 1e-311),
                    InvertsBetaIncompleteNear(0.05, 1.0, 1e-15, 1e-28)));
  EXPECT_THAT(BetaIncompleteInv(0.05, 2.0, 1e-15),
              AllOf(Gt(0.0), Lt(1e-299),
                    InvertsBetaIncompleteNear(0.05, 2.0, 1e-15, 1e-28)));
  EXPECT_THAT(BetaIncompleteInv(0.05, 0.05, 1e-15),
              AllOf(Gt(0.0), Lt(1e-293),
                    InvertsBetaIncompleteNear(0.05, 0.05, 1e-15, 1e-28)));

  // Positive initial-estimate underflow branches (both p > 1 && q > 1 and
  // t <= 1) clamp to DBL_MIN and converge without stalling.
  EXPECT_THAT(BetaIncompleteInv(1.1, 100.0, 1e-300),
              AllOf(Gt(0.0), Lt(1e-270),
                    InvertsBetaIncompleteNear(1.1, 100.0, 1e-300, 1e-312)));
  EXPECT_THAT(BetaIncompleteInv(0.05, 2.0, 1e-20),
              AllOf(Gt(0.0), Le(kMinNormal)));
  EXPECT_THAT(BetaIncompleteInv(0.001, 1.0, 0.48),
              AllOf(Gt(0.0), Le(kMinNormal)));

  // Quantiles pressed against 0 / 1 terminate cleanly (including under x87).
  EXPECT_THAT(BetaIncompleteInv(1e-5, 1.0, 0.75), AllOf(Gt(0.0), Lt(1e-13)));
  EXPECT_THAT(BetaIncompleteInv(1.0, 1e-5, 0.25),
              AllOf(Gt(1.0 - 1e-13), Lt(1.0)));
}

TEST(BetaTest, InvalidAndBoundaryInputs) {
  const double kInf = std::numeric_limits<double>::infinity();
  const double kNan = std::numeric_limits<double>::quiet_NaN();

  EXPECT_THAT(BetaIncomplete(kNan, 1.0, 1.0), IsNan());
  EXPECT_THAT(BetaIncomplete(0.5, kNan, 1.0), IsNan());
  EXPECT_THAT(BetaIncomplete(0.5, 1.0, kNan), IsNan());
  EXPECT_THAT(BetaIncomplete(0.0, kNan, 1.0), IsNan());

  EXPECT_THAT(BetaIncomplete(0.5, 0.0, 1.0), DoubleEq(kInf));
  EXPECT_THAT(BetaIncomplete(0.5, 1.0, 0.0), DoubleEq(kInf));
  EXPECT_THAT(BetaIncomplete(0.5, -1.0, 1.0), DoubleEq(kInf));
  EXPECT_THAT(BetaIncomplete(0.5, 1.0, -1.0), DoubleEq(kInf));
  EXPECT_THAT(BetaIncomplete(0.5, kInf, 1.0), DoubleEq(kInf));
  EXPECT_THAT(BetaIncomplete(0.5, 1.0, kInf), DoubleEq(kInf));
  EXPECT_THAT(BetaIncomplete(-0.1, 1.0, 1.0), DoubleEq(kInf));
  EXPECT_THAT(BetaIncomplete(1.1, 1.0, 1.0), DoubleEq(kInf));
  EXPECT_THAT(BetaIncomplete(0.0, 0.0, 1.0), DoubleEq(kInf));

  EXPECT_THAT(BetaIncompleteInv(kNan, 1.0, 0.5), IsNan());
  EXPECT_THAT(BetaIncompleteInv(1.0, kNan, 0.5), IsNan());
  EXPECT_THAT(BetaIncompleteInv(1.0, 1.0, kNan), IsNan());
  EXPECT_THAT(BetaIncompleteInv(kNan, 1.0, 0.0), IsNan());

  EXPECT_THAT(BetaIncompleteInv(0.0, 1.0, 0.25), DoubleEq(kInf));
  EXPECT_THAT(BetaIncompleteInv(1.0, 0.0, 0.25), DoubleEq(kInf));
  EXPECT_THAT(BetaIncompleteInv(-1.0, 1.0, 0.5), DoubleEq(kInf));
  EXPECT_THAT(BetaIncompleteInv(1.0, -1.0, 0.5), DoubleEq(kInf));
  EXPECT_THAT(BetaIncompleteInv(kInf, 1.0, 0.5), DoubleEq(kInf));
  EXPECT_THAT(BetaIncompleteInv(1.0, kInf, 0.5), DoubleEq(kInf));
  EXPECT_THAT(BetaIncompleteInv(1.0, 1.0, -0.1), DoubleEq(kInf));
  EXPECT_THAT(BetaIncompleteInv(1.0, 1.0, 1.1), DoubleEq(kInf));
  EXPECT_THAT(BetaIncompleteInv(0.0, 1.0, 0.0), DoubleEq(kInf));
}

TEST(TestUtil, RequiredSuccessProbabilityAndMaxErrorTolerance) {
  const double kInf = std::numeric_limits<double>::infinity();

  // Small p_fail where naive 1.0 - p_fail loses ~40 bits of precision.
  const double p_1e12_100 = RequiredSuccessProbability(1e-12, 100);
  EXPECT_THAT(p_1e12_100, AllOf(Lt(1.0), DoubleNear(1.0 - 1e-14, 1e-15)));
  EXPECT_THAT(MaxErrorTolerance(p_1e12_100), DoubleNear(7.739358, 1e-4));

  // When p_fail / num_trials <= 2^-54, RequiredSuccessProbability clamps below
  // 1.0 so MaxErrorTolerance remains finite.
  const double p_1e12_100k = RequiredSuccessProbability(1e-12, 100000);
  EXPECT_THAT(p_1e12_100k, Lt(1.0));
  EXPECT_THAT(MaxErrorTolerance(p_1e12_100k),
              AllOf(Gt(8.0), Lt(kInf), DoubleNear(8.292361, 1e-4)));

  const double p_1e15_100 = RequiredSuccessProbability(1e-15, 100);
  EXPECT_THAT(p_1e15_100, Lt(1.0));
  EXPECT_THAT(MaxErrorTolerance(p_1e15_100),
              AllOf(Gt(8.0), Lt(kInf), DoubleNear(8.292361, 1e-4)));

  EXPECT_THAT(RequiredSuccessProbability(0.0, 100), DoubleEq(1.0));
  EXPECT_THAT(MaxErrorTolerance(1.0), DoubleEq(kInf));

  // Very small acceptance probability without 1 - p cancellation near 0.
  EXPECT_THAT(MaxErrorTolerance(1e-12),
              DoubleNear(1.2533141373155002e-12, 1e-20));
}

TEST(TestUtil, InverseNormalSurvivalExtremeTailsAndBoundaries) {
  const double kInf = std::numeric_limits<double>::infinity();
  const double kNan = std::numeric_limits<double>::quiet_NaN();

  EXPECT_THAT(InverseNormalSurvival(0.0), DoubleEq(kInf));
  EXPECT_THAT(InverseNormalSurvival(1.0), DoubleEq(-kInf));
  EXPECT_THAT(InverseNormalSurvival(0.5), DoubleEq(0.0));
  EXPECT_THAT(InverseNormalSurvival(-0.1), IsNan());
  EXPECT_THAT(InverseNormalSurvival(1.1), IsNan());
  EXPECT_THAT(InverseNormalSurvival(kNan), IsNan());

  EXPECT_THAT(InverseNormalSurvival(1e-10),
              HasNormalSurvivalNear(1e-10, 1e-12));
  EXPECT_THAT(InverseNormalSurvival(1e-15),
              HasNormalSurvivalNear(1e-15, 1e-12));
  EXPECT_THAT(InverseNormalSurvival(std::ldexp(1.0, -54)),
              AllOf(DoubleNear(8.292361075813595, 1e-12),
                    HasNormalSurvivalNear(std::ldexp(1.0, -54), 1e-12)));
  EXPECT_THAT(InverseNormalSurvival(1e-20),
              HasNormalSurvivalNear(1e-20, 1e-12));
  EXPECT_THAT(InverseNormalSurvival(1e-50),
              HasNormalSurvivalNear(1e-50, 1e-12));
  EXPECT_THAT(InverseNormalSurvival(1e-100),
              HasNormalSurvivalNear(1e-100, 1e-12));
  EXPECT_THAT(InverseNormalSurvival(1e-300),
              HasNormalSurvivalNear(1e-300, 1e-12));

  // Anti-symmetry across 0.5 using an exact dyadic fraction so 1 - u is exact.
  const double kDyadic = std::ldexp(1.0, -33);
  EXPECT_THAT(InverseNormalSurvival(1.0 - kDyadic),
              DoubleEq(-InverseNormalSurvival(kDyadic)));
}

TEST(TestUtil, InverseErfBoundaries) {
  const double kInf = std::numeric_limits<double>::infinity();
  const double kNan = std::numeric_limits<double>::quiet_NaN();

  EXPECT_THAT(erfinv(1.0), DoubleEq(kInf));
  EXPECT_THAT(erfinv(-1.0), DoubleEq(-kInf));
  EXPECT_THAT(erfinv(0.0), DoubleEq(0.0));
  EXPECT_THAT(erfinv(1.5), IsNan());
  EXPECT_THAT(erfinv(-1.5), IsNan());
  EXPECT_THAT(erfinv(kNan), IsNan());
}

}  // namespace
