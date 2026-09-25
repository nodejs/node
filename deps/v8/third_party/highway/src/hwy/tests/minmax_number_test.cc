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

// For faster build on Arm. We still test SVE, SVE2, and NEON.
#ifndef HWY_DISABLED_TARGETS
#define HWY_DISABLED_TARGETS (HWY_NEON_WITHOUT_AES | HWY_SVE_256 | HWY_SVE2_128)
#endif

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/minmax_number_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/nanobenchmark.h"  // Unpredictable1
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {

struct TestUnsignedMinMaxNumber {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const Vec<D> v0 = Zero(d);
    // Leave headroom such that v1 < v2 even after wraparound.
    const Vec<D> mod = And(Iota(d, 0), Set(d, LimitsMax<T>() >> 1));
    const Vec<D> v1 = Add(mod, Set(d, static_cast<T>(1)));
    const Vec<D> v2 = Add(mod, Set(d, static_cast<T>(2)));
    HWY_ASSERT_VEC_EQ(d, v1, MinNumber(v1, v2));
    HWY_ASSERT_VEC_EQ(d, v2, MaxNumber(v1, v2));
    HWY_ASSERT_VEC_EQ(d, v0, MinNumber(v1, v0));
    HWY_ASSERT_VEC_EQ(d, v1, MaxNumber(v1, v0));

    const Vec<D> vmin = Set(d, static_cast<T>(Unpredictable1() - 1));  // 0
    const Vec<D> vmax = Set(d, LimitsMax<T>());

    const Vec<D> actual_min1 = MinNumber(vmin, vmax);
    const Vec<D> actual_min2 = MinNumber(vmax, vmin);
#if HWY_TARGET_IS_SVE
    // lvalue and `PreventElision` work around incorrect codegen on SVE2.
    PreventElision(GetLane(actual_min1));
    PreventElision(GetLane(actual_min2));
#endif
    HWY_ASSERT_VEC_EQ(d, vmin, actual_min1);
    HWY_ASSERT_VEC_EQ(d, vmin, actual_min2);

    const Vec<D> actual_max1 = MaxNumber(vmin, vmax);
    const Vec<D> actual_max2 = MaxNumber(vmax, vmin);
    HWY_ASSERT_VEC_EQ(d, vmax, actual_max1);
    HWY_ASSERT_VEC_EQ(d, vmax, actual_max2);
  }
};

HWY_NOINLINE void TestMinMaxNumberU() {
  ForUnsignedTypes(ForPartialVectors<TestUnsignedMinMaxNumber>());
}

struct TestSignedMinMaxNumber {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    // Leave headroom such that v1 < v2 even after wraparound.
    const Vec<D> mod =
        And(Iota(d, 0), Set(d, ConvertScalarTo<T>(LimitsMax<T>() >> 1)));
    const Vec<D> v1 = Add(mod, Set(d, ConvertScalarTo<T>(1)));
    const Vec<D> v2 = Add(mod, Set(d, ConvertScalarTo<T>(2)));
    const Vec<D> v_neg = Sub(Zero(d), v1);
    HWY_ASSERT_VEC_EQ(d, v1, MinNumber(v1, v2));
    HWY_ASSERT_VEC_EQ(d, v2, MaxNumber(v1, v2));
    HWY_ASSERT_VEC_EQ(d, v_neg, MinNumber(v1, v_neg));
    HWY_ASSERT_VEC_EQ(d, v1, MaxNumber(v1, v_neg));

    const Vec<D> v0 = Set(d, static_cast<T>(Unpredictable1() - 1));
    const Vec<D> vmin = Or(v0, Set(d, LimitsMin<T>()));
    const Vec<D> vmax = Or(v0, Set(d, LimitsMax<T>()));
    HWY_ASSERT_VEC_EQ(d, vmin, MinNumber(v0, vmin));
    HWY_ASSERT_VEC_EQ(d, vmin, MinNumber(vmin, v0));
    HWY_ASSERT_VEC_EQ(d, v0, MaxNumber(v0, vmin));
    HWY_ASSERT_VEC_EQ(d, v0, MaxNumber(vmin, v0));

