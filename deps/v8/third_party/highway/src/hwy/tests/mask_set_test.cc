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
#define HWY_TARGET_INCLUDE "tests/mask_set_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {

struct TestMaskFalse {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_HAVE_SCALABLE || HWY_TARGET_IS_SVE || HWY_TARGET == HWY_SCALAR
    // For RVV, SVE and SCALAR, use the underlying native vector.
    const DFromV<Vec<D>> d2;
#else
    // Other targets are strongly-typed, but we can safely ResizeBitCast to the
    // native vector. All targets have at least 128-bit vectors, but NEON also
    // supports 64-bit vectors.
    constexpr size_t kMinD2Lanes = (HWY_TARGET_IS_NEON ? 8 : 16) / sizeof(T);
    const FixedTag<T, HWY_MAX(HWY_MAX_LANES_D(D), kMinD2Lanes)> d2;
#endif
    static_assert(d2.MaxBytes() >= d.MaxBytes(),
                  "d2.MaxBytes() >= d.MaxBytes() should be true");
    using V2 = Vec<decltype(d2)>;

    // Various ways of checking that false masks are false.
    HWY_ASSERT(AllFalse(d, MaskFalse(d)));
    HWY_ASSERT_EQ(0, CountTrue(d, MaskFalse(d)));
    HWY_ASSERT_VEC_EQ(d, Zero(d), VecFromMask(d, MaskFalse(d)));

#if HWY_HAVE_SCALABLE || HWY_TARGET_IS_SVE
    // For these targets, we can treat the result as if it were a vector of type
    // `V2`. On SVE, vectors are always full (not fractional) and caps are only
    // enforced by Highway ops. On RVV, LMUL must match but caps can also be
    // ignored. For safety, MaskFalse also sets lanes >= `Lanes(d)` to false,
    // and we verify that here.
    HWY_ASSERT(AllFalse(d2, MaskFalse(d)));
    HWY_ASSERT_EQ(0, CountTrue(d2, MaskFalse(d)));
    HWY_ASSERT_VEC_EQ(d2, Zero(d2), VecFromMask(d2, MaskFalse(d)));
#endif

    // All targets support, and strongly-typed (non-scalable) targets require,
    // ResizeBitCast before we compare to the 'native' underlying vector size.
    const V2 actual2 = ResizeBitCast(d2, VecFromMask(d, MaskFalse(d)));
    HWY_ASSERT_VEC_EQ(d2, Zero(d2), actual2);
  }
};

HWY_NOINLINE void TestAllMaskFalse() {
  ForAllTypes(ForPartialVectors<TestMaskFalse>());
}

struct TestFirstN {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto bool_lanes = AllocateAligned<T>(N);
    HWY_ASSERT(bool_lanes);

    using TN = SignedFromSize<HWY_MIN(sizeof(size_t), sizeof(T))>;
    const size_t max_len = static_cast<size_t>(LimitsMax<TN>());

    const Vec<D> k1 = Set(d, ConvertScalarTo<T>(1));

    const size_t max_lanes = HWY_MIN(2 * N, AdjustedReps(512));
    for (size_t len = 0; len <= HWY_MIN(max_lanes, max_len); ++len) {
      // Loop instead of Iota+Lt to avoid wraparound for 8-bit T.
      for (size_t i = 0; i < N; ++i) {
        bool_lanes[i] = ConvertScalarTo<T>(i < len ? 1 : 0);
      }
      const Mask<D> expected = Eq(Load(d, bool_lanes.get()), k1);
      HWY_ASSERT_MASK_EQ(d, expected, FirstN(d, len));
    }

    // Also ensure huge values yield all-true (unless the vector is actually
    // larger than max_len).
    for (size_t i = 0; i < N; ++i) {
      bool_lanes[i] = ConvertScalarTo<T>(i < max_len ? 1 : 0);
    }
    const Mask<D> expected = Eq(Load(d, bool_lanes.get()), k1);
    HWY_ASSERT_MASK_EQ(d, expected, FirstN(d, max_len));
  }
};

HWY_NOINLINE void TestAllFirstN() {
  ForAllTypes(ForPartialVectors<TestFirstN>());
}

struct TestSetBeforeFirst {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(di);
    auto bool_lanes = AllocateAligned<TI>(N);
    HWY_ASSERT(bool_lanes);
    memset(bool_lanes.get(), 0, N * sizeof(TI));

    // For all combinations of zero/nonzero state of subset of lanes:
    const size_t max_lanes = AdjustedLog2Reps(HWY_MIN(N, size_t(6)));
    for (size_t code = 0; code < (1ull << max_lanes); ++code) {
      for (size_t i = 0; i < max_lanes; ++i) {
        bool_lanes[i] = (code & (1ull << i)) ? TI(1) : TI(0);
      }

      const auto m = RebindMask(d, Gt(Load(di, bool_lanes.get()), Zero(di)));

      const size_t first_set_lane_idx =
          (code != 0)
              ? Num0BitsBelowLS1Bit_Nonzero64(static_cast<uint64_t>(code))
              : N;
      const auto expected_mask = FirstN(d, first_set_lane_idx);

      HWY_ASSERT_MASK_EQ(d, expected_mask, SetBeforeFirst(m));
    }
  }
};

