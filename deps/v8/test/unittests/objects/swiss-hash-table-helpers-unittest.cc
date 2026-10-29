// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/swiss-hash-table-helpers.h"

#include <vector>

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace v8 {
namespace internal {
namespace swiss_table {

template <typename T>
class SwissTableGroupTest : public testing::Test {};

using GroupTypes = testing::Types<
#if V8_SWISS_TABLE_HAVE_SSE2_HOST
    GroupSse2Impl,
#endif
    GroupSse2Polyfill, GroupPortableImpl>;
TYPED_TEST_SUITE(SwissTableGroupTest, GroupTypes);

// Tests imported from Abseil's raw_hash_set_test.cc, modified to be
// parameterized.

TYPED_TEST(SwissTableGroupTest, EmptyGroup) {
  const ctrl_t kEmptyGroup[16] = {
      kSentinel, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty,
      kEmpty,    kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty, kEmpty,
  };
  for (h2_t h = 0; h != 128; ++h) EXPECT_FALSE(TypeParam{kEmptyGroup}.Match(h));
}

TYPED_TEST(SwissTableGroupTest, Match) {
  if (TypeParam::kWidth == 16) {
    ctrl_t group[] = {kEmpty, 1, kDeleted, 3, kEmpty, 5, kSentinel, 7,
                      7,      5, 3,        1, 1,      1, 1,         1};
    EXPECT_THAT(TypeParam{group}.Match(0), ElementsAre());
    EXPECT_THAT(TypeParam{group}.Match(1), ElementsAre(1, 11, 12, 13, 14, 15));
    EXPECT_THAT(TypeParam{group}.Match(3), ElementsAre(3, 10));
    EXPECT_THAT(TypeParam{group}.Match(5), ElementsAre(5, 9));
    EXPECT_THAT(TypeParam{group}.Match(7), ElementsAre(7, 8));
  } else if (TypeParam::kWidth == 8) {
    ctrl_t group[] = {kEmpty, 1, 2, kDeleted, 2, 1, kSentinel, 1};
    EXPECT_THAT(TypeParam{group}.Match(0), ElementsAre());
    EXPECT_THAT(TypeParam{group}.Match(1), ElementsAre(1, 5, 7));
    EXPECT_THAT(TypeParam{group}.Match(2), ElementsAre(2, 4));
  } else {
    FAIL() << "No test coverage for kWidth==" << TypeParam::kWidth;
  }
}

TYPED_TEST(SwissTableGroupTest, MatchEmpty) {
  if (TypeParam::kWidth == 16) {
    ctrl_t group[] = {kEmpty, 1, kDeleted, 3, kEmpty, 5, kSentinel, 7,
                      7,      5, 3,        1, 1,      1, 1,         1};
    EXPECT_THAT(TypeParam{group}.MatchEmpty(), ElementsAre(0, 4));
  } else if (TypeParam::kWidth == 8) {
    ctrl_t group[] = {kEmpty, 1, 2, kDeleted, 2, 1, kSentinel, 1};
    EXPECT_THAT(TypeParam{group}.MatchEmpty(), ElementsAre(0));
  } else {
    FAIL() << "No test coverage for kWidth==" << TypeParam::kWidth;
  }
}

}  // namespace swiss_table
}  // namespace internal
}  // namespace v8
