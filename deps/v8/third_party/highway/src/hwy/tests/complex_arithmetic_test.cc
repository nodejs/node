// Copyright 2023 Google LLC
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

#include <stdint.h>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/complex_arithmetic_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {

struct TestComplexConj {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const Vec<D> v1 = Iota(d, 2);

    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    for (size_t i = 0; i < N; i += 2) {
      // expected = (x - iy)
      auto x = ConvertScalarTo<T>(i + 2);
      auto y = ConvertScalarTo<T>(i + 2 + 1);
      expected[i + 0] = ConvertScalarTo<T>(x);
      expected[i + 1] = ConvertScalarTo<T>(-y);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), ComplexConj(v1));
#else
    (void)d;
#endif  // HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllComplexConj() {
  ForSignedTypes(ForShrinkableVectors<TestComplexConj>());
  ForFloatTypes(ForShrinkableVectors<TestComplexConj>());
}

struct TestMulComplex {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const Vec<D> v1 = Iota(d, 2);
    const Vec<D> v2 = Iota(d, 5);

    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    for (size_t i = 0; i < N; i += 2) {
      // expected = (x + iy)(u + iv)
      auto x = ConvertScalarTo<T>(i + 2);
      auto y = ConvertScalarTo<T>(i + 2 + 1);
      auto u = ConvertScalarTo<T>(i + 5);
      auto v = ConvertScalarTo<T>(i + 5 + 1);
      expected[i + 0] = ConvertScalarTo<T>((x * u) - (y * v));
      expected[i + 1] = ConvertScalarTo<T>((x * v) + (y * u));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulComplex(v1, v2));
#else
    (void)d;
#endif  // HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllMulComplex() {
  ForAllTypes(ForShrinkableVectors<TestMulComplex>());
}

struct TestMulComplexAdd {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const Vec<D> v1 = Iota(d, 2);
    const Vec<D> v2 = Iota(d, 5);
    const Vec<D> v3 = Iota(d, 6);

    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    for (size_t i = 0; i < N; i += 2) {
      // expected = (x + iy)(u + iv) + a + ib
      auto x = ConvertScalarTo<T>(i + 2);
      auto y = ConvertScalarTo<T>(i + 2 + 1);
      auto u = ConvertScalarTo<T>(i + 5);
      auto v = ConvertScalarTo<T>(i + 5 + 1);
      auto a = ConvertScalarTo<T>(i + 6);
      auto b = ConvertScalarTo<T>(i + 6 + 1);
      expected[i + 0] = ConvertScalarTo<T>((x * u) - (y * v) + a);
      expected[i + 1] = ConvertScalarTo<T>((x * v) + (y * u) + b);
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulComplexAdd(v1, v2, v3));
#else
    (void)d;
#endif  // HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllMulComplexAdd() {
  ForAllTypes(ForShrinkableVectors<TestMulComplexAdd>());
}

struct TestMaskedMulComplexOr {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const Vec<D> v1 = Iota(d, 2);
    const Vec<D> v2 = Iota(d, 5);
    const Vec<D> v3 = Iota(d, 6);

    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    auto bool_lanes = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    for (size_t i = 0; i < N; i += 2) {
      // expected = (x + iy)(u + iv)
      auto x = ConvertScalarTo<T>(i + 2);
      auto y = ConvertScalarTo<T>(i + 2 + 1);
      auto u = ConvertScalarTo<T>(i + 5);
      auto v = ConvertScalarTo<T>(i + 5 + 1);
      auto a = ConvertScalarTo<T>(i + 6);
      auto b = ConvertScalarTo<T>(i + 6 + 1);
      // Alternate between masking the real and imaginary lanes
      if ((i % 4) == 0) {
        bool_lanes[i + 0] = ConvertScalarTo<T>(1);
        expected[i + 0] = ConvertScalarTo<T>((x * u) - (y * v));
        bool_lanes[i + 1] = ConvertScalarTo<T>(0);
        expected[i + 1] = ConvertScalarTo<T>(b);
      } else {
        bool_lanes[i + 0] = ConvertScalarTo<T>(0);
        expected[i + 0] = ConvertScalarTo<T>(a);
        bool_lanes[i + 1] = ConvertScalarTo<T>(1);
        expected[i + 1] = ConvertScalarTo<T>((x * v) + (y * u));
      }
    }

