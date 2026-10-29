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

// Clang aarch64 OOM workaround: Not using AES, hence NEON_WITHOUT_AES is
// sufficient. SVE is mostly superseded by SVE2.
#ifndef HWY_DISABLED_TARGETS
#define HWY_DISABLED_TARGETS (HWY_NEON | HWY_SVE | HWY_SVE_256)
#endif  // HWY_DISABLED_TARGETS

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/masked_compare_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {

struct TestMaskedCompare {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    RandomState rng;

    const Vec<D> v0 = Zero(d);
    const Vec<D> v2 = PositiveIota(d, 2);
    const Vec<D> v2b = PositiveIota(d, 2);
    const Vec<D> v3 = PositiveIota(d, 3);
    const size_t N = Lanes(d);

    const Mask<D> expected_eq = Eq(v2, v3);
    const Mask<D> expected_ne = Ne(v2, v3);
    const Mask<D> expected_lt = Lt(v2, v0);
    const Mask<D> expected_gt = Gt(v2, v0);
    const Mask<D> expected_le = Le(v2, v0);
    const Mask<D> expected_ge = Ge(v2, v0);
    // Use only for same input vector, because PositiveIota saturates.
    const Mask<D> mask_false = MaskFalse(d);
    const Mask<D> mask_true = MaskTrue(d);

    HWY_ASSERT_MASK_EQ(d, expected_eq, MaskedEq(mask_true, v2, v3));
    HWY_ASSERT_MASK_EQ(d, expected_eq, MaskedEq(mask_true, v3, v2));
    HWY_ASSERT_MASK_EQ(d, mask_true, MaskedEq(mask_true, v2, v2));
    HWY_ASSERT_MASK_EQ(d, mask_true, MaskedEq(mask_true, v2, v2b));

    HWY_ASSERT_MASK_EQ(d, expected_ne, MaskedNe(mask_true, v2, v3));
    HWY_ASSERT_MASK_EQ(d, expected_ne, MaskedNe(mask_true, v3, v2));
    HWY_ASSERT_MASK_EQ(d, mask_false, MaskedNe(mask_true, v2, v2));
    HWY_ASSERT_MASK_EQ(d, mask_false, MaskedNe(mask_true, v2, v2b));

    HWY_ASSERT_MASK_EQ(d, expected_lt, MaskedLt(mask_true, v2, v0));
    HWY_ASSERT_MASK_EQ(d, expected_gt, MaskedLt(mask_true, v0, v2));
    HWY_ASSERT_MASK_EQ(d, mask_false, MaskedLt(mask_true, v0, v0));
    HWY_ASSERT_MASK_EQ(d, mask_false, MaskedLt(mask_true, v2, v2));

    HWY_ASSERT_MASK_EQ(d, expected_gt, MaskedGt(mask_true, v2, v0));
    HWY_ASSERT_MASK_EQ(d, expected_lt, MaskedGt(mask_true, v0, v2));
    HWY_ASSERT_MASK_EQ(d, mask_false, MaskedGt(mask_true, v0, v0));
    HWY_ASSERT_MASK_EQ(d, mask_false, MaskedGt(mask_true, v2, v2));

    HWY_ASSERT_MASK_EQ(d, expected_le, MaskedLe(mask_true, v2, v0));
    HWY_ASSERT_MASK_EQ(d, expected_ge, MaskedLe(mask_true, v0, v2));
    HWY_ASSERT_MASK_EQ(d, mask_true, MaskedLe(mask_true, v0, v0));
    HWY_ASSERT_MASK_EQ(d, mask_true, MaskedLe(mask_true, v2, v2));

    HWY_ASSERT_MASK_EQ(d, expected_ge, MaskedGe(mask_true, v2, v0));
    HWY_ASSERT_MASK_EQ(d, expected_le, MaskedGe(mask_true, v0, v2));
    HWY_ASSERT_MASK_EQ(d, mask_true, MaskedGe(mask_true, v0, v0));
    HWY_ASSERT_MASK_EQ(d, mask_true, MaskedGe(mask_true, v2, v2));

    auto bool_lanes = AllocateAligned<T>(N);
    HWY_ASSERT(bool_lanes);

    for (size_t rep = 0; rep < AdjustedReps(200); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        bool_lanes[i] = (Random32(&rng) & 1024) ? T(1) : T(0);
      }

      const Vec<D> mask_i = Load(d, bool_lanes.get());
      const Mask<D> mask = RebindMask(d, Gt(mask_i, Zero(d)));

      HWY_ASSERT_MASK_EQ(d, And(mask, expected_eq), MaskedEq(mask, v2, v3));
      HWY_ASSERT_MASK_EQ(d, And(mask, expected_eq), MaskedEq(mask, v3, v2));
      HWY_ASSERT_MASK_EQ(d, mask, MaskedEq(mask, v2, v2));
      HWY_ASSERT_MASK_EQ(d, mask, MaskedEq(mask, v2, v2b));

      HWY_ASSERT_MASK_EQ(d, And(mask, expected_ne), MaskedNe(mask, v2, v3));
      HWY_ASSERT_MASK_EQ(d, And(mask, expected_ne), MaskedNe(mask, v3, v2));
      HWY_ASSERT_MASK_EQ(d, mask_false, MaskedNe(mask, v2, v2));
      HWY_ASSERT_MASK_EQ(d, mask_false, MaskedNe(mask, v2, v2b));

      HWY_ASSERT_MASK_EQ(d, And(mask, expected_lt), MaskedLt(mask, v2, v0));
      HWY_ASSERT_MASK_EQ(d, And(mask, expected_gt), MaskedLt(mask, v0, v2));
      HWY_ASSERT_MASK_EQ(d, mask_false, MaskedLt(mask, v0, v0));
      HWY_ASSERT_MASK_EQ(d, mask_false, MaskedLt(mask, v2, v2));

      HWY_ASSERT_MASK_EQ(d, And(mask, expected_gt), MaskedGt(mask, v2, v0));
      HWY_ASSERT_MASK_EQ(d, And(mask, expected_lt), MaskedGt(mask, v0, v2));
      HWY_ASSERT_MASK_EQ(d, mask_false, MaskedGt(mask, v0, v0));
      HWY_ASSERT_MASK_EQ(d, mask_false, MaskedGt(mask, v2, v2));

      HWY_ASSERT_MASK_EQ(d, And(mask, expected_le), MaskedLe(mask, v2, v0));
      HWY_ASSERT_MASK_EQ(d, And(mask, expected_ge), MaskedLe(mask, v0, v2));
      HWY_ASSERT_MASK_EQ(d, mask, MaskedLe(mask, v0, v0));
      HWY_ASSERT_MASK_EQ(d, mask, MaskedLe(mask, v2, v2));

      HWY_ASSERT_MASK_EQ(d, And(mask, expected_ge), MaskedGe(mask, v2, v0));
      HWY_ASSERT_MASK_EQ(d, And(mask, expected_le), MaskedGe(mask, v0, v2));
      HWY_ASSERT_MASK_EQ(d, mask, MaskedGe(mask, v0, v0));
      HWY_ASSERT_MASK_EQ(d, mask, MaskedGe(mask, v2, v2));
    }
  }
};

