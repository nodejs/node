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

#include <algorithm>  // std::count, std::count_if
#include <vector>

#include "hwy/aligned_allocator.h"
#include "hwy/base.h"

// clang-format off
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE \
  "hwy/contrib/algo/count_value_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/contrib/algo/count-inl.h"
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
    counts[0] = 0;  // ensure we test count=0.

    for (size_t count : counts) {
      for (size_t m : misalignments) {
        Test()(d, count, m, rng);
      }
    }
  }
};

struct TestCount {
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

    for (size_t pos = 0; pos < HWY_MIN(count, size_t{3}); ++pos) {
      const T value = in[pos];
      const size_t actual = Count(d, value, in, count);
      const size_t expected =
          static_cast<size_t>(std::count(in, in + count, value));

      if (expected != actual) {
        fprintf(stderr,
                "%s count %d misalign %d: Count(%f) expected %d got %d\n",
                hwy::TypeName(T(), Lanes(d)).c_str(), static_cast<int>(count),
                static_cast<int>(misalign), ConvertScalarTo<double>(value),
                static_cast<int>(expected), static_cast<int>(actual));
        HWY_ASSERT(false);
      }
    }

    HWY_ASSERT_EQ(size_t{0}, Count(d, ConvertScalarTo<T>(9), in, count));
  }
};

void TestAllCount() {
  // Widens to i32, hence require at least 4 i8 or 2 i16. We have an adapter for
  // 128-bit and above which is stricter than required.
  ForAllTypes(ForGE128Vectors<ForeachCountAndMisalign<TestCount>>());
}

struct TestCountIf {
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

    const int min_val = IsSigned<T>() ? -9 : 0;
    for (int val = min_val; val <= 9; ++val) {
      const auto greater = [val](const auto d2, const auto v) HWY_ATTR {
        return Gt(v, Set(d2, ConvertScalarTo<T>(val)));
      };
      const size_t actual = CountIf(d, in, count, greater);

      const size_t expected = static_cast<size_t>(std::count_if(
          in, in + count, [val](T x) { return x > ConvertScalarTo<T>(val); }));

      if (expected != actual) {
        fprintf(stderr,
                "%s count %d misalign %d val %d: CountIf expected %d got %d\n",
                hwy::TypeName(T(), Lanes(d)).c_str(), static_cast<int>(count),
                static_cast<int>(misalign), val, static_cast<int>(expected),
                static_cast<int>(actual));
        HWY_ASSERT(false);
      }
    }
  }
};

void TestAllCountIf() {
  // Widens to i32, hence require at least 4 i8 or 2 i16. We have an adapter for
  // 128-bit and above which is stricter than required.
  ForAllTypes(ForGE128Vectors<ForeachCountAndMisalign<TestCountIf>>());
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_BEFORE_TEST(CountTest);
HWY_EXPORT_AND_TEST_P(CountTest, TestAllCount);
HWY_EXPORT_AND_TEST_P(CountTest, TestAllCountIf);
HWY_AFTER_TEST();
}  // namespace
}  // namespace hwy
HWY_TEST_MAIN();
#endif  // HWY_ONCE
