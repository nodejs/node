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
#define HWY_TARGET_INCLUDE "tests/arithmetic_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/nanobenchmark.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {

struct TestPlusMinus {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v2 = Iota(d, hwy::Unpredictable1() + 1);
    const auto v3 = Iota(d, hwy::Unpredictable1() + 2);
    const auto v4 = Iota(d, hwy::Unpredictable1() + 3);

    const size_t N = Lanes(d);
    auto lanes = AllocateAligned<T>(N);
    HWY_ASSERT(lanes);
    for (size_t i = 0; i < N; ++i) {
      lanes[i] = ConvertScalarTo<T>((2 + i) + (3 + i));
    }
    HWY_ASSERT_VEC_EQ(d, lanes.get(), Add(v2, v3));
    HWY_ASSERT_VEC_EQ(d, Set(d, ConvertScalarTo<T>(2)), Sub(v4, v2));

    for (size_t i = 0; i < N; ++i) {
      lanes[i] = ConvertScalarTo<T>((2 + i) + (4 + i));
    }
    auto sum = v2;
    sum = Add(sum, v4);  // sum == 6,8..
    HWY_ASSERT_VEC_EQ(d, Load(d, lanes.get()), sum);

    sum = Sub(sum, v4);
    HWY_ASSERT_VEC_EQ(d, v2, sum);
  }
};

struct TestPlusMinusOverflow {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v1 = Iota(d, 1);
    const auto vMax = Iota(d, LimitsMax<T>());
    const auto vMin = Iota(d, LimitsMin<T>());

    // Check that no UB triggered.
    // "assert" here is formal - to avoid compiler dropping calculations
    HWY_ASSERT_VEC_EQ(d, Add(v1, vMax), Add(vMax, v1));
    HWY_ASSERT_VEC_EQ(d, Add(vMax, vMax), Add(vMax, vMax));
    HWY_ASSERT_VEC_EQ(d, Sub(vMin, v1), Sub(vMin, v1));
    HWY_ASSERT_VEC_EQ(d, Sub(vMin, vMax), Sub(vMin, vMax));
  }
};

HWY_NOINLINE void TestAllPlusMinus() {
  ForAllTypes(ForPartialVectors<TestPlusMinus>());
  ForIntegerTypes(ForPartialVectors<TestPlusMinusOverflow>());
}

struct TestAddSub {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto v2 = Iota(d, 2);
    const auto v4 = Iota(d, 4);

    const size_t N = Lanes(d);
    auto lanes = AllocateAligned<T>(N);
    HWY_ASSERT(lanes);
    for (size_t i = 0; i < N; ++i) {
      lanes[i] = ConvertScalarTo<T>(((i & 1) == 0) ? 2 : ((4 + i) + (2 + i)));
    }
    HWY_ASSERT_VEC_EQ(d, lanes.get(), AddSub(v4, v2));

    for (size_t i = 0; i < N; ++i) {
      if ((i & 1) == 0) {
        lanes[i] = ConvertScalarTo<T>(-2);
      }
    }
    HWY_ASSERT_VEC_EQ(d, lanes.get(), AddSub(v2, v4));
  }
};

HWY_NOINLINE void TestAllAddSub() {
  ForAllTypes(ForPartialVectors<TestAddSub>());
}

struct TestAverage {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using TI = MakeSigned<T>;

    const RebindToSigned<decltype(d)> di;
    const RebindToUnsigned<decltype(d)> du;

    const Vec<D> v0 = Zero(d);
    const Vec<D> v1 = Set(d, static_cast<T>(1));
    const Vec<D> v2 = Set(d, static_cast<T>(2));

    const Vec<D> vn1 = Set(d, static_cast<T>(-1));
    const Vec<D> vn2 = Set(d, static_cast<T>(-2));
    const Vec<D> vn3 = Set(d, static_cast<T>(-3));
    const Vec<D> vn4 = Set(d, static_cast<T>(-4));

    HWY_ASSERT_VEC_EQ(d, v0, AverageRound(v0, v0));
    HWY_ASSERT_VEC_EQ(d, v1, AverageRound(v0, v1));
    HWY_ASSERT_VEC_EQ(d, v1, AverageRound(v1, v1));
    HWY_ASSERT_VEC_EQ(d, v2, AverageRound(v1, v2));
    HWY_ASSERT_VEC_EQ(d, v2, AverageRound(v2, v2));

