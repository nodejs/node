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
#include <string.h>  // memset

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/mask_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {

struct TestMaskFromVec {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto lanes = AllocateAligned<T>(N);
    HWY_ASSERT(lanes);

    memset(lanes.get(), 0, N * sizeof(T));
    const Mask<D> actual_false = MaskFromVec(Load(d, lanes.get()));
    HWY_ASSERT_MASK_EQ(d, MaskFalse(d), actual_false);

    memset(lanes.get(), 0xFF, N * sizeof(T));
    const Mask<D> actual_true = MaskFromVec(Load(d, lanes.get()));
    HWY_ASSERT_MASK_EQ(d, MaskTrue(d), actual_true);
  }
};

HWY_NOINLINE void TestAllMaskFromVec() {
  ForAllTypes(ForPartialVectors<TestMaskFromVec>());
}

// Round trip, using MaskFromVec.
struct TestVecFromMask {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    RandomState rng;

    using M = Mask<D>;  // == MFromD<D>
// Ensure DFromM works on all targets except `SVE` and `RVV`, whose built-in
// mask types are not strongly typed.
#if !HWY_TARGET_IS_SVE && HWY_TARGET != HWY_RVV
    static_assert(hwy::IsSame<DFromM<M>, D>(), "");
#endif

    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(d);
    auto lanes = AllocateAligned<TI>(N);
    HWY_ASSERT(lanes);

    // Each lane should have a chance of having mask=true.
    for (size_t rep = 0; rep < AdjustedReps(200); ++rep) {
      for (size_t i = 0; i < N; ++i) {
        lanes[i] = (Random32(&rng) & 1024) ? TI(1) : TI(0);
      }

      const M mask = RebindMask(d, Gt(Load(di, lanes.get()), Zero(di)));
      HWY_ASSERT_MASK_EQ(d, mask, MaskFromVec(VecFromMask(d, mask)));
    }
  }
};

HWY_NOINLINE void TestAllVecFromMask() {
  ForAllTypes(ForPartialVectors<TestVecFromMask>());
}

struct TestBitsFromMask {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_MAX_BYTES > 64
    (void)d;
#else
    RandomState rng;

    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(d);
    HWY_ASSERT(N <= 64);  // non-scalable targets have at most 512 bits.
    auto lanes = AllocateAligned<TI>(N);
    HWY_ASSERT(lanes);

    // Each lane should have a chance of having mask=true.
    for (size_t rep = 0; rep < AdjustedReps(200); ++rep) {
      uint64_t expected_bits = 0;
      for (size_t i = 0; i < N; ++i) {
        lanes[i] = (Random32(&rng) & 1024) ? TI(1) : TI(0);
        expected_bits |= lanes[i] ? (1ull << i) : 0;
      }

      const Mask<D> mask = RebindMask(d, Gt(Load(di, lanes.get()), Zero(di)));
      const uint64_t actual_bits = BitsFromMask(d, mask);
      HWY_ASSERT_EQ(expected_bits, actual_bits);
    }
#endif  // HWY_MAX_BYTES > 64
  }
};

HWY_NOINLINE void TestAllBitsFromMask() {
  ForAllTypes(ForPartialVectors<TestBitsFromMask>());
}

struct TestAllTrueFalse {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto zero = Zero(d);
    auto v = zero;

    const size_t N = Lanes(d);
    auto lanes = AllocateAligned<T>(N);
    HWY_ASSERT(lanes);
    ZeroBytes(lanes.get(), N * sizeof(T));

    HWY_ASSERT(AllTrue(d, Eq(v, zero)));
    HWY_ASSERT(!AllFalse(d, Eq(v, zero)));

    // Single lane implies AllFalse = !AllTrue. Otherwise, there are multiple
    // lanes and one is nonzero.
    const bool expected_all_false = (N != 1);

    // Set each lane to nonzero and back to zero
    for (size_t i = 0; i < N; ++i) {
      lanes[i] = ConvertScalarTo<T>(1);
      v = Load(d, lanes.get());

      HWY_ASSERT(!AllTrue(d, Eq(v, zero)));

      HWY_ASSERT(expected_all_false ^ AllFalse(d, Eq(v, zero)));

      lanes[i] = ConvertScalarTo<T>(-1);
      v = Load(d, lanes.get());
      HWY_ASSERT(!AllTrue(d, Eq(v, zero)));
      HWY_ASSERT(expected_all_false ^ AllFalse(d, Eq(v, zero)));

      // Reset to all zero
      lanes[i] = ConvertScalarTo<T>(0);
      v = Load(d, lanes.get());
      HWY_ASSERT(AllTrue(d, Eq(v, zero)));
      HWY_ASSERT(!AllFalse(d, Eq(v, zero)));
    }
  }
};

HWY_NOINLINE void TestAllAllTrueFalse() {
  ForAllTypes(ForPartialVectors<TestAllTrueFalse>());
}

struct TestCountTrue {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(di);
    auto bool_lanes = AllocateAligned<TI>(N);
    HWY_ASSERT(bool_lanes);
    memset(bool_lanes.get(), 0, N * sizeof(TI));

