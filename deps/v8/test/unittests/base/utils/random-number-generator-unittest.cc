// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/utils/random-number-generator.h"

#include <algorithm>
#include <climits>

#include "src/execution/isolate.h"
#include "src/flags/flags.h"
#include "src/utils/utils.h"
#include "test/common/flag-utils.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {
namespace random_number_generator_unittest {

class RandomNumberGeneratorTest : public ::testing::TestWithParam<int> {};

static const int kMaxRuns = 12345;

static void CheckSample(std::vector<uint64_t> sample, uint64_t max,
                        size_t size) {
  EXPECT_EQ(sample.size(), size);

  // Check if values are unique.
  std::sort(sample.begin(), sample.end());
  EXPECT_EQ(std::adjacent_find(sample.begin(), sample.end()), sample.end());

  for (uint64_t x : sample) {
    EXPECT_LT(x, max);
  }
}

static void CheckSlowSample(const std::vector<uint64_t>& sample, uint64_t max,
                            size_t size,
                            const std::unordered_set<uint64_t>& excluded) {
  CheckSample(sample, max, size);

  for (uint64_t i : sample) {
    EXPECT_FALSE(excluded.count(i));
  }
}

static void TestNextSample(RandomNumberGenerator* rng, uint64_t max,
                           size_t size, bool slow = false) {
  std::vector<uint64_t> sample =
      slow ? rng->NextSampleSlow(max, size) : rng->NextSample(max, size);

  CheckSample(sample, max, size);
}

TEST_P(RandomNumberGeneratorTest, NextIntWithMaxValue) {
  RandomNumberGenerator rng(GetParam());
  for (int max = 1; max <= kMaxRuns; ++max) {
    int n = rng.NextInt(max);
    EXPECT_LE(0, n);
    EXPECT_LT(n, max);
  }
}

TEST_P(RandomNumberGeneratorTest, NextBooleanReturnsFalseOrTrue) {
  RandomNumberGenerator rng(GetParam());
  for (int k = 0; k < kMaxRuns; ++k) {
    bool b = rng.NextBool();
    EXPECT_TRUE(b == false || b == true);
  }
}

TEST_P(RandomNumberGeneratorTest, NextDoubleReturnsValueBetween0And1) {
  RandomNumberGenerator rng(GetParam());
  for (int k = 0; k < kMaxRuns; ++k) {
    double d = rng.NextDouble();
    EXPECT_LE(0.0, d);
    EXPECT_LT(d, 1.0);
  }
}

#if !defined(DEBUG) && defined(OFFICIAL_BUILD)
// Official release builds strip all fatal messages for saving binary size,
// see src/base/logging.h.
#define FATAL_MSG(msg) ""
#else
#define FATAL_MSG(msg) "Check failed: " msg
#endif

TEST(RandomNumberGenerator, NextSampleInvalidParam) {
  RandomNumberGenerator rng(123);
  std::vector<uint64_t> sample;
  ASSERT_DEATH_IF_SUPPORTED(sample = rng.NextSample(10, 11),
                            FATAL_MSG("n <= max"));
}

TEST(RandomNumberGenerator, NextSampleSlowInvalidParam1) {
  RandomNumberGenerator rng(123);
  std::vector<uint64_t> sample;
  ASSERT_DEATH_IF_SUPPORTED(sample = rng.NextSampleSlow(10, 11),
                            FATAL_MSG("max - excluded.size"));
}

TEST(RandomNumberGenerator, NextSampleSlowInvalidParam2) {
  RandomNumberGenerator rng(123);
  std::vector<uint64_t> sample;
  ASSERT_DEATH_IF_SUPPORTED(sample = rng.NextSampleSlow(5, 3, {0, 2, 3}),
                            FATAL_MSG("max - excluded.size"));
}

#undef FATAL_MSG

TEST_P(RandomNumberGeneratorTest, NextSample0) {
  size_t m = 1;
  RandomNumberGenerator rng(GetParam());

  TestNextSample(&rng, m, 0);
}

TEST_P(RandomNumberGeneratorTest, NextSampleSlow0) {
  size_t m = 1;
  RandomNumberGenerator rng(GetParam());

  TestNextSample(&rng, m, 0, true);
}

TEST_P(RandomNumberGeneratorTest, NextSample1) {
  size_t m = 10;
  RandomNumberGenerator rng(GetParam());

  for (int k = 0; k < kMaxRuns; ++k) {
    TestNextSample(&rng, m, 1);
  }
}

TEST_P(RandomNumberGeneratorTest, NextSampleSlow1) {
  size_t m = 10;
  RandomNumberGenerator rng(GetParam());

  for (int k = 0; k < kMaxRuns; ++k) {
    TestNextSample(&rng, m, 1, true);
  }
}

TEST_P(RandomNumberGeneratorTest, NextSampleMax) {
  size_t m = 10;
  RandomNumberGenerator rng(GetParam());

  for (int k = 0; k < kMaxRuns; ++k) {
    TestNextSample(&rng, m, m);
  }
}

TEST_P(RandomNumberGeneratorTest, NextSampleSlowMax) {
  size_t m = 10;
  RandomNumberGenerator rng(GetParam());

  for (int k = 0; k < kMaxRuns; ++k) {
    TestNextSample(&rng, m, m, true);
  }
}

TEST_P(RandomNumberGeneratorTest, NextSampleHalf) {
  size_t n = 5;
  uint64_t m = 10;
  RandomNumberGenerator rng(GetParam());

  for (int k = 0; k < kMaxRuns; ++k) {
    TestNextSample(&rng, m, n);
  }
}

TEST_P(RandomNumberGeneratorTest, NextSampleSlowHalf) {
  size_t n = 5;
  uint64_t m = 10;
  RandomNumberGenerator rng(GetParam());

  for (int k = 0; k < kMaxRuns; ++k) {
    TestNextSample(&rng, m, n, true);
  }
}

TEST_P(RandomNumberGeneratorTest, NextSampleMoreThanHalf) {
  size_t n = 90;
  uint64_t m = 100;
  RandomNumberGenerator rng(GetParam());

  for (int k = 0; k < kMaxRuns; ++k) {
    TestNextSample(&rng, m, n);
  }
}

TEST_P(RandomNumberGeneratorTest, NextSampleSlowMoreThanHalf) {
  size_t n = 90;
  uint64_t m = 100;
  RandomNumberGenerator rng(GetParam());

  for (int k = 0; k < kMaxRuns; ++k) {
    TestNextSample(&rng, m, n, true);
  }
}

TEST_P(RandomNumberGeneratorTest, NextSampleLessThanHalf) {
  size_t n = 10;
  uint64_t m = 100;
  RandomNumberGenerator rng(GetParam());

  for (int k = 0; k < kMaxRuns; ++k) {
    TestNextSample(&rng, m, n);
  }
}

TEST_P(RandomNumberGeneratorTest, NextSampleSlowLessThanHalf) {
  size_t n = 10;
  uint64_t m = 100;
  RandomNumberGenerator rng(GetParam());

  for (int k = 0; k < kMaxRuns; ++k) {
    TestNextSample(&rng, m, n, true);
  }
}

TEST_P(RandomNumberGeneratorTest, NextSampleSlowExcluded) {
  size_t n = 2;
  uint64_t m = 10;
  std::unordered_set<uint64_t> excluded = {2, 4, 5, 9};
  RandomNumberGenerator rng(GetParam());

  for (int k = 0; k < kMaxRuns; ++k) {
    std::vector<uint64_t> sample = rng.NextSampleSlow(m, n, excluded);

    CheckSlowSample(sample, m, n, excluded);
  }
}

TEST_P(RandomNumberGeneratorTest, NextSampleSlowExcludedMax1) {
  size_t n = 1;
  uint64_t m = 5;
  std::unordered_set<uint64_t> excluded = {0, 2, 3, 4};
  RandomNumberGenerator rng(GetParam());

  for (int k = 0; k < kMaxRuns; ++k) {
    std::vector<uint64_t> sample = rng.NextSampleSlow(m, n, excluded);

    CheckSlowSample(sample, m, n, excluded);
  }
}

TEST_P(RandomNumberGeneratorTest, NextSampleSlowExcludedMax2) {
  size_t n = 7;
  uint64_t m = 10;
  std::unordered_set<uint64_t> excluded = {0, 4, 8};
  RandomNumberGenerator rng(GetParam());

  for (int k = 0; k < kMaxRuns; ++k) {
    std::vector<uint64_t> sample = rng.NextSampleSlow(m, n, excluded);

    CheckSlowSample(sample, m, n, excluded);
  }
}

INSTANTIATE_TEST_SUITE_P(RandomSeeds, RandomNumberGeneratorTest,
                         ::testing::Values(INT_MIN, -1, 0, 1, 42, 100,
                                           1234567890, 987654321, INT_MAX));

}  // namespace random_number_generator_unittest
}  // namespace base

