// Copyright 2026 Google LLC
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

#include <stdio.h>

#include <vector>

#include "hwy/aligned_allocator.h"
#include "hwy/base.h"

// clang-format off
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "hwy/contrib/algo/minmax_value_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/contrib/algo/minmax-inl.h"
#include "hwy/tests/test_util-inl.h"
// clang-format on

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {

template <typename T>
T Random(RandomState& rng) {
  const int32_t bits = static_cast<int32_t>(Random32(&rng)) & 1023;
  double val = (bits - 512) / 64.0;
  if (!hwy::IsSigned<T>() && val < 0.0) {
    val = -val;
  }
  return ConvertScalarTo<T>(val);
}

template <typename T>
T ScalarMin(const T* in, size_t count) {
  T result = hwy::PositiveInfOrHighestValue<T>();
  for (size_t i = 0; i < count; ++i) {
    if (in[i] < result) {
      result = in[i];
    }
  }
  return result;
}

template <typename T>
T ScalarMax(const T* in, size_t count) {
  T result = hwy::NegativeInfOrLowestValue<T>();
  for (size_t i = 0; i < count; ++i) {
    if (in[i] > result) {
      result = in[i];
    }
  }
  return result;
}

template <class Test>
struct ForeachCountAndMisalign {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) const {
    RandomState rng;
    const size_t N = Lanes(d);
    const size_t misalignments[3] = {0, N / 4, 3 * N / 5};

    std::vector<size_t> counts(AdjustedReps(512));
    for (size_t& count : counts) {
      count = static_cast<size_t>(rng()) % (16 * N + 1);
    }
    counts[0] = 0;

    for (size_t count : counts) {
      for (size_t m : misalignments) {
        Test()(d, count, m, rng);
      }
    }
  }
};

struct TestMinValue {
  template <class D>
  void operator()(D d, size_t count, size_t misalign, RandomState& rng) {
    using T = TFromD<D>;
    AlignedFreeUniquePtr<T[]> storage =
        AllocateAligned<T>(HWY_MAX(1, misalign + count));
    HWY_ASSERT(storage);
    T* in = storage.get() + misalign;
    for (size_t i = 0; i < count; ++i) {
      in[i] = Random<T>(rng);
    }

    const T expected = ScalarMin(in, count);
    const T actual = MinValue(d, in, count);

    if (!IsEqual(expected, actual)) {
      fprintf(stderr, "%s count %d misalign %d: MinValue expected %f got %f\n",
              hwy::TypeName(T(), Lanes(d)).c_str(), static_cast<int>(count),
              static_cast<int>(misalign), ConvertScalarTo<double>(expected),
              ConvertScalarTo<double>(actual));
      HWY_ASSERT(false);
    }
  }
};

struct TestMaxValue {
  template <class D>
  void operator()(D d, size_t count, size_t misalign, RandomState& rng) {
    using T = TFromD<D>;
    AlignedFreeUniquePtr<T[]> storage =
        AllocateAligned<T>(HWY_MAX(1, misalign + count));
    HWY_ASSERT(storage);
    T* in = storage.get() + misalign;
    for (size_t i = 0; i < count; ++i) {
      in[i] = Random<T>(rng);
    }

    const T expected = ScalarMax(in, count);
    const T actual = MaxValue(d, in, count);

    if (!IsEqual(expected, actual)) {
      fprintf(stderr, "%s count %d misalign %d: MaxValue expected %f got %f\n",
              hwy::TypeName(T(), Lanes(d)).c_str(), static_cast<int>(count),
              static_cast<int>(misalign), ConvertScalarTo<double>(expected),
              ConvertScalarTo<double>(actual));
      HWY_ASSERT(false);
    }
  }
};

void TestAllMinValue() {
  ForAllTypes(ForPartialVectors<ForeachCountAndMisalign<TestMinValue>>());
}

void TestAllMaxValue() {
  ForAllTypes(ForPartialVectors<ForeachCountAndMisalign<TestMaxValue>>());
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_BEFORE_TEST(MinMaxTest);
HWY_EXPORT_AND_TEST_P(MinMaxTest, TestAllMinValue);
HWY_EXPORT_AND_TEST_P(MinMaxTest, TestAllMaxValue);
HWY_AFTER_TEST();
}  // namespace
}  // namespace hwy
HWY_TEST_MAIN();
#endif  // HWY_ONCE