    // For all combinations of zero/nonzero state of subset of lanes:
    const size_t max_lanes = HWY_MIN(N, size_t(10));

    for (size_t code = 0; code < (1ull << max_lanes); ++code) {
      // Number of zeros written = number of mask lanes that are true.
      size_t expected = 0;
      for (size_t i = 0; i < max_lanes; ++i) {
        const bool is_true = (code & (1ull << i)) != 0;
        bool_lanes[i] = is_true ? TI(1) : TI(0);
        expected += is_true;
      }

      const auto mask = RebindMask(d, Gt(Load(di, bool_lanes.get()), Zero(di)));
      const size_t actual = CountTrue(d, mask);
      HWY_ASSERT_EQ(expected, actual);
    }
  }
};

HWY_NOINLINE void TestAllCountTrue() {
  ForAllTypes(ForPartialVectors<TestCountTrue>());
}

struct TestFindFirstTrue {  // Also FindKnownFirstTrue
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(di);
    auto bool_lanes = AllocateAligned<TI>(N);
    HWY_ASSERT(bool_lanes);
    memset(bool_lanes.get(), 0, N * sizeof(TI));

    // For all combinations of zero/nonzero state of subset of lanes:
    const size_t max_lanes = AdjustedLog2Reps(HWY_MIN(N, size_t(9)));

    HWY_ASSERT_EQ(intptr_t(-1), FindFirstTrue(d, MaskFalse(d)));
    HWY_ASSERT_EQ(intptr_t(0), FindFirstTrue(d, MaskTrue(d)));
    HWY_ASSERT_EQ(size_t(0), FindKnownFirstTrue(d, MaskTrue(d)));

    for (size_t code = 1; code < (1ull << max_lanes); ++code) {
      for (size_t i = 0; i < max_lanes; ++i) {
        bool_lanes[i] = (code & (1ull << i)) ? TI(1) : TI(0);
      }

      const size_t expected =
          Num0BitsBelowLS1Bit_Nonzero32(static_cast<uint32_t>(code));
      const auto mask = RebindMask(d, Gt(Load(di, bool_lanes.get()), Zero(di)));
      HWY_ASSERT_EQ(static_cast<intptr_t>(expected), FindFirstTrue(d, mask));
      HWY_ASSERT_EQ(expected, FindKnownFirstTrue(d, mask));
    }
  }
};

HWY_NOINLINE void TestAllFindFirstTrue() {
  ForAllTypes(ForPartialVectors<TestFindFirstTrue>());
}

struct TestFindLastTrue {  // Also FindKnownLastTrue
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(di);
    auto bool_lanes = AllocateAligned<TI>(N);
    HWY_ASSERT(bool_lanes);
    memset(bool_lanes.get(), 0, N * sizeof(TI));

    // For all combinations of zero/nonzero state of subset of lanes:
    const size_t max_lanes = AdjustedLog2Reps(HWY_MIN(N, size_t(9)));

    HWY_ASSERT_EQ(intptr_t(-1), FindLastTrue(d, MaskFalse(d)));
    HWY_ASSERT_EQ(intptr_t(Lanes(d) - 1), FindLastTrue(d, MaskTrue(d)));
    HWY_ASSERT_EQ(size_t(Lanes(d) - 1), FindKnownLastTrue(d, MaskTrue(d)));

    for (size_t code = 1; code < (1ull << max_lanes); ++code) {
      for (size_t i = 0; i < max_lanes; ++i) {
        bool_lanes[i] = (code & (1ull << i)) ? TI(1) : TI(0);
      }

      const size_t expected =
          31 - Num0BitsAboveMS1Bit_Nonzero32(static_cast<uint32_t>(code));
      const auto mask = RebindMask(d, Gt(Load(di, bool_lanes.get()), Zero(di)));
      HWY_ASSERT_EQ(static_cast<intptr_t>(expected), FindLastTrue(d, mask));
      HWY_ASSERT_EQ(expected, FindKnownLastTrue(d, mask));
    }
  }
};

HWY_NOINLINE void TestAllFindLastTrue() {
  ForAllTypes(ForPartialVectors<TestFindLastTrue>());
}

struct TestLogicalMask {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto m0 = MaskFalse(d);
    const auto m_all = MaskTrue(d);

    using TI = MakeSigned<T>;  // For mask > 0 comparison
    const Rebind<TI, D> di;
    const size_t N = Lanes(di);
    auto bool_lanes = AllocateAligned<TI>(N);
    HWY_ASSERT(bool_lanes);
    memset(bool_lanes.get(), 0, N * sizeof(TI));

    HWY_ASSERT_MASK_EQ(d, m0, Not(m_all));
    HWY_ASSERT_MASK_EQ(d, m_all, Not(m0));

    HWY_ASSERT_MASK_EQ(d, m_all, ExclusiveNeither(m0, m0));
    HWY_ASSERT_MASK_EQ(d, m0, ExclusiveNeither(m_all, m0));
    HWY_ASSERT_MASK_EQ(d, m0, ExclusiveNeither(m0, m_all));