    const auto mask_i = Load(d, bool_lanes.get());
    const Mask<D> mask = Gt(mask_i, Zero(d));

    HWY_ASSERT_VEC_EQ(d, expected.get(), MaskedMulComplexOr(v3, mask, v1, v2));
#else
    (void)d;
#endif  // HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllMaskedMulComplexOr() {
  ForAllTypes(ForShrinkableVectors<TestMaskedMulComplexOr>());
}

struct TestMulComplexConj {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const Vec<D> v1 = Iota(d, 2);
    const Vec<D> v2 = Iota(d, 5);

    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    for (size_t i = 0; i < N; i += 2) {
      // expected = (x + iy)(u - iv)
      auto x = ConvertScalarTo<T>(i + 2);
      auto y = ConvertScalarTo<T>(i + 2 + 1);
      auto u = ConvertScalarTo<T>(i + 5);
      auto v = ConvertScalarTo<T>(i + 5 + 1);
      expected[i + 0] = ConvertScalarTo<T>((x * u) + (y * v));
      expected[i + 1] = ConvertScalarTo<T>((y * u) - (x * v));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulComplexConj(v1, v2));
#else
    (void)d;
#endif  // HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllMulComplexConj() {
  ForAllTypes(ForShrinkableVectors<TestMulComplexConj>());
}

struct TestMulComplexConjAdd {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const Vec<D> v1 = Iota(d, 2);
    const Vec<D> v2 = Iota(d, 5);
    const Vec<D> v3 = Iota(d, 6);

    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    for (size_t i = 0; i < N; i += 2) {
      // expected = (x + iy)(u - iv) + a + ib
      auto x = ConvertScalarTo<T>(i + 2);
      auto y = ConvertScalarTo<T>(i + 2 + 1);
      auto u = ConvertScalarTo<T>(i + 5);
      auto v = ConvertScalarTo<T>(i + 5 + 1);
      auto a = ConvertScalarTo<T>(i + 6);
      auto b = ConvertScalarTo<T>(i + 6 + 1);
      expected[i + 0] = ConvertScalarTo<T>((a + (u * x)) + (v * y));
      expected[i + 1] = ConvertScalarTo<T>((b + (u * y)) - (v * x));
    }
    HWY_ASSERT_VEC_EQ(d, expected.get(), MulComplexConjAdd(v1, v2, v3));
#else
    (void)d;
#endif  // HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllMulComplexConjAdd() {
  ForAllTypes(ForShrinkableVectors<TestMulComplexConjAdd>());
}

struct TestMaskedMulComplexConj {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const Vec<D> v1 = Iota(d, 2);
    const Vec<D> v2 = Iota(d, 5);

    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    auto bool_lanes = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    for (size_t i = 0; i < N; i += 2) {
      // expected = (x + iy)(u - iv)
      auto x = ConvertScalarTo<T>(i + 2);
      auto y = ConvertScalarTo<T>(i + 2 + 1);
      auto u = ConvertScalarTo<T>(i + 5);
      auto v = ConvertScalarTo<T>(i + 5 + 1);
      // Alternate between masking the real and imaginary lanes
      if ((i % 4) == 0) {
        bool_lanes[i + 0] = ConvertScalarTo<T>(1);
        expected[i + 0] = ConvertScalarTo<T>((x * u) + (y * v));
        bool_lanes[i + 1] = ConvertScalarTo<T>(0);
        expected[i + 1] = ConvertScalarTo<T>(0);
      } else {
        bool_lanes[i + 0] = ConvertScalarTo<T>(0);
        expected[i + 0] = ConvertScalarTo<T>(0);
        bool_lanes[i + 1] = ConvertScalarTo<T>(1);
        expected[i + 1] = ConvertScalarTo<T>((y * u) - (x * v));
      }
    }

