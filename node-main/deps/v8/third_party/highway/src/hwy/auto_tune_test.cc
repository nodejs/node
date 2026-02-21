// Copyright 2025 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hwy/auto_tune.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <random>
#include <vector>

#include "hwy/base.h"           // HWY_ASSERT
#include "hwy/nanobenchmark.h"  // Unpredictable1
#include "hwy/tests/hwy_gtest.h"
#include "hwy/tests/test_util-inl.h"

namespace hwy {
namespace {

// Returns random floating-point number in [-8, 8).
static double Random(RandomState& rng) {
  const int32_t bits = static_cast<int32_t>(Random32(&rng)) & 1023;
  return (bits - 512) / 64.0;
}

TEST(AutoTuneTest, TestCostDistribution) {
  // All equal exercises the MAD=0 trimming case.
  const size_t kMaxValues = CostDistribution::kMaxValues;
  for (size_t num : {size_t{3}, kMaxValues - 1, kMaxValues, kMaxValues + 1}) {
    const double kVal = 6.5;
    for (double outlier : {0.0, 1000.0}) {
      CostDistribution cd;
      const size_t num_outliers = HWY_MAX(num / 4, size_t{1});
      for (size_t i = 0; i < num - num_outliers; ++i) cd.Notify(kVal);
      for (size_t i = 0; i < num_outliers; ++i) cd.Notify(outlier);
      const double cost = cd.EstimateCost();
      // Winsorization allows outliers to shift the central tendency a bit.
      HWY_ASSERT(cost >= kVal - 0.25);
      HWY_ASSERT(cost <= kVal + 0.25);
    }
  }

  // Gaussian distribution with additive+multiplicative noise.
  RandomState rng;
  for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
    CostDistribution cd;
    const size_t num = 1000;  // enough for stable variance
    for (size_t i = 0; i < num; ++i) {
      // Central limit theorem: sum of independent random is Gaussian.
      double sum = 500.0;
      for (size_t sum_idx = 0; sum_idx < 100; ++sum_idx) sum += Random(rng);

      // 16% noise: mostly additive, some lucky shots.
      const uint32_t r = Random32(&rng);
      if (r < (1u << 28)) {
        static constexpr double kPowers[4] = {0.0, 1E3, 1E4, 1E5};
        static constexpr double kMul[4] = {0.50, 0.75, 0.85, 0.90};
        if (r & 3) {  // 75% chance of large additive noise
          sum += kPowers[r & 3];
        } else {  // 25% chance of small multiplicative reduction
          sum *= kMul[(r >> 2) & 3];
        }
      }
      cd.Notify(sum);
    }
    const double cost = cd.EstimateCost();
    if (!(490.0 <= cost && cost <= 540.0)) {
      HWY_ABORT("Cost %f outside expected range.", cost);
    }
  }
}

TEST(AutoTuneTest, TestNextEdges) {
  NextWithSkip list(123);
  HWY_ASSERT_EQ(0, list.Next(122));  // Check wrap-around
  HWY_ASSERT_EQ(1, list.Next(0));
  list.Skip(1);
  HWY_ASSERT_EQ(2, list.Next(0));
  list.Skip(2);
  HWY_ASSERT_EQ(3, list.Next(0));

  // Skip last
  list.Skip(122);
  HWY_ASSERT_EQ(0, list.Next(121));

  // Skip first
  list.Skip(0);
  HWY_ASSERT_EQ(3, list.Next(121));
}

TEST(AutoTuneTest, TestNextSkipAllButOne) {
  // Prime, pow2 +/- 1
  for (size_t num : {size_t{37}, size_t{63}, size_t{513}}) {
    NextWithSkip list(num);
    std::vector<uint32_t> pos;
    pos.reserve(num);
    for (size_t i = 0; i < num; ++i) {
      pos.push_back(static_cast<uint32_t>(i));
    }
    std::mt19937 rng(static_cast<uint_fast32_t>(129 * Unpredictable1()));
    std::shuffle(pos.begin(), pos.end(), rng);
    for (size_t i = 0; i < num - 1; ++i) {
      list.Skip(pos[i]);
    }
    HWY_ASSERT_EQ(pos.back(), list.Next(pos.back()));  // only one left
  }
}

}  // namespace
}  // namespace hwy

HWY_TEST_MAIN();