HWY_NOINLINE void TestAllSetBeforeFirst() {
  ForAllTypes(ForPartialVectors<TestSetBeforeFirst>());
}

struct TestSetAtOrBeforeFirst {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(di);
    auto bool_lanes = AllocateAligned<TI>(N);
    HWY_ASSERT(bool_lanes);
    memset(bool_lanes.get(), 0, N * sizeof(TI));

    // For all combinations of zero/nonzero state of subset of lanes:
    const size_t max_lanes = AdjustedLog2Reps(HWY_MIN(N, size_t(6)));
    for (size_t code = 0; code < (1ull << max_lanes); ++code) {
      for (size_t i = 0; i < max_lanes; ++i) {
        bool_lanes[i] = (code & (1ull << i)) ? TI(1) : TI(0);
      }

      const auto m = RebindMask(d, Gt(Load(di, bool_lanes.get()), Zero(di)));

      const size_t idx_after_first_set_lane =
          (code != 0)
              ? (Num0BitsBelowLS1Bit_Nonzero64(static_cast<uint64_t>(code)) + 1)
              : N;
      const auto expected_mask = FirstN(d, idx_after_first_set_lane);

      HWY_ASSERT_MASK_EQ(d, expected_mask, SetAtOrBeforeFirst(m));
    }
  }
};

HWY_NOINLINE void TestAllSetAtOrBeforeFirst() {
  ForAllTypes(ForPartialVectors<TestSetAtOrBeforeFirst>());
}

struct TestSetOnlyFirst {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(di);
    auto bool_lanes = AllocateAligned<TI>(N);
    HWY_ASSERT(bool_lanes);
    memset(bool_lanes.get(), 0, N * sizeof(TI));
    auto expected_lanes = AllocateAligned<TI>(N);
    HWY_ASSERT(expected_lanes);

    // For all combinations of zero/nonzero state of subset of lanes:
    const size_t max_lanes = AdjustedLog2Reps(HWY_MIN(N, size_t(6)));
    for (size_t code = 0; code < (1ull << max_lanes); ++code) {
      for (size_t i = 0; i < max_lanes; ++i) {
        bool_lanes[i] = (code & (1ull << i)) ? TI(1) : TI(0);
      }

      memset(expected_lanes.get(), 0, N * sizeof(TI));
      if (code != 0) {
        const size_t idx_of_first_lane =
            Num0BitsBelowLS1Bit_Nonzero64(static_cast<uint64_t>(code));
        expected_lanes[idx_of_first_lane] = TI(1);
      }

      const auto m = RebindMask(d, Gt(Load(di, bool_lanes.get()), Zero(di)));
      const auto expected_mask =
          RebindMask(d, Gt(Load(di, expected_lanes.get()), Zero(di)));

      HWY_ASSERT_MASK_EQ(d, expected_mask, SetOnlyFirst(m));
    }
  }
};

HWY_NOINLINE void TestAllSetOnlyFirst() {
  ForAllTypes(ForPartialVectors<TestSetOnlyFirst>());
}

struct TestSetAtOrAfterFirst {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(di);
    auto bool_lanes = AllocateAligned<TI>(N);
    HWY_ASSERT(bool_lanes);
    memset(bool_lanes.get(), 0, N * sizeof(TI));

    // For all combinations of zero/nonzero state of subset of lanes:
    const size_t max_lanes = AdjustedLog2Reps(HWY_MIN(N, size_t(6)));
    for (size_t code = 0; code < (1ull << max_lanes); ++code) {
      for (size_t i = 0; i < max_lanes; ++i) {
        bool_lanes[i] = (code & (1ull << i)) ? TI(1) : TI(0);
      }

      const auto m = RebindMask(d, Gt(Load(di, bool_lanes.get()), Zero(di)));

      const size_t first_set_lane_idx =
          (code != 0)
              ? Num0BitsBelowLS1Bit_Nonzero64(static_cast<uint64_t>(code))
              : N;
      const auto expected_at_or_after_first_mask =
          Not(FirstN(d, first_set_lane_idx));
      const auto actual_at_or_after_first_mask = SetAtOrAfterFirst(m);

      HWY_ASSERT_MASK_EQ(d, expected_at_or_after_first_mask,
                         actual_at_or_after_first_mask);
      HWY_ASSERT_MASK_EQ(
          d, SetOnlyFirst(m),
          And(actual_at_or_after_first_mask, SetAtOrBeforeFirst(m)));
      HWY_ASSERT_MASK_EQ(d, m, And(m, actual_at_or_after_first_mask));
      HWY_ASSERT(
          AllTrue(d, Xor(actual_at_or_after_first_mask, SetBeforeFirst(m))));
    }
  }
};

HWY_NOINLINE void TestAllSetAtOrAfterFirst() {
  ForAllTypes(ForPartialVectors<TestSetAtOrAfterFirst>());
}