    const auto mask_i = Load(d, bool_lanes.get());
    const Mask<D> mask = Gt(mask_i, Zero(d));

    HWY_ASSERT_VEC_EQ(d, expected.get(), MaskedMulComplexConj(mask, v1, v2));
#else
    (void)d;
#endif  // HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllMaskedMulComplexConj() {
  ForAllTypes(ForShrinkableVectors<TestMaskedMulComplexConj>());
}

struct TestMaskedMulComplexConjAdd {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
#if HWY_TARGET != HWY_SCALAR
    const Vec<D> v1 = Iota(d, 2);
    const Vec<D> v2 = Iota(d, 5);
    const Vec<D> v3 = Iota(d, 6);

    const size_t N = Lanes(d);
    auto expected = AllocateAligned<T>(N);
    auto bool_lanes = AllocateAligned<T>(N);
    HWY_ASSERT(expected);

    for (size_t i = 0; i < N; i += 2) {
      // expected = (x + iy)(u - iv) + a + ib
      auto x = ConvertScalarTo<T>(i + 2);
      auto y = ConvertScalarTo<T>(i + 2 + 1);
      auto u = ConvertScalarTo<T>(i + 5);
      auto v = ConvertScalarTo<T>(i + 5 + 1);
      auto a = ConvertScalarTo<T>(i + 6);
      auto b = ConvertScalarTo<T>(i + 6 + 1);
      // Alternate between masking the real and imaginary lanes
      if ((i % 4) == 2) {
        bool_lanes[i + 0] = ConvertScalarTo<T>(1);
        expected[i + 0] = ConvertScalarTo<T>((a + (u * x)) + (v * y));
        bool_lanes[i + 1] = ConvertScalarTo<T>(0);
        expected[i + 1] = ConvertScalarTo<T>(0);
      } else {
        bool_lanes[i + 0] = ConvertScalarTo<T>(0);
        expected[i + 0] = ConvertScalarTo<T>(0);
        bool_lanes[i + 1] = ConvertScalarTo<T>(1);
        expected[i + 1] = ConvertScalarTo<T>((b + (u * y)) - (v * x));
      }
    }

    const auto mask_i = Load(d, bool_lanes.get());
    const Mask<D> mask = Gt(mask_i, Zero(d));

    HWY_ASSERT_VEC_EQ(d, expected.get(),
                      MaskedMulComplexConjAdd(mask, v1, v2, v3));
#else
    (void)d;
#endif  // HWY_TARGET != HWY_SCALAR
  }
};

HWY_NOINLINE void TestAllMaskedMulComplexConjAdd() {
  ForAllTypes(ForShrinkableVectors<TestMaskedMulComplexConjAdd>());
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE

namespace hwy {
HWY_BEFORE_TEST(HwyComplexTest);
HWY_EXPORT_AND_TEST_P(HwyComplexTest, TestAllComplexConj);

HWY_EXPORT_AND_TEST_P(HwyComplexTest, TestAllMulComplex);
HWY_EXPORT_AND_TEST_P(HwyComplexTest, TestAllMulComplexAdd);
HWY_EXPORT_AND_TEST_P(HwyComplexTest, TestAllMaskedMulComplexOr);

HWY_EXPORT_AND_TEST_P(HwyComplexTest, TestAllMulComplexConj);
HWY_EXPORT_AND_TEST_P(HwyComplexTest, TestAllMulComplexConjAdd);
HWY_EXPORT_AND_TEST_P(HwyComplexTest, TestAllMaskedMulComplexConj);
HWY_EXPORT_AND_TEST_P(HwyComplexTest, TestAllMaskedMulComplexConjAdd);
HWY_AFTER_TEST();
}  // namespace hwy
HWY_TEST_MAIN();
#endif