    HWY_ASSERT_VEC_EQ(d, vn1, AverageRound(vn1, vn1));
    HWY_ASSERT_VEC_EQ(d, vn1, AverageRound(vn1, vn2));
    HWY_ASSERT_VEC_EQ(d, vn2, AverageRound(vn1, vn3));
    HWY_ASSERT_VEC_EQ(d, vn2, AverageRound(vn1, vn4));
    HWY_ASSERT_VEC_EQ(d, vn2, AverageRound(vn2, vn2));
    HWY_ASSERT_VEC_EQ(d, vn3, AverageRound(vn2, vn4));

    const T kSignedMax = static_cast<T>(LimitsMax<TI>());

    const Vec<D> v_iota1 = Iota(d, static_cast<T>(1));
    Vec<D> v_neg_even = BitCast(d, Neg(BitCast(di, Add(v_iota1, v_iota1))));
    HWY_IF_CONSTEXPR(HWY_MAX_LANES_D(D) > static_cast<size_t>(kSignedMax)) {
      v_neg_even = Or(v_neg_even, SignBit(d));
    }

    const Vec<D> v_pos_even = And(v_neg_even, Set(d, kSignedMax));
    const Vec<D> v_pos_odd = Or(v_pos_even, v1);

    const Vec<D> expected_even =
        Add(ShiftRight<1>(v_neg_even),
            BitCast(d, ShiftRight<1>(BitCast(du, v_pos_even))));

    HWY_ASSERT_VEC_EQ(d, expected_even, AverageRound(v_neg_even, v_pos_even));
    HWY_ASSERT_VEC_EQ(d, Add(expected_even, v1),
                      AverageRound(v_neg_even, v_pos_odd));
  }
};

HWY_NOINLINE void TestAllAverage() {
  ForIntegerTypes(ForPartialVectors<TestAverage>());
}

struct TestPairwiseAdd {
  template <typename T, class D, HWY_IF_LANES_GT_D(D, 1)>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Vec<D> a = Iota(d, 1);
    const Vec<D> b = Iota(d, 2);

    const size_t N = Lanes(d);
    if (N < 2) {
      return;
    }
    T even_val_a, odd_val_a, even_val_b, odd_val_b;
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    for (size_t i = 0; i < N; i += 2) {
      // Results of a and b are interleaved
      even_val_a = ConvertScalarTo<T>(i + 1);     // a[i]
      odd_val_a = ConvertScalarTo<T>(i + 1 + 1);  // a[i+1]
      even_val_b = ConvertScalarTo<T>(i + 2);     // b[i]
      odd_val_b = ConvertScalarTo<T>(i + 2 + 1);  // b[i+1]

      expected[i] = ConvertScalarTo<T>(even_val_a + odd_val_a);
      expected[i + 1] = ConvertScalarTo<T>(even_val_b + odd_val_b);
    }

    HWY_ASSERT_VEC_EQ(d, expected.get(), PairwiseAdd(d, a, b));
  }

  template <typename T, class D, HWY_IF_LANES_D(D, 1)>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    (void)d;
  }
};

struct TestPairwiseSub {
  template <typename T, class D, HWY_IF_LANES_GT_D(D, 1)>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const size_t N = Lanes(d);
    if (N < 2) {
      return;
    }

    auto a_lanes = AllocateAligned<T>(N);
    auto b_lanes = AllocateAligned<T>(N);
    HWY_ASSERT(a_lanes && b_lanes);

    T even_val_a, odd_val_a, even_val_b, odd_val_b;
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    for (size_t i = 0; i < N; i += 2) {
      // Results of a and are interleaved
      even_val_a = ConvertScalarTo<T>(i);         // a[i]
      odd_val_a = ConvertScalarTo<T>(i * i);      // a[i+1]
      even_val_b = ConvertScalarTo<T>(i);         // b[i]
      odd_val_b = ConvertScalarTo<T>(i * i + 2);  // b[i+1]

      a_lanes[i] = even_val_a;     // a[i]
      a_lanes[i + 1] = odd_val_a;  // a[i+1]
      b_lanes[i] = even_val_b;     // b[i]
      b_lanes[i + 1] = odd_val_b;  // b[i+1]

      expected[i] = ConvertScalarTo<T>(odd_val_a - even_val_a);
      expected[i + 1] = ConvertScalarTo<T>(odd_val_b - even_val_b);
    }

