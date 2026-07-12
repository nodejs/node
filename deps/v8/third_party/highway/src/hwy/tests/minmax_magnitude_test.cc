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
#define HWY_TARGET_INCLUDE "tests/minmax_magnitude_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {

struct TestMinMaxMagnitude {
  template <class T>
  static constexpr MakeSigned<T> MaxPosIotaVal(hwy::FloatTag /*type_tag*/) {
    return static_cast<MakeSigned<T>>(MantissaMask<T>() + 1);
  }
  template <class T>
  static constexpr MakeSigned<T> MaxPosIotaVal(hwy::NonFloatTag /*type_tag*/) {
    return static_cast<MakeSigned<T>>(((LimitsMax<MakeSigned<T>>()) >> 1) + 1);
  }

  template <class D, typename T = TFromD<D>>
  HWY_NOINLINE static void VerifyMinMaxMagnitude(
      D d, const T* HWY_RESTRICT in1_lanes, const T* HWY_RESTRICT in2_lanes,
      const int line) {
    using TAbs = If<IsFloat<T>() || IsSpecialFloat<T>(), T, MakeUnsigned<T>>;

    const size_t N = Lanes(d);
    auto expected_min_mag = AllocateAligned<T>(N);
    auto expected_max_mag = AllocateAligned<T>(N);
    HWY_ASSERT(expected_min_mag && expected_max_mag);

    for (size_t i = 0; i < N; i++) {
      const T val1 = in1_lanes[i];
      const T val2 = in2_lanes[i];
      const T abs1 = ScalarAbs(val1);
      const T abs2 = ScalarAbs(val2);
      TAbs u1, u2;
      hwy::CopySameSize(&abs1, &u1);
      hwy::CopySameSize(&abs2, &u2);
      if (u1 < u2 || (u1 == u2 && val1 < val2)) {
        expected_min_mag[i] = val1;
        expected_max_mag[i] = val2;
      } else {
        expected_min_mag[i] = val2;
        expected_max_mag[i] = val1;
      }
    }

    const Vec<D> in1 = Load(d, in1_lanes);
    const Vec<D> in2 = Load(d, in2_lanes);
    const Vec<D> actual_min1 = MinMagnitude(in1, in2);
    const Vec<D> actual_max1 = MaxMagnitude(in1, in2);
    const Vec<D> actual_min2 = MinMagnitude(in2, in1);
    const Vec<D> actual_max2 = MaxMagnitude(in2, in1);
    AssertVecEqual(d, expected_min_mag.get(), actual_min1, __FILE__, line);
    AssertVecEqual(d, expected_min_mag.get(), actual_min2, __FILE__, line);
    AssertVecEqual(d, expected_max_mag.get(), actual_max1, __FILE__, line);
    AssertVecEqual(d, expected_max_mag.get(), actual_max2, __FILE__, line);
  }

  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using TI = MakeSigned<T>;
    using TU = MakeUnsigned<T>;
    constexpr TI kMaxPosIotaVal = MaxPosIotaVal<T>(hwy::IsFloatTag<T>());
    static_assert(kMaxPosIotaVal > 0, "kMaxPosIotaVal > 0 must be true");

    constexpr size_t kPositiveIotaMask = static_cast<size_t>(
        static_cast<TU>(kMaxPosIotaVal - 1) & (HWY_MAX_LANES_D(D) - 1));

    const size_t N = Lanes(d);
    auto in1_lanes = AllocateAligned<T>(N);
    auto in2_lanes = AllocateAligned<T>(N);
    auto in3_lanes = AllocateAligned<T>(N);
    auto in4_lanes = AllocateAligned<T>(N);
    HWY_ASSERT(in1_lanes && in2_lanes && in3_lanes && in4_lanes);

    for (size_t i = 0; i < N; i++) {
      const TI x1 = static_cast<TI>((i & kPositiveIotaMask) + 1);
      const TI x2 = static_cast<TI>(kMaxPosIotaVal - x1);
      const TI x3 = static_cast<TI>(-x1);
      const TI x4 = static_cast<TI>(-x2);

      in1_lanes[i] = ConvertScalarTo<T>(x1);
      in2_lanes[i] = ConvertScalarTo<T>(x2);
      in3_lanes[i] = ConvertScalarTo<T>(x3);
      in4_lanes[i] = ConvertScalarTo<T>(x4);
    }

    VerifyMinMaxMagnitude(d, in1_lanes.get(), in2_lanes.get(), __LINE__);
    VerifyMinMaxMagnitude(d, in1_lanes.get(), in3_lanes.get(), __LINE__);
    VerifyMinMaxMagnitude(d, in1_lanes.get(), in4_lanes.get(), __LINE__);
    VerifyMinMaxMagnitude(d, in2_lanes.get(), in3_lanes.get(), __LINE__);
    VerifyMinMaxMagnitude(d, in2_lanes.get(), in4_lanes.get(), __LINE__);
    VerifyMinMaxMagnitude(d, in3_lanes.get(), in4_lanes.get(), __LINE__);

    in2_lanes[0] = HighestValue<T>();
    in4_lanes[0] = LowestValue<T>();

    VerifyMinMaxMagnitude(d, in1_lanes.get(), in2_lanes.get(), __LINE__);
    VerifyMinMaxMagnitude(d, in1_lanes.get(), in4_lanes.get(), __LINE__);
    VerifyMinMaxMagnitude(d, in2_lanes.get(), in3_lanes.get(), __LINE__);
    VerifyMinMaxMagnitude(d, in2_lanes.get(), in4_lanes.get(), __LINE__);
    VerifyMinMaxMagnitude(d, in3_lanes.get(), in4_lanes.get(), __LINE__);
  }
};

HWY_NOINLINE void TestMinMaxMagnitudeU() {
  ForUnsignedTypes(ForPartialVectors<TestMinMaxMagnitude>());
}

HWY_NOINLINE void TestMinMaxMagnitudeI() {
  ForSignedTypes(ForPartialVectors<TestMinMaxMagnitude>());
}

HWY_NOINLINE void TestMinMaxMagnitudeF() {
  ForFloatTypes(ForPartialVectors<TestMinMaxMagnitude>());
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_BEFORE_TEST(HwyMinMaxMagnitudeTest);
HWY_EXPORT_AND_TEST_P(HwyMinMaxMagnitudeTest, TestMinMaxMagnitudeU);
HWY_EXPORT_AND_TEST_P(HwyMinMaxMagnitudeTest, TestMinMaxMagnitudeI);
HWY_EXPORT_AND_TEST_P(HwyMinMaxMagnitudeTest, TestMinMaxMagnitudeF);
HWY_AFTER_TEST();
}  // namespace
}  // namespace hwy
HWY_TEST_MAIN();
#endif  // HWY_ONCE