    const Vec<D> actual_min1 = MinNumber(vmin, vmax);
    const Vec<D> actual_min2 = MinNumber(vmax, vmin);
    HWY_ASSERT_VEC_EQ(d, vmin, actual_min1);
    HWY_ASSERT_VEC_EQ(d, vmin, actual_min2);

    HWY_ASSERT_VEC_EQ(d, vmax, MaxNumber(vmin, vmax));
    HWY_ASSERT_VEC_EQ(d, vmax, MaxNumber(vmax, vmin));
  }
};

HWY_NOINLINE void TestMinMaxNumberI() {
  ForSignedTypes(ForPartialVectors<TestSignedMinMaxNumber>());
}

struct TestFloatMinMaxNumber {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using TU = MakeUnsigned<T>;
    constexpr TU kQNaNBits =
        static_cast<TU>(ExponentMask<T>() | (ExponentMask<T>() >> 1));

    const RebindToUnsigned<D> du;
    const Vec<D> vnan = BitCast(d, Set(du, kQNaNBits));

    const Vec<D> v1 = Iota(d, Unpredictable1());
    const Vec<D> v2 = Iota(d, Unpredictable1() + 1);
    const Vec<D> v3 = Or(v1, vnan);
    const Vec<D> v4 = Or(v2, vnan);
    const Vec<D> v_neg = Iota(d, -ConvertScalarTo<T>(Lanes(d)));
    HWY_ASSERT_VEC_EQ(d, v1, MinNumber(v1, v2));
    HWY_ASSERT_VEC_EQ(d, v2, MaxNumber(v1, v2));
    HWY_ASSERT_VEC_EQ(d, v_neg, MinNumber(v1, v_neg));
    HWY_ASSERT_VEC_EQ(d, v1, MaxNumber(v1, v_neg));

    HWY_ASSERT_VEC_EQ(d, v1, MinNumber(v1, v4));
    HWY_ASSERT_VEC_EQ(d, v2, MinNumber(v2, v3));
    HWY_ASSERT_VEC_EQ(d, v2, MinNumber(v3, v2));
    HWY_ASSERT_VEC_EQ(d, v1, MinNumber(v4, v1));

    HWY_ASSERT_VEC_EQ(d, v1, MaxNumber(v1, v4));
    HWY_ASSERT_VEC_EQ(d, v2, MaxNumber(v2, v3));
    HWY_ASSERT_VEC_EQ(d, v2, MaxNumber(v3, v2));
    HWY_ASSERT_VEC_EQ(d, v1, MaxNumber(v4, v1));

    const Vec<D> v0 = Zero(d);
    const Vec<D> vmin = Set(d, ConvertScalarTo<T>(-1E30));
    const Vec<D> vmax = Set(d, ConvertScalarTo<T>(1E30));
    HWY_ASSERT_VEC_EQ(d, vmin, MinNumber(v0, vmin));
    HWY_ASSERT_VEC_EQ(d, vmin, MinNumber(vmin, v0));
    HWY_ASSERT_VEC_EQ(d, v0, MaxNumber(v0, vmin));
    HWY_ASSERT_VEC_EQ(d, v0, MaxNumber(vmin, v0));

    HWY_ASSERT_VEC_EQ(d, vmin, MinNumber(vmin, vmax));
    HWY_ASSERT_VEC_EQ(d, vmin, MinNumber(vmax, vmin));

    HWY_ASSERT_VEC_EQ(d, vmax, MaxNumber(vmin, vmax));
    HWY_ASSERT_VEC_EQ(d, vmax, MaxNumber(vmax, vmin));
  }
};

HWY_NOINLINE void TestMinMaxNumberF() {
  ForFloatTypes(ForPartialVectors<TestFloatMinMaxNumber>());
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_BEFORE_TEST(HwyMinMaxNumberTest);
HWY_EXPORT_AND_TEST_P(HwyMinMaxNumberTest, TestMinMaxNumberU);
HWY_EXPORT_AND_TEST_P(HwyMinMaxNumberTest, TestMinMaxNumberI);
HWY_EXPORT_AND_TEST_P(HwyMinMaxNumberTest, TestMinMaxNumberF);
HWY_AFTER_TEST();
}  // namespace
}  // namespace hwy
HWY_TEST_MAIN();
#endif  // HWY_ONCE
