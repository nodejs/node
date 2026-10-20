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

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "tests/table_test.cc"
#include "hwy/foreach_target.h"  // IWYU pragma: keep
#include "hwy/highway.h"
#include "hwy/nanobenchmark.h"
#include "hwy/tests/test_util-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace {

struct TestTableLookupLanes {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const RebindToSigned<D> di;
    using TI = TFromD<decltype(di)>;
#if HWY_TARGET != HWY_SCALAR
    const size_t N = Lanes(d);
    auto idx = AllocateAligned<TI>(N);
    auto expected = AllocateAligned<T>(N);
    HWY_ASSERT(idx && expected);
    ZeroBytes(idx.get(), N * sizeof(TI));
    const Vec<D> v = Iota(d, static_cast<T>(Unpredictable1()));

    if (N <= 8) {  // Test all permutations
      for (size_t i0 = 0; i0 < N; ++i0) {
        idx[0] =
            static_cast<TI>(i0 + static_cast<size_t>(Unpredictable1() - 1));

        for (size_t i1 = 0; i1 < N; ++i1) {
          if (N >= 2) idx[1] = static_cast<TI>(i1);
          for (size_t i2 = 0; i2 < N; ++i2) {
            if (N >= 4) idx[2] = static_cast<TI>(i2);
            for (size_t i3 = 0; i3 < N; ++i3) {
              if (N >= 4) idx[3] = static_cast<TI>(i3);

              for (size_t i = 0; i < N; ++i) {
                expected[i] = ConvertScalarTo<T>(idx[i] + 1);  // == v[idx[i]]
              }

              const auto opaque1 = IndicesFromVec(d, Load(di, idx.get()));
              const Vec<D> actual1 = TableLookupLanes(v, opaque1);
              HWY_ASSERT_VEC_EQ(d, expected.get(), actual1);

              const auto opaque2 = SetTableIndices(d, idx.get());
              const Vec<D> actual2 = TableLookupLanes(v, opaque2);
              HWY_ASSERT_VEC_EQ(d, expected.get(), actual2);
            }
          }
        }
      }
    } else {
      // Too many permutations to test exhaustively; choose one with repeated
      // and cross-block indices and ensure indices do not exceed #lanes.
      // For larger vectors, upper lanes will be zero.
      HWY_ALIGN TI idx_source[16] = {1,  3,  2,  2,  8, 1, 7, 6,
                                     15, 14, 14, 15, 4, 9, 8, 5};
      for (size_t i = 0; i < N; ++i) {
        idx[i] = (i < 16)
                     ? static_cast<TI>(idx_source[i] +
                                       static_cast<TI>(Unpredictable1() - 1))
                     : 0;
        // Avoid undefined results / asan error for scalar by capping indices.
        if (idx[i] >= static_cast<TI>(N)) {
          idx[i] = static_cast<TI>(N - 1);
        }
        expected[i] = ConvertScalarTo<T>(idx[i] + 1);  // == v[idx[i]]
      }

      const auto opaque1 = IndicesFromVec(d, Load(di, idx.get()));
      Vec<D> actual1 = TableLookupLanes(v, opaque1);
      HWY_ASSERT_VEC_EQ(d, expected.get(), actual1);

      const auto opaque2 = SetTableIndices(d, idx.get());
      const Vec<D> actual2 = TableLookupLanes(v, opaque2);
      HWY_ASSERT_VEC_EQ(d, expected.get(), actual2);
    }
#else
    const TI index = 0;
    const Vec<D> v = Set(d, static_cast<T>(Unpredictable1()));
    const auto opaque1 = SetTableIndices(d, &index);
    HWY_ASSERT_VEC_EQ(d, v, TableLookupLanes(v, opaque1));
    const auto opaque2 = IndicesFromVec(d, Zero(di));
    HWY_ASSERT_VEC_EQ(d, v, TableLookupLanes(v, opaque2));
#endif
  }
};

HWY_NOINLINE void TestAllTableLookupLanes() {
  ForAllTypes(ForPartialVectors<TestTableLookupLanes>());
}

struct TestTwoTablesLookupLanes {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    const RebindToUnsigned<D> du;
    using TU = TFromD<decltype(du)>;