HWY_NOINLINE void TestAllMaskedCompare() {
  ForAllTypes(ForPartialVectors<TestMaskedCompare>());
}

struct TestMaskedFloatClassification {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    RandomState rng;

    const Vec<D> v0 = Zero(d);
    const Vec<D> v1 = Iota(d, 2);
    const Vec<D> v2 = Inf(d);
    const Vec<D> v3 = NaN(d);
    const size_t N = Lanes(d);

    const Mask<D> mask_false = MaskFalse(d);
    const Mask<D> mask_true = MaskTrue(d);

    // Test against all zeros
    HWY_ASSERT_MASK_EQ(d, mask_false, MaskedIsNaN(mask_true, v0));

    // Test against finite values
    HWY_ASSERT_MASK_EQ(d, mask_false, MaskedIsNaN(mask_true, v1));

    // Test against infinite values
    HWY_ASSERT_MASK_EQ(d, mask_false, MaskedIsNaN(mask_true, v2));

    // Test against NaN values
    HWY_ASSERT_MASK_EQ(d, mask_true, MaskedIsNaN(mask_true, v3));

    auto bool_lanes = AllocateAligned<T>(N);
    HWY_ASSERT(bool_lanes);

    for (size_t rep = 0; rep < AdjustedReps(200); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        bool_lanes[i] = (Random32(&rng) & 1024) ? T(1) : T(0);
      }

      const Vec<D> mask_i = Load(d, bool_lanes.get());
      const Mask<D> mask = RebindMask(d, Gt(mask_i, Zero(d)));

      // Test against all zeros
      HWY_ASSERT_MASK_EQ(d, mask_false, MaskedIsNaN(mask, v0));

      // Test against finite values
      HWY_ASSERT_MASK_EQ(d, mask_false, MaskedIsNaN(mask, v1));

      // Test against infinite values
      HWY_ASSERT_MASK_EQ(d, mask_false, MaskedIsNaN(mask, v2));

      // Test against NaN values
      HWY_ASSERT_MASK_EQ(d, mask, MaskedIsNaN(mask, v3));
    }
  }
};

HWY_NOINLINE void TestAllMaskedFloatClassification() {
  ForFloatTypes(ForPartialVectors<TestMaskedFloatClassification>());
}
}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_BEFORE_TEST(HwyMaskedCompareTest);
HWY_EXPORT_AND_TEST_P(HwyMaskedCompareTest, TestAllMaskedCompare);
HWY_EXPORT_AND_TEST_P(HwyMaskedCompareTest, TestAllMaskedFloatClassification);
HWY_AFTER_TEST();
}  // namespace
}  // namespace hwy
HWY_TEST_MAIN();
#endif  // HWY_ONCE