namespace internal {
namespace random_number_generator_unittest {

using RandomNumberGeneratorIsolateTest = v8::TestWithPlatform;

static const int64_t kRandomSeeds[] = {-1, 1, 42, 100, 1234567890, 987654321};

TEST_F(RandomNumberGeneratorIsolateTest, RandomSeedFlagIsUsed) {
  for (unsigned n = 0; n < arraysize(kRandomSeeds); ++n) {
    FlagScope<int> flag_scope(&v8_flags.random_seed,
                              static_cast<int>(kRandomSeeds[n]));
    v8::IsolateWrapper isolate_wrapper(v8::kNoCounters);
    v8::base::RandomNumberGenerator& rng =
        *isolate_wrapper.i_isolate()->random_number_generator();
    CHECK_EQ(kRandomSeeds[n], rng.initial_seed());
  }
}

// Chi squared for getting m 0s out of n bits.
double ChiSquared(int m, int n) {
  double ys_minus_np1 = (m - n / 2.0);
  double chi_squared_1 = ys_minus_np1 * ys_minus_np1 * 2.0 / n;
  double ys_minus_np2 = ((n - m) - n / 2.0);
  double chi_squared_2 = ys_minus_np2 * ys_minus_np2 * 2.0 / n;
  return chi_squared_1 + chi_squared_2;
}

// Test for correlations between recent bits from the PRNG, or bits that are
// biased.
void RandomBitCorrelation(int random_bit) {
  FlagScope<int> flag_scope(&v8_flags.random_seed, 31415926);
  v8::IsolateWrapper isolate_wrapper(v8::kNoCounters);
  v8::Isolate* isolate = isolate_wrapper.isolate();
  Isolate* i_isolate = isolate_wrapper.i_isolate();
  v8::base::RandomNumberGenerator* rng = i_isolate->random_number_generator();
#ifdef DEBUG
  const int kHistory = 2;
  const int kRepeats = 1000;
#else
  const int kHistory = 8;
  const int kRepeats = 10000;
#endif
  uint32_t history[kHistory];
  // The predictor bit is either constant 0 or 1, or one of the bits from the
  // history.
  for (int predictor_bit = -2; predictor_bit < 32; predictor_bit++) {
    // The predicted bit is one of the bits from the PRNG.
    for (int ago = 0; ago < kHistory; ago++) {
      // We don't want to check whether each bit predicts itself.
      if (ago == 0 && predictor_bit == random_bit) continue;

      // Enter the new random value into the history
      for (int i = ago; i >= 0; i--) {
        history[i] = base::bit_cast<uint32_t>(rng->NextInt());
      }

      // Find out how many of the bits are the same as the prediction bit.
      int m = 0;
      for (int i = 0; i < kRepeats; i++) {
        v8::HandleScope scope(isolate);
        uint32_t random = base::bit_cast<uint32_t>(rng->NextInt());
        for (int j = ago - 1; j >= 0; j--) history[j + 1] = history[j];
        history[0] = random;

        int predicted;
        if (predictor_bit >= 0) {
          predicted = (history[ago] >> predictor_bit) & 1;
        } else {
          predicted = predictor_bit == -2 ? 0 : 1;
        }
        int bit = (random >> random_bit) & 1;
        if (bit == predicted) m++;
      }

      // Chi squared analysis for k = 2 (2, states: same/not-same) and one
      // degree of freedom (k - 1).
      double chi_squared = ChiSquared(m, kRepeats);
      if (chi_squared > 24) {
        int percent = static_cast<int>(m * 100.0 / kRepeats);
        if (predictor_bit < 0) {
          PrintF("Bit %d is %d %d%% of the time\n", random_bit,
                 predictor_bit == -2 ? 0 : 1, percent);
        } else {
          PrintF("Bit %d is the same as bit %d %d ago %d%% of the time\n",
                 random_bit, predictor_bit, ago, percent);
        }
      }

      // For 1 degree of freedom this corresponds to 1 in a million.  We are
      // running ~8000 tests, so that would be surprising.
      CHECK_LE(chi_squared, 24);

      // If the predictor bit is a fixed 0 or 1 then it makes no sense to
      // repeat the test with a different age.
      if (predictor_bit < 0) break;
    }
  }
}

#define TEST_RANDOM_BIT(BIT)                                             \
  TEST_F(RandomNumberGeneratorIsolateTest, RandomBitCorrelations##BIT) { \
    RandomBitCorrelation(BIT);                                           \
  }

TEST_RANDOM_BIT(0)
TEST_RANDOM_BIT(1)
TEST_RANDOM_BIT(2)
TEST_RANDOM_BIT(3)
TEST_RANDOM_BIT(4)
TEST_RANDOM_BIT(5)
TEST_RANDOM_BIT(6)
TEST_RANDOM_BIT(7)
TEST_RANDOM_BIT(8)
TEST_RANDOM_BIT(9)
TEST_RANDOM_BIT(10)
TEST_RANDOM_BIT(11)
TEST_RANDOM_BIT(12)
TEST_RANDOM_BIT(13)
TEST_RANDOM_BIT(14)
TEST_RANDOM_BIT(15)
TEST_RANDOM_BIT(16)
TEST_RANDOM_BIT(17)
TEST_RANDOM_BIT(18)
TEST_RANDOM_BIT(19)
TEST_RANDOM_BIT(20)
TEST_RANDOM_BIT(21)
TEST_RANDOM_BIT(22)
TEST_RANDOM_BIT(23)
TEST_RANDOM_BIT(24)
TEST_RANDOM_BIT(25)
TEST_RANDOM_BIT(26)
TEST_RANDOM_BIT(27)
TEST_RANDOM_BIT(28)
TEST_RANDOM_BIT(29)
TEST_RANDOM_BIT(30)
TEST_RANDOM_BIT(31)

#undef TEST_RANDOM_BIT

}  // namespace random_number_generator_unittest
}  // namespace internal
}  // namespace v8