    const size_t N = Lanes(d);
    const size_t twiceN = N * 2;
    auto idx = AllocateAligned<TU>(twiceN);
    auto expected = AllocateAligned<T>(twiceN);
    HWY_ASSERT(idx && expected);
    ZeroBytes(idx.get(), twiceN * sizeof(TU));
    const Vec<D> a = Iota(d, static_cast<T>(Unpredictable1()));
    const Vec<D> b = Add(a, Set(d, ConvertScalarTo<T>(N)));

    if (twiceN <= 8) {  // Test all permutations
      for (size_t i0 = 0; i0 < twiceN; ++i0) {
        idx[0] = static_cast<TU>(i0);

        for (size_t i1 = 0; i1 < twiceN; ++i1) {
          if (twiceN >= 2) idx[1] = static_cast<TU>(i1);
          for (size_t i2 = 0; i2 < twiceN; ++i2) {
            if (twiceN >= 4) idx[2] = static_cast<TU>(i2);
            for (size_t i3 = 0; i3 < twiceN; ++i3) {
              if (twiceN >= 4) idx[3] = static_cast<TU>(i3);

              for (size_t i = 0; i < twiceN; ++i) {
                expected[i] = ConvertScalarTo<T>(idx[i] + 1);  // == v[idx[i]]
              }

              const auto opaque1_a = IndicesFromVec(d, Load(du, idx.get()));
              const auto opaque1_b = IndicesFromVec(d, Load(du, idx.get() + N));
              const Vec<D> actual1_a = TwoTablesLookupLanes(d, a, b, opaque1_a);
              const Vec<D> actual1_b = TwoTablesLookupLanes(d, a, b, opaque1_b);
              HWY_ASSERT_VEC_EQ(d, expected.get(), actual1_a);
              HWY_ASSERT_VEC_EQ(d, expected.get() + N, actual1_b);

              const auto opaque2_a = SetTableIndices(d, idx.get());
              const auto opaque2_b = SetTableIndices(d, idx.get() + N);
              const Vec<D> actual2_a = TwoTablesLookupLanes(d, a, b, opaque2_a);
              const Vec<D> actual2_b = TwoTablesLookupLanes(d, a, b, opaque2_b);
              HWY_ASSERT_VEC_EQ(d, expected.get(), actual2_a);
              HWY_ASSERT_VEC_EQ(d, expected.get() + N, actual2_b);
            }
          }
        }
      }
    } else {
      constexpr size_t kLanesPerBlock = 16 / sizeof(T);
      constexpr size_t kMaxBlockIdx = static_cast<size_t>(LimitsMax<TU>()) >> 1;
      static_assert(kMaxBlockIdx > 0, "kMaxBlockIdx > 0 must be true");

      const size_t num_of_blocks_per_vect = HWY_MAX(N / kLanesPerBlock, 1);
      const size_t num_of_blocks_to_check =
          HWY_MIN(num_of_blocks_per_vect * 2, kMaxBlockIdx);

      for (size_t i = 0; i < num_of_blocks_to_check; i++) {
        // Too many permutations to test exhaustively; choose one with repeated
        // and cross-block indices and ensure indices do not exceed #lanes.
        // For larger vectors, upper lanes will be zero.
        HWY_ALIGN TU idx_source[16] = {1,  3,  2,  2,  8, 1, 7, 6,
                                       15, 14, 14, 15, 4, 9, 8, 5};
        for (size_t j = 0; j < twiceN; ++j) {
          idx[j] = static_cast<TU>((i * kLanesPerBlock + idx_source[j & 15] +
                                    (j & static_cast<size_t>(-16))) &
                                   (twiceN - 1));
          expected[j] = ConvertScalarTo<T>(idx[j] + 1);  // == v[idx[j]]
        }

        const auto opaque1_a = IndicesFromVec(d, Load(du, idx.get()));
        const auto opaque1_b = IndicesFromVec(d, Load(du, idx.get() + N));
        const Vec<D> actual1_a = TwoTablesLookupLanes(d, a, b, opaque1_a);
        const Vec<D> actual1_b = TwoTablesLookupLanes(d, a, b, opaque1_b);
        HWY_ASSERT_VEC_EQ(d, expected.get(), actual1_a);
        HWY_ASSERT_VEC_EQ(d, expected.get() + N, actual1_b);

        const auto opaque2_a = SetTableIndices(d, idx.get());
        const auto opaque2_b = SetTableIndices(d, idx.get() + N);
        const Vec<D> actual2_a = TwoTablesLookupLanes(d, a, b, opaque2_a);
        const Vec<D> actual2_b = TwoTablesLookupLanes(d, a, b, opaque2_b);
        HWY_ASSERT_VEC_EQ(d, expected.get(), actual2_a);
        HWY_ASSERT_VEC_EQ(d, expected.get() + N, actual2_b);
      }
    }
  }
};