    const auto a = Load(d, a_lanes.get());
    const auto b = Load(d, b_lanes.get());

    HWY_ASSERT_VEC_EQ(d, expected.get(), PairwiseSub(d, a, b));
  }

  template <typename T, class D, HWY_IF_LANES_D(D, 1)>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    (void)d;
  }
};

struct TestPairwiseAdd128 {
  template <typename T, class D, HWY_IF_LANES_GT_D(D, 1)>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    auto a = Iota(d, 1);
    auto b = Iota(d, 2);
    T even_val_a, odd_val_a, even_val_b, odd_val_b;
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);

    const size_t vec_bytes = sizeof(T) * N;
    const size_t blocks_of_128 = (vec_bytes >= 16) ? vec_bytes / 16 : 1;
    const size_t lanes_in_128 = (vec_bytes >= 16) ? 16 / sizeof(T) : N;

    for (size_t block = 0; block < blocks_of_128; ++block) {
      for (size_t i = 0; i < lanes_in_128 / 2; ++i) {
        size_t j = 2 * (block * lanes_in_128 / 2 + i);

        even_val_a = ConvertScalarTo<T>(j + 1);
        odd_val_a = ConvertScalarTo<T>(j + 2);
        even_val_b = ConvertScalarTo<T>(j + 2);
        odd_val_b = ConvertScalarTo<T>(j + 3);

        expected[block * lanes_in_128 + i] =
            ConvertScalarTo<T>(even_val_a + odd_val_a);
        expected[block * lanes_in_128 + lanes_in_128 / 2 + i] =
            ConvertScalarTo<T>(even_val_b + odd_val_b);
      }
    }
    const auto expected_v = Load(d, expected.get());
    auto res = PairwiseAdd128(d, a, b);
    HWY_ASSERT_VEC_EQ(d, expected_v, res);
  }

  template <typename T, class D, HWY_IF_LANES_D(D, 1)>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    (void)d;
  }
};

struct TestPairwiseSub128 {
  template <typename T, class D, HWY_IF_LANES_GT_D(D, 1)>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const auto iota0 = Iota(d, 0);

    auto a = Add(Iota(d, 1), OddEven(Zero(d), iota0));
    auto b = Add(Iota(d, 2), OddEven(iota0, Zero(d)));
    T even_val_a, odd_val_a, even_val_b, odd_val_b;
    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);

    const size_t vec_bytes = sizeof(T) * N;
    const size_t blocks_of_128 = (vec_bytes >= 16) ? vec_bytes / 16 : 1;
    const size_t lanes_in_128 = (vec_bytes >= 16) ? 16 / sizeof(T) : N;

    for (size_t block = 0; block < blocks_of_128; ++block) {
      for (size_t i = 0; i < lanes_in_128 / 2; ++i) {
        size_t j = 2 * (block * lanes_in_128 / 2 + i);

        even_val_a = ConvertScalarTo<T>(2 * j + 1);
        odd_val_a = ConvertScalarTo<T>(j + 2);
        even_val_b = ConvertScalarTo<T>(j + 2);
        odd_val_b = ConvertScalarTo<T>(2 * j + 4);

        expected[block * lanes_in_128 + i] =
            ConvertScalarTo<T>(odd_val_a - even_val_a);
        expected[block * lanes_in_128 + lanes_in_128 / 2 + i] =
            ConvertScalarTo<T>(odd_val_b - even_val_b);
      }
    }
    const auto expected_v = Load(d, expected.get());
    auto res = PairwiseSub128(d, a, b);
    HWY_ASSERT_VEC_EQ(d, expected_v, res);
  }

  template <typename T, class D, HWY_IF_LANES_D(D, 1)>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    (void)d;
  }
};

HWY_NOINLINE void TestAllPairwise() {
  ForAllTypes(ForPartialVectors<TestPairwiseAdd>());
  ForAllTypes(ForPartialVectors<TestPairwiseSub>());
  ForAllTypes(ForGEVectors<128, TestPairwiseAdd128>());
  ForAllTypes(ForGEVectors<128, TestPairwiseSub128>());
}


}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_BEFORE_TEST(HwyArithmeticTest);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllPlusMinus);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllAddSub);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllAverage);
HWY_EXPORT_AND_TEST_P(HwyArithmeticTest, TestAllPairwise);
HWY_AFTER_TEST();
}  // namespace
}  // namespace hwy
HWY_TEST_MAIN();
#endif  // HWY_ONCE
