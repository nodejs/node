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

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/compare_128_test.cc"
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

struct TestLt128 {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using V = Vec<D>;
    const V v00 = Zero(d);
    const V v01 = Make128(d, 0, 1);
    const V v10 = Make128(d, 1, 0);
    const V v11 = Add(v01, v10);

    const auto mask_false = MaskFalse(d);
    const auto mask_true = MaskTrue(d);

    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, v00, v00));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, v01, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, v10, v10));

    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, v00, v01));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, v01, v10));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, v01, v11));

    // Reversed order
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, v01, v00));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, v10, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, v11, v01));

    // Also check 128-bit blocks are independent
    const V iota = Iota(d, 1);
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, iota, Add(iota, v01)));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, iota, Add(iota, v10)));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, Add(iota, v01), iota));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, Add(iota, v10), iota));

    // Max value
    const V vm = Make128(d, LimitsMax<T>(), LimitsMax<T>());
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, vm, vm));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, vm, v00));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, vm, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, vm, v10));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128(d, vm, v11));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, v00, vm));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, v01, vm));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, v10, vm));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128(d, v11, vm));
  }
};

HWY_NOINLINE void TestAllLt128() { ForGEVectors<128, TestLt128>()(uint64_t()); }

struct TestLt128Upper {
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using V = Vec<D>;
    const V v00 = Zero(d);
    const V v01 = Make128(d, 0, 1);
    const V v10 = Make128(d, 1, 0);
    const V v11 = Add(v01, v10);

    const auto mask_false = MaskFalse(d);
    const auto mask_true = MaskTrue(d);

    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128Upper(d, v00, v00));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128Upper(d, v01, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128Upper(d, v10, v10));

    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128Upper(d, v00, v01));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128Upper(d, v01, v10));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128Upper(d, v01, v11));

    // Reversed order
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128Upper(d, v01, v00));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128Upper(d, v10, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128Upper(d, v11, v01));

    // Also check 128-bit blocks are independent
    const V iota = Iota(d, 1);
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128Upper(d, iota, Add(iota, v01)));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128Upper(d, iota, Add(iota, v10)));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128Upper(d, Add(iota, v01), iota));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128Upper(d, Add(iota, v10), iota));

    // Max value
    const V vm = Make128(d, LimitsMax<T>(), LimitsMax<T>());
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128Upper(d, vm, vm));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128Upper(d, vm, v00));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128Upper(d, vm, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128Upper(d, vm, v10));
    HWY_ASSERT_MASK_EQ(d, mask_false, Lt128Upper(d, vm, v11));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128Upper(d, v00, vm));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128Upper(d, v01, vm));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128Upper(d, v10, vm));
    HWY_ASSERT_MASK_EQ(d, mask_true, Lt128Upper(d, v11, vm));
  }
};

HWY_NOINLINE void TestAllLt128Upper() {
  ForGEVectors<128, TestLt128Upper>()(uint64_t());
}

struct TestEq128 {  // Also Ne128
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using V = Vec<D>;
    const V v00 = Zero(d);
    const V v01 = Make128(d, 0, 1);
    const V v10 = Make128(d, 1, 0);
    const V v11 = Add(v01, v10);

    const auto mask_false = MaskFalse(d);
    const auto mask_true = MaskTrue(d);

    HWY_ASSERT_MASK_EQ(d, mask_true, Eq128(d, v00, v00));
    HWY_ASSERT_MASK_EQ(d, mask_true, Eq128(d, v01, v01));
    HWY_ASSERT_MASK_EQ(d, mask_true, Eq128(d, v10, v10));
    HWY_ASSERT_MASK_EQ(d, mask_false, Ne128(d, v00, v00));
    HWY_ASSERT_MASK_EQ(d, mask_false, Ne128(d, v01, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Ne128(d, v10, v10));

    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, v00, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, v01, v10));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, v01, v11));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, v00, v01));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, v01, v10));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, v01, v11));

    // Reversed order
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, v01, v00));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, v10, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, v11, v01));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, v01, v00));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, v10, v01));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, v11, v01));

    // Also check 128-bit blocks are independent
    const V iota = Iota(d, 1);
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, iota, Add(iota, v01)));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, iota, Add(iota, v10)));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, Add(iota, v01), iota));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, Add(iota, v10), iota));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, iota, Add(iota, v01)));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, iota, Add(iota, v10)));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, Add(iota, v01), iota));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, Add(iota, v10), iota));

    // Max value
    const V vm = Make128(d, LimitsMax<T>(), LimitsMax<T>());
    HWY_ASSERT_MASK_EQ(d, mask_true, Eq128(d, vm, vm));
    HWY_ASSERT_MASK_EQ(d, mask_false, Ne128(d, vm, vm));

    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, vm, v00));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, vm, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, vm, v10));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, vm, v11));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, v00, vm));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, v01, vm));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, v10, vm));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128(d, v11, vm));

    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, vm, v00));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, vm, v01));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, vm, v10));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, vm, v11));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, v00, vm));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, v01, vm));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, v10, vm));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128(d, v11, vm));
  }
};

