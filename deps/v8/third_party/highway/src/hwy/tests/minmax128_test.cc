// Copyright 2019 Google LLC
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

#include <stddef.h>
#include <stdint.h>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/minmax128_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {

template <class D>
static HWY_NOINLINE Vec<D> Make128(D d, uint64_t hi, uint64_t lo) {
  alignas(16) uint64_t in[2];
  in[0] = lo;
  in[1] = hi;
  return LoadDup128(d, in);
}

struct TestMinMax128 {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using V = Vec<D>;
    const size_t N = Lanes(d);
    auto a_lanes = AllocateAligned<T>(N);
    auto b_lanes = AllocateAligned<T>(N);
    auto min_lanes = AllocateAligned<T>(N);
    auto max_lanes = AllocateAligned<T>(N);
    HWY_ASSERT(a_lanes && b_lanes && min_lanes && max_lanes);
    RandomState rng;

    const V v00 = Zero(d);
    const V v01 = Make128(d, 0, 1);
    const V v10 = Make128(d, 1, 0);
    const V v11 = Add(v01, v10);

    // Same arg
    HWY_ASSERT_VEC_EQ(d, v00, Min128(d, v00, v00));
    HWY_ASSERT_VEC_EQ(d, v01, Min128(d, v01, v01));
    HWY_ASSERT_VEC_EQ(d, v10, Min128(d, v10, v10));
    HWY_ASSERT_VEC_EQ(d, v11, Min128(d, v11, v11));
    HWY_ASSERT_VEC_EQ(d, v00, Max128(d, v00, v00));
    HWY_ASSERT_VEC_EQ(d, v01, Max128(d, v01, v01));
    HWY_ASSERT_VEC_EQ(d, v10, Max128(d, v10, v10));
    HWY_ASSERT_VEC_EQ(d, v11, Max128(d, v11, v11));

    // First arg less
    HWY_ASSERT_VEC_EQ(d, v00, Min128(d, v00, v01));
    HWY_ASSERT_VEC_EQ(d, v01, Min128(d, v01, v10));
    HWY_ASSERT_VEC_EQ(d, v10, Min128(d, v10, v11));
    HWY_ASSERT_VEC_EQ(d, v01, Max128(d, v00, v01));
    HWY_ASSERT_VEC_EQ(d, v10, Max128(d, v01, v10));
    HWY_ASSERT_VEC_EQ(d, v11, Max128(d, v10, v11));

    // Second arg less
    HWY_ASSERT_VEC_EQ(d, v00, Min128(d, v01, v00));
    HWY_ASSERT_VEC_EQ(d, v01, Min128(d, v10, v01));
    HWY_ASSERT_VEC_EQ(d, v10, Min128(d, v11, v10));
    HWY_ASSERT_VEC_EQ(d, v01, Max128(d, v01, v00));
    HWY_ASSERT_VEC_EQ(d, v10, Max128(d, v10, v01));
    HWY_ASSERT_VEC_EQ(d, v11, Max128(d, v11, v10));

    // Also check 128-bit blocks are independent
    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        a_lanes[i] = Random64(&rng);
        b_lanes[i] = Random64(&rng);
      }
      const V a = Load(d, a_lanes.get());
      const V b = Load(d, b_lanes.get());
      for (size_t i = 0; i < N; i += 2) {
        const bool lt = a_lanes[i + 1] == b_lanes[i + 1]
                            ? (a_lanes[i] < b_lanes[i])
                            : (a_lanes[i + 1] < b_lanes[i + 1]);
        min_lanes[i + 0] = lt ? a_lanes[i + 0] : b_lanes[i + 0];
        min_lanes[i + 1] = lt ? a_lanes[i + 1] : b_lanes[i + 1];
        max_lanes[i + 0] = lt ? b_lanes[i + 0] : a_lanes[i + 0];
        max_lanes[i + 1] = lt ? b_lanes[i + 1] : a_lanes[i + 1];
      }
      HWY_ASSERT_VEC_EQ(d, min_lanes.get(), Min128(d, a, b));
      HWY_ASSERT_VEC_EQ(d, max_lanes.get(), Max128(d, a, b));
    }
  }
};

HWY_NOINLINE void TestAllMinMax128() {
  ForGEVectors<128, TestMinMax128>()(uint64_t());
}