struct TestDup128MaskFromMaskBits {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(di);
    constexpr size_t kLanesPer16ByteBlock = 16 / sizeof(T);

    auto expected = AllocateAligned<TI>(N);
    HWY_ASSERT(expected);

    // For all combinations of zero/nonzero state of subset of lanes:
    constexpr size_t kMaxLanesToCheckPerBlk =
        HWY_MIN(HWY_MAX_LANES_D(D), HWY_MIN(kLanesPer16ByteBlock, 10));
    const size_t max_lanes = HWY_MIN(N, kMaxLanesToCheckPerBlk);

    for (unsigned code = 0; code < (1u << max_lanes); ++code) {
      for (size_t i = 0; i < N; i++) {
        expected[i] = static_cast<TI>(
            -static_cast<TI>((code >> (i & (kLanesPer16ByteBlock - 1))) & 1));
      }

      const auto expected_mask =
          MaskFromVec(BitCast(d, LoadDup128(di, expected.get())));

      const auto m = Dup128MaskFromMaskBits(d, code);
      HWY_ASSERT_VEC_EQ(di, expected.get(), VecFromMask(di, RebindMask(di, m)));
      HWY_ASSERT_MASK_EQ(d, expected_mask, m);
    }
  }
};

HWY_NOINLINE void TestAllDup128MaskFromMaskBits() {
  ForAllTypes(ForPartialVectors<TestDup128MaskFromMaskBits>());
}

struct TestSetMask {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto expected_false_mask = MaskFalse(d);

    const auto false_mask_1 =
        SetMask(d, static_cast<bool>(hwy::Unpredictable1() - 1));
    HWY_ASSERT(AllFalse(d, false_mask_1));
    HWY_ASSERT(!AllTrue(d, false_mask_1));
    HWY_ASSERT(CountTrue(d, false_mask_1) == 0);
    HWY_ASSERT(FindFirstTrue(d, false_mask_1) == -1);
    HWY_ASSERT(FindLastTrue(d, false_mask_1) == -1);
    HWY_ASSERT_MASK_EQ(d, expected_false_mask, false_mask_1);

    const auto false_mask_2 = SetMask(d, false);
    HWY_ASSERT(AllFalse(d, false_mask_2));
    HWY_ASSERT(!AllTrue(d, false_mask_2));
    HWY_ASSERT(CountTrue(d, false_mask_2) == 0);
    HWY_ASSERT(FindFirstTrue(d, false_mask_2) == -1);
    HWY_ASSERT(FindLastTrue(d, false_mask_2) == -1);
    HWY_ASSERT_MASK_EQ(d, expected_false_mask, false_mask_2);

    const size_t N = Lanes(d);
    const auto expected_true_mask = MaskTrue(d);

    const auto true_mask_1 =
        SetMask(d, static_cast<bool>(hwy::Unpredictable1()));
    HWY_ASSERT(!AllFalse(d, true_mask_1));
    HWY_ASSERT(AllTrue(d, true_mask_1));
    HWY_ASSERT(CountTrue(d, true_mask_1) == N);
    HWY_ASSERT(FindFirstTrue(d, true_mask_1) == 0);
    HWY_ASSERT(FindLastTrue(d, true_mask_1) == static_cast<intptr_t>(N - 1));
    HWY_ASSERT_MASK_EQ(d, expected_true_mask, true_mask_1);

    const auto true_mask_2 = SetMask(d, true);
    HWY_ASSERT(!AllFalse(d, true_mask_2));
    HWY_ASSERT(AllTrue(d, true_mask_2));
    HWY_ASSERT(CountTrue(d, true_mask_2) == N);
    HWY_ASSERT(FindFirstTrue(d, true_mask_2) == 0);
    HWY_ASSERT(FindLastTrue(d, true_mask_2) == static_cast<intptr_t>(N - 1));
    HWY_ASSERT_MASK_EQ(d, expected_true_mask, true_mask_2);
  }
};

HWY_NOINLINE void TestAllSetMask() {
  ForAllTypes(ForPartialVectors<TestSetMask>());
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_BEFORE_TEST(HwyMaskSetTest);
HWY_EXPORT_AND_TEST_P(HwyMaskSetTest, TestAllMaskFalse);
HWY_EXPORT_AND_TEST_P(HwyMaskSetTest, TestAllFirstN);
HWY_EXPORT_AND_TEST_P(HwyMaskSetTest, TestAllSetBeforeFirst);
HWY_EXPORT_AND_TEST_P(HwyMaskSetTest, TestAllSetAtOrBeforeFirst);
HWY_EXPORT_AND_TEST_P(HwyMaskSetTest, TestAllSetOnlyFirst);
HWY_EXPORT_AND_TEST_P(HwyMaskSetTest, TestAllSetAtOrAfterFirst);
HWY_EXPORT_AND_TEST_P(HwyMaskSetTest, TestAllDup128MaskFromMaskBits);
HWY_EXPORT_AND_TEST_P(HwyMaskSetTest, TestAllSetMask);
HWY_AFTER_TEST();
}  // namespace
}  // namespace hwy
HWY_TEST_MAIN();
#endif  // HWY_ONCE