    // For all combinations of zero/nonzero state of subset of lanes:
    const size_t max_lanes = AdjustedLog2Reps(HWY_MIN(N, size_t(6)));
    for (size_t code = 0; code < (1ull << max_lanes); ++code) {
      for (size_t i = 0; i < max_lanes; ++i) {
        bool_lanes[i] = (code & (1ull << i)) ? TI(1) : TI(0);
      }

      const auto m = RebindMask(d, Gt(Load(di, bool_lanes.get()), Zero(di)));

      HWY_ASSERT_MASK_EQ(d, m0, Xor(m, m));
      HWY_ASSERT_MASK_EQ(d, m0, AndNot(m, m));
      HWY_ASSERT_MASK_EQ(d, m0, AndNot(m_all, m));

      HWY_ASSERT_MASK_EQ(d, m, Or(m, m));
      HWY_ASSERT_MASK_EQ(d, m, Or(m0, m));
      HWY_ASSERT_MASK_EQ(d, m, Or(m, m0));
      HWY_ASSERT_MASK_EQ(d, m, Xor(m0, m));
      HWY_ASSERT_MASK_EQ(d, m, Xor(m, m0));
      HWY_ASSERT_MASK_EQ(d, m, And(m, m));
      HWY_ASSERT_MASK_EQ(d, m, And(m_all, m));
      HWY_ASSERT_MASK_EQ(d, m, And(m, m_all));
      HWY_ASSERT_MASK_EQ(d, m, AndNot(m0, m));
    }
  }
};

HWY_NOINLINE void TestAllLogicalMask() {
  ForAllTypes(ForPartialVectors<TestLogicalMask>());
}

struct TestMaskedSetOr {
  template <class D>
  void testWithMask(D d, MFromD<D> m) {
    TFromD<D> a = 1;
    auto yes = Set(d, a);
    auto no = Set(d, 2);
    auto expected = IfThenElse(m, yes, no);
    auto actual = MaskedSetOr(no, m, a);
    HWY_ASSERT_VEC_EQ(d, expected, actual);
  }
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // All False
    testWithMask(d, MaskFalse(d));
    auto N = Lanes(d);
    // All True
    testWithMask(d, FirstN(d, N));
    // Lower half
    testWithMask(d, FirstN(d, N / 2));
    // Upper half
    testWithMask(d, Not(FirstN(d, N / 2)));
    // Interleaved
    testWithMask(d,
                 MaskFromVec(InterleaveLower(Zero(d), Set(d, (TFromD<D>)-1))));
  }
};

HWY_NOINLINE void TestAllMaskedSetOr() {
  ForAllTypes(ForShrinkableVectors<TestMaskedSetOr>());
}

struct TestMaskedSet {
  template <class D>
  void testWithMask(D d, MFromD<D> m) {
    TFromD<D> a = 1;
    auto yes = Set(d, a);
    auto no = Zero(d);
    auto expected = IfThenElse(m, yes, no);
    auto actual = MaskedSet(d, m, a);
    HWY_ASSERT_VEC_EQ(d, expected, actual);
  }
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // All False
    testWithMask(d, MaskFalse(d));
    auto N = Lanes(d);
    // All True
    testWithMask(d, FirstN(d, N));
    // Lower half
    testWithMask(d, FirstN(d, N / 2));
    // Upper half
    testWithMask(d, Not(FirstN(d, N / 2)));
    // Interleaved
    testWithMask(d,
                 MaskFromVec(InterleaveLower(Zero(d), Set(d, (TFromD<D>)-1))));
  }
};

HWY_NOINLINE void TestAllMaskedSet() {
  ForAllTypes(ForShrinkableVectors<TestMaskedSet>());
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_BEFORE_TEST(HwyMaskTest);
HWY_EXPORT_AND_TEST_P(HwyMaskTest, TestAllMaskFromVec);
HWY_EXPORT_AND_TEST_P(HwyMaskTest, TestAllVecFromMask);
HWY_EXPORT_AND_TEST_P(HwyMaskTest, TestAllBitsFromMask);
HWY_EXPORT_AND_TEST_P(HwyMaskTest, TestAllAllTrueFalse);
HWY_EXPORT_AND_TEST_P(HwyMaskTest, TestAllCountTrue);
HWY_EXPORT_AND_TEST_P(HwyMaskTest, TestAllFindFirstTrue);
HWY_EXPORT_AND_TEST_P(HwyMaskTest, TestAllFindLastTrue);
HWY_EXPORT_AND_TEST_P(HwyMaskTest, TestAllLogicalMask);
HWY_EXPORT_AND_TEST_P(HwyMaskTest, TestAllMaskedSetOr);
HWY_EXPORT_AND_TEST_P(HwyMaskTest, TestAllMaskedSet);
HWY_AFTER_TEST();
}  // namespace
}  // namespace hwy
HWY_TEST_MAIN();
#endif  // HWY_ONCE