HWY_NOINLINE void TestAllEq128() { ForGEVectors<128, TestEq128>()(uint64_t()); }

struct TestEq128Upper {  // Also Ne128Upper
  template <typename T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using V = Vec<D>;
    const V v00 = Zero(d);
    const V v01 = Make128(d, 0, 1);
    const V v10 = Make128(d, 1, 0);
    const V v11 = Add(v01, v10);

    const auto mask_false = MaskFalse(d);
    const auto mask_true = MaskTrue(d);

    HWY_ASSERT_MASK_EQ(d, mask_true, Eq128Upper(d, v00, v00));
    HWY_ASSERT_MASK_EQ(d, mask_true, Eq128Upper(d, v01, v01));
    HWY_ASSERT_MASK_EQ(d, mask_true, Eq128Upper(d, v10, v10));
    HWY_ASSERT_MASK_EQ(d, mask_false, Ne128Upper(d, v00, v00));
    HWY_ASSERT_MASK_EQ(d, mask_false, Ne128Upper(d, v01, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Ne128Upper(d, v10, v10));

    HWY_ASSERT_MASK_EQ(d, mask_true, Eq128Upper(d, v00, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Ne128Upper(d, v00, v01));

    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128Upper(d, v01, v10));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128Upper(d, v01, v11));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128Upper(d, v01, v10));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128Upper(d, v01, v11));

    // Reversed order
    HWY_ASSERT_MASK_EQ(d, mask_true, Eq128Upper(d, v01, v00));
    HWY_ASSERT_MASK_EQ(d, mask_false, Ne128Upper(d, v01, v00));

    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128Upper(d, v10, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128Upper(d, v11, v01));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128Upper(d, v10, v01));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128Upper(d, v11, v01));

    // Also check 128-bit blocks are independent
    const V iota = Iota(d, 1);
    HWY_ASSERT_MASK_EQ(d, mask_true, Eq128Upper(d, iota, Add(iota, v01)));
    HWY_ASSERT_MASK_EQ(d, mask_false, Ne128Upper(d, iota, Add(iota, v01)));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128Upper(d, iota, Add(iota, v10)));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128Upper(d, iota, Add(iota, v10)));
    HWY_ASSERT_MASK_EQ(d, mask_true, Eq128Upper(d, Add(iota, v01), iota));
    HWY_ASSERT_MASK_EQ(d, mask_false, Ne128Upper(d, Add(iota, v01), iota));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128Upper(d, Add(iota, v10), iota));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128Upper(d, Add(iota, v10), iota));

    // Max value
    const V vm = Make128(d, LimitsMax<T>(), LimitsMax<T>());
    HWY_ASSERT_MASK_EQ(d, mask_true, Eq128Upper(d, vm, vm));
    HWY_ASSERT_MASK_EQ(d, mask_false, Ne128Upper(d, vm, vm));

    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128Upper(d, vm, v00));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128Upper(d, vm, v01));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128Upper(d, vm, v10));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128Upper(d, vm, v11));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128Upper(d, v00, vm));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128Upper(d, v01, vm));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128Upper(d, v10, vm));
    HWY_ASSERT_MASK_EQ(d, mask_false, Eq128Upper(d, v11, vm));

    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128Upper(d, vm, v00));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128Upper(d, vm, v01));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128Upper(d, vm, v10));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128Upper(d, vm, v11));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128Upper(d, v00, vm));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128Upper(d, v01, vm));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128Upper(d, v10, vm));
    HWY_ASSERT_MASK_EQ(d, mask_true, Ne128Upper(d, v11, vm));
  }
};

HWY_NOINLINE void TestAllEq128Upper() {
  ForGEVectors<128, TestEq128Upper>()(uint64_t());
}

}  // namespace
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace hwy {
namespace {
HWY_BEFORE_TEST(HwyCompare128Test);
HWY_EXPORT_AND_TEST_P(HwyCompare128Test, TestAllLt128);
HWY_EXPORT_AND_TEST_P(HwyCompare128Test, TestAllLt128Upper);
HWY_EXPORT_AND_TEST_P(HwyCompare128Test, TestAllEq128);
HWY_EXPORT_AND_TEST_P(HwyCompare128Test, TestAllEq128Upper);
HWY_AFTER_TEST();
}  // namespace
}  // namespace hwy
HWY_TEST_MAIN();
#endif  // HWY_ONCE