HWY_NOINLINE void TestAllTwoTablesLookupLanes() {
  ForAllTypes(ForPartialVectors<TestTwoTablesLookupLanes>());
}

struct TestLookup8 {
  template <class T, class D>
  HWY_NOINLINE void operator()(T /*unused*/, D d) {
    using V = Vec<D>;
    const RebindToUnsigned<D> du;
    using TU = TFromD<decltype(du)>;

    const size_t N = Lanes(d);
    if (N < 4) return;

    const size_t padded_N = HWY_MAX(N, 8);
    auto tbl = AllocateAligned<T>(padded_N);
    auto idx = AllocateAligned<TU>(padded_N);
    auto expected = AllocateAligned<T>(padded_N);
    HWY_ASSERT(tbl && idx && expected);
    ZeroBytes(idx.get(), padded_N * sizeof(TU));

    for (size_t i = 0; i < padded_N; ++i) {
      tbl[i] = ConvertScalarTo<T>(i + static_cast<size_t>(Unpredictable1()));
    }

    if (N < 8) {  // Test all permutations (max index is 7)
      for (size_t i0 = 0; i0 < 8; ++i0) {
        idx[0] = static_cast<TU>(i0);

        for (size_t i1 = 0; i1 < 8; ++i1) {
          idx[1] = static_cast<TU>(i1);
          for (size_t i2 = 0; i2 < 8; ++i2) {
            idx[2] = static_cast<TU>(i2);
            for (size_t i3 = 0; i3 < 8; ++i3) {
              idx[3] = static_cast<TU>(i3);

              for (size_t i = 0; i < padded_N; ++i) {
                expected[i] = ConvertScalarTo<T>(idx[i] + 1);
              }

              const V actual = Lookup8(d, tbl.get(), Load(du, idx.get()));
              HWY_ASSERT_VEC_EQ(d, expected.get(), actual);
            }
          }
        }
      }
    } else {
      // Too many permutations to test exhaustively; choose one with repeated
      // indices. Note that the ops checks that indices do not exceed 8.
      // For larger vectors, upper lanes will be zero.
      HWY_ALIGN TU idx_source[16] = {1, 3, 2, 2, 3, 7, 6, 5,
                                     0, 4, 0, 4, 5, 7, 6, 6};
      for (size_t j = 0; j < padded_N; ++j) {
        idx[j] = static_cast<TU>(idx_source[j & 15]);
        expected[j] = ConvertScalarTo<T>(idx[j] + 1);  // == v[idx[j]]
      }

      const V actual = Lookup8(d, tbl.get(), Load(du, idx.get()));
      HWY_ASSERT_VEC_EQ(d, expected.get(), actual);
    }
  }
};

HWY_NOINLINE void TestAllLookup8() {
  ForUIF16(ForGE128Vectors<TestLookup8>());
  ForUIF32(ForGE128Vectors<TestLookup8>());
  // For Lookup8 with 64-bit lanes, we require a scalable target (fine because
  // the test has a runtime check) or 256 bit vectors: 8 elements across two
  // vectors, hence at least four 8-byte uint64_t per vector.
#if HWY_HAVE_SCALABLE || HWY_MIN_BYTES / 8 >= 4
  ForUIF64(ForGE128Vectors<TestLookup8>());
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
HWY_BEFORE_TEST(HwyTableTest);
HWY_EXPORT_AND_TEST_P(HwyTableTest, TestAllTableLookupLanes);
HWY_EXPORT_AND_TEST_P(HwyTableTest, TestAllTwoTablesLookupLanes);
HWY_EXPORT_AND_TEST_P(HwyTableTest, TestAllLookup8);
HWY_AFTER_TEST();
}  // namespace
}  // namespace hwy
HWY_TEST_MAIN();
#endif  // HWY_ONCE