struct TestMinMax128Upper {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using V = Vec<D>;
    const size_t N = Lanes(d);
    auto a_lanes = AllocateAligned<T>(N);
    auto b_lanes = AllocateAligned<T>(N);
    auto min_lanes = AllocateAligned<T>(N);
    auto max_lanes = AllocateAligned<T>(N);
    RandomState rng;

    const V v00 = Zero(d);
    const V v01 = Make128(d, 0, 1);
    const V v10 = Make128(d, 1, 0);
    const V v11 = Add(v01, v10);

    // Same arg
    HWY_ASSERT_VEC_EQ(d, v00, Min128Upper(d, v00, v00));
    HWY_ASSERT_VEC_EQ(d, v01, Min128Upper(d, v01, v01));
    HWY_ASSERT_VEC_EQ(d, v10, Min128Upper(d, v10, v10));
    HWY_ASSERT_VEC_EQ(d, v11, Min128Upper(d, v11, v11));
    HWY_ASSERT_VEC_EQ(d, v00, Max128Upper(d, v00, v00));
    HWY_ASSERT_VEC_EQ(d, v01, Max128Upper(d, v01, v01));
    HWY_ASSERT_VEC_EQ(d, v10, Max128Upper(d, v10, v10));
    HWY_ASSERT_VEC_EQ(d, v11, Max128Upper(d, v11, v11));

    // Equivalent but not equal (chooses second arg)
    HWY_ASSERT_VEC_EQ(d, v01, Min128Upper(d, v00, v01));
    HWY_ASSERT_VEC_EQ(d, v11, Min128Upper(d, v10, v11));
    HWY_ASSERT_VEC_EQ(d, v00, Min128Upper(d, v01, v00));
    HWY_ASSERT_VEC_EQ(d, v10, Min128Upper(d, v11, v10));
    HWY_ASSERT_VEC_EQ(d, v00, Max128Upper(d, v01, v00));
    HWY_ASSERT_VEC_EQ(d, v10, Max128Upper(d, v11, v10));
    HWY_ASSERT_VEC_EQ(d, v01, Max128Upper(d, v00, v01));
    HWY_ASSERT_VEC_EQ(d, v11, Max128Upper(d, v10, v11));

    // First arg less
    HWY_ASSERT_VEC_EQ(d, v01, Min128Upper(d, v01, v10));
    HWY_ASSERT_VEC_EQ(d, v10, Max128Upper(d, v01, v10));

    // Second arg less
    HWY_ASSERT_VEC_EQ(d, v01, Min128Upper(d, v10, v01));
    HWY_ASSERT_VEC_EQ(d, v10, Max128Upper(d, v10, v01));

    // Also check 128-bit blocks are independent
    for (size_t rep = 0; rep < AdjustedReps(1000); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        a_lanes[i] = Random64(&rng);
        b_lanes[i] = Random64(&rng);
      }
      const V a = Load(d, a_lanes.get());
      const V b = Load(d, b_lanes.get());
      for (size_t i = 0; i < N; i += 2) {
        const bool lt = a_lanes[i + 1] < b_lanes[i + 1];
        min_lanes[i + 0] = lt ? a_lanes[i + 0] : b_lanes[i + 0];
        min_lanes[i + 1] = lt ? a_lanes[i + 1] : b_lanes[i + 1];
        max_lanes[i + 0] = lt ? b_lanes[i + 0] : a_lanes[i + 0];
        max_lanes[i + 1] = lt ? b_lanes[i + 1] : a_lanes[i + 1];
      }
      HWY_ASSERT_VEC_EQ(d, min_lanes.get(), Min128Upper(d, a, b));
      HWY_ASSERT_VEC_EQ(d, max_lanes.get(), Max128Upper(d, a, b));
    }
  }
};

HWY_NOINLINE void TestAllMinMax128Upper() {
  ForGEVectors<128, TestMinMax128Upper>()(uint64_t());
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_BEFORE_TEST(HwyMinMax128Test);
HWY_EXPORT_AND_TEST_P(HwyMinMax128Test, TestAllMinMax128);
HWY_EXPORT_AND_TEST_P(HwyMinMax128Test, TestAllMinMax128Upper);
HWY_AFTER_TEST();
}  // namespace
}  // namespace hwy
HWY_TEST_MAIN();
#endif  // HWY_ONCE
