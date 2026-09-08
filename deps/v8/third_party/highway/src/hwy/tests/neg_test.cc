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

#include <stddef.h>
#include <stdint.h>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/neg_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/nanobenchmark.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {

struct TestAbs {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Vec<D> v0 = Zero(d);
    const Vec<D> vp1 = Set(d, static_cast<T>(1));
    const Vec<D> vn1 = Set(d, static_cast<T>(-1));
    const Vec<D> vpm = Set(d, LimitsMax<T>());
    const Vec<D> vnm = Set(d, LimitsMin<T>());

    HWY_ASSERT_VEC_EQ(d, v0, Abs(v0));
    HWY_ASSERT_VEC_EQ(d, vp1, Abs(vp1));
    HWY_ASSERT_VEC_EQ(d, vp1, Abs(vn1));
    HWY_ASSERT_VEC_EQ(d, vpm, Abs(vpm));
    HWY_ASSERT_VEC_EQ(d, vnm, Abs(vnm));
  }
};

struct TestFloatAbs {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Vec<D> v0 = Zero(d);
    const Vec<D> vp1 = Set(d, ConvertScalarTo<T>(1));
    const Vec<D> vn1 = Set(d, ConvertScalarTo<T>(-1));
    const Vec<D> vp2 = Set(d, ConvertScalarTo<T>(0.01));
    const Vec<D> vn2 = Set(d, ConvertScalarTo<T>(-0.01));

    HWY_ASSERT_VEC_EQ(d, v0, Abs(v0));
    HWY_ASSERT_VEC_EQ(d, vp1, Abs(vp1));
    HWY_ASSERT_VEC_EQ(d, vp1, Abs(vn1));
    HWY_ASSERT_VEC_EQ(d, vp2, Abs(vp2));
    HWY_ASSERT_VEC_EQ(d, vp2, Abs(vn2));
  }
};

struct TestMaskedAbs {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const MFromD<D> zero_mask = MaskFalse(d);
    const MFromD<D> first_five = FirstN(d, 5);

    const Vec<D> v0 = Zero(d);
    const Vec<D> vp1 = Set(d, ConvertScalarTo<T>(hwy::Unpredictable1()));
    const Vec<D> vn1 = Neg(vp1);
    const Vec<D> vp2 = Set(d, ConvertScalarTo<T>(0.01));
    const Vec<D> vn2 = Set(d, ConvertScalarTo<T>(-0.01));

    // Test that mask is applied correctly for MaskedAbsOr
    const Vec<D> v1_exp = IfThenElse(first_five, vp1, vn1);
    const Vec<D> v2_exp = IfThenElse(first_five, vp2, vn2);

    HWY_ASSERT_VEC_EQ(d, v1_exp, MaskedAbsOr(vn1, first_five, vn1));
    HWY_ASSERT_VEC_EQ(d, v2_exp, MaskedAbsOr(vn2, first_five, vn2));

    // Test that zero mask will return all zeroes for MaskedAbs
    HWY_ASSERT_VEC_EQ(d, v0, MaskedAbs(zero_mask, vn1));

    // Test that zero is returned in cases m==0 for MaskedAbs
    const Vec<D> v1_exp_z = IfThenElseZero(first_five, vp1);
    const Vec<D> v2_exp_z = IfThenElseZero(first_five, vp2);

    HWY_ASSERT_VEC_EQ(d, v1_exp_z, MaskedAbs(first_five, vn1));
    HWY_ASSERT_VEC_EQ(d, v2_exp_z, MaskedAbs(first_five, vn2));
  }
};

HWY_NOINLINE void TestAllAbs() {
  ForSignedTypes(ForPartialVectors<TestAbs>());
  ForFloatTypes(ForPartialVectors<TestFloatAbs>());
  ForSignedTypes(ForPartialVectors<TestMaskedAbs>());
}

struct TestIntegerNeg {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const RebindToUnsigned<D> du;
    using TU = TFromD<decltype(du)>;
    const Vec<D> v0 = Zero(d);
    const Vec<D> v1 = BitCast(d, Set(du, TU{1}));
    const Vec<D> vp = BitCast(d, Set(du, TU{3}));
    const Vec<D> vn = Add(Not(vp), v1);  // 2's complement
    HWY_ASSERT_VEC_EQ(d, v0, Neg(v0));
    HWY_ASSERT_VEC_EQ(d, vp, Neg(vn));
    HWY_ASSERT_VEC_EQ(d, vn, Neg(vp));
  }
};

struct TestFloatNeg {
  // Must be inlined on aarch64 for bf16, else clang crashes.
  template <typename T, class D>
  HWY_INLINE void operator()(T /*unused*/, D d) {
    const RebindToUnsigned<D> du;
    using TU = TFromD<decltype(du)>;
    // 1.25 in binary16.
    const Vec<D> vp =
        BitCast(d, Set(du, static_cast<TU>(Unpredictable1() * 0x3D00)));
    // Flip sign bit in MSB
    const Vec<D> vn = BitCast(d, Xor(BitCast(du, vp), SignBit(du)));
    // Do not check negative zero - we do not yet have proper bfloat16_t Eq().
    HWY_ASSERT_VEC_EQ(du, BitCast(du, vp), BitCast(du, Neg(vn)));
    HWY_ASSERT_VEC_EQ(du, BitCast(du, vn), BitCast(du, Neg(vp)));
  }
};

struct TestNegOverflow {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto vn = Set(d, LimitsMin<T>());
    const auto vp = Set(d, LimitsMax<T>());
    HWY_ASSERT_VEC_EQ(d, Neg(vn), Neg(vn));
    HWY_ASSERT_VEC_EQ(d, Neg(vp), Neg(vp));
  }
};

HWY_NOINLINE void TestAllNeg() {
  ForFloatTypes(ForPartialVectors<TestFloatNeg>());
  // Always supported, even if !HWY_HAVE_FLOAT16.
  ForPartialVectors<TestFloatNeg>()(float16_t());

  ForSignedTypes(ForPartialVectors<TestIntegerNeg>());

  ForSignedTypes(ForPartialVectors<TestNegOverflow>());
}

struct TestIntegerAbsDiff {
  template <typename T, HWY_IF_T_SIZE_ONE_OF(T, (1 << 1) | (1 << 2) | (1 << 4))>
  static inline T ScalarAbsDiff(T a, T b) {
    using TW = MakeSigned<MakeWide<T>>;
    const TW diff = static_cast<TW>(static_cast<TW>(a) - static_cast<TW>(b));
    return static_cast<T>((diff >= 0) ? diff : -diff);
  }
  template <typename T, HWY_IF_T_SIZE(T, 8)>
  static inline T ScalarAbsDiff(T a, T b) {
    if (a >= b) {
      return static_cast<T>(static_cast<uint64_t>(a) -
                            static_cast<uint64_t>(b));
    } else {
      return static_cast<T>(static_cast<uint64_t>(b) -
                            static_cast<uint64_t>(a));
    }
  }

  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    auto in_lanes_a = AllocateAligned<T>(N);
    auto in_lanes_b = AllocateAligned<T>(N);
    auto out_lanes = AllocateAligned<T>(N);
    HWY_ASSERT(in_lanes_a && in_lanes_b && out_lanes);
    constexpr size_t shift_amt_mask = sizeof(T) * 8 - 1;
    for (size_t i = 0; i < N; ++i) {
      // Need to mask out shift_amt as i can be greater than or equal to
      // the number of bits in T if T is int8_t, uint8_t, int16_t, or uint16_t.
      const auto shift_amt = i & shift_amt_mask;
      in_lanes_a[i] =
          static_cast<T>((static_cast<uint64_t>(i) ^ 1u) << shift_amt);
      in_lanes_b[i] = static_cast<T>(static_cast<uint64_t>(i) << shift_amt);
      out_lanes[i] = ScalarAbsDiff(in_lanes_a[i], in_lanes_b[i]);
    }
    const auto a = Load(d, in_lanes_a.get());
    const auto b = Load(d, in_lanes_b.get());
    const auto expected = Load(d, out_lanes.get());
    HWY_ASSERT_VEC_EQ(d, expected, AbsDiff(a, b));
    HWY_ASSERT_VEC_EQ(d, expected, AbsDiff(b, a));
  }
};

HWY_NOINLINE void TestAllIntegerAbsDiff() {
  ForPartialVectors<TestIntegerAbsDiff>()(int8_t());
  ForPartialVectors<TestIntegerAbsDiff>()(uint8_t());
  ForPartialVectors<TestIntegerAbsDiff>()(int16_t());
  ForPartialVectors<TestIntegerAbsDiff>()(uint16_t());
  ForPartialVectors<TestIntegerAbsDiff>()(int32_t());
  ForPartialVectors<TestIntegerAbsDiff>()(uint32_t());
#if HWY_HAVE_INTEGER64
  ForPartialVectors<TestIntegerAbsDiff>()(int64_t());
  ForPartialVectors<TestIntegerAbsDiff>()(uint64_t());
#endif
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_BEFORE_TEST(HwyNegTest);
HWY_EXPORT_AND_TEST_P(HwyNegTest, TestAllAbs);
HWY_EXPORT_AND_TEST_P(HwyNegTest, TestAllNeg);
HWY_EXPORT_AND_TEST_P(HwyNegTest, TestAllIntegerAbsDiff);
HWY_AFTER_TEST();
}  // namespace
}  // namespace hwy
HWY_TEST_MAIN();
#endif  // HWY_ONCE
