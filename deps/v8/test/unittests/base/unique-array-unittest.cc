// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/unique-array.h"

#include <algorithm>
#include <span>

#include "src/base/vector.h"
#include "testing/gmock-support.h"

namespace v8 {
namespace base {

TEST(UniqueArrayTest, Equals) {
  auto int_arr = base::UniqueArray<int>::New(4);
  EXPECT_EQ(4u, int_arr.size());
  auto find_non_zero = [](int i) { return i != 0; };
  EXPECT_EQ(int_arr.end(),
            std::find_if(int_arr.begin(), int_arr.end(), find_non_zero));

  constexpr int kInit[] = {4, 11, 3};
  auto init_arr1 = base::UniqueCopyOf(kInit);
  // Note: `const int` should also work: We initialize the unique array, but
  // afterwards it's non-modifiable.
  auto init_arr2 = base::UniqueCopyOf(base::ArrayVector(kInit));
  EXPECT_EQ(init_arr1.as_vector(), base::ArrayVector(kInit));
  EXPECT_EQ(init_arr1.as_vector(), init_arr2.as_vector());
}

TEST(UniqueArrayTest, FactoriesAndViews) {
  auto fill_arr = base::UniqueArray<int>::New(3, 42);
  EXPECT_EQ(3u, fill_arr.size());
  EXPECT_THAT(fill_arr.as_span(), testing::ElementsAre(42, 42, 42));
  EXPECT_THAT(fill_arr.as_vector(), testing::ElementsAre(42, 42, 42));

  auto const_fill_arr = base::UniqueArray<const int>::New(2, 7);
  EXPECT_EQ(2u, const_fill_arr.size());
  EXPECT_THAT(const_fill_arr.as_span(), testing::ElementsAre(7, 7));

  constexpr int kInit[] = {4, 11, 3};
  auto copied_ptr = base::UniqueArray<int>::CopiedFrom(kInit, 3);
  EXPECT_THAT(copied_ptr.as_span(), testing::ElementsAreArray(kInit));
  EXPECT_EQ(copied_ptr.as_vector(), base::ArrayVector(kInit));

  auto copied_span = base::UniqueArray<int>::CopiedFrom(std::span(kInit));
  EXPECT_THAT(copied_span.as_span(), testing::ElementsAreArray(kInit));
  EXPECT_EQ(copied_span.as_vector(), base::ArrayVector(kInit));

  auto const_copied_span =
      base::UniqueArray<const int>::CopiedFrom(std::span(kInit));
  EXPECT_THAT(const_copied_span.as_span(), testing::ElementsAreArray(kInit));
  EXPECT_EQ(const_copied_span.as_vector(), base::ArrayVector(kInit));

  auto copy_of_ptr = base::UniqueCopyOf(kInit, 2);
  EXPECT_THAT(copy_of_ptr.as_span(), testing::ElementsAre(4, 11));
  EXPECT_THAT(copy_of_ptr.as_vector(), testing::ElementsAre(4, 11));
}

TEST(UniqueArrayTest, MoveConstructionAndAssignment) {
  constexpr int kValues[] = {4, 11, 3};
  auto int_arr = base::UniqueCopyOf(kValues);
  EXPECT_EQ(3u, int_arr.size());

  auto move_constructed_arr = std::move(int_arr);
  EXPECT_EQ(move_constructed_arr.as_vector(), base::ArrayVector(kValues));

  auto move_assigned_to_empty = base::UniqueArray<int>{};
  move_assigned_to_empty = std::move(move_constructed_arr);
  EXPECT_EQ(move_assigned_to_empty.as_vector(), base::ArrayVector(kValues));

  auto move_assigned_to_non_empty = base::UniqueArray<int>::New(2);
  move_assigned_to_non_empty = std::move(move_assigned_to_empty);
  EXPECT_EQ(move_assigned_to_non_empty.as_vector(), base::ArrayVector(kValues));

  // All but the last array must be empty (length 0, nullptr data).
  EXPECT_TRUE(int_arr.empty());
  EXPECT_TRUE(int_arr.begin() == nullptr);
  EXPECT_TRUE(move_constructed_arr.empty());
  EXPECT_TRUE(move_constructed_arr.begin() == nullptr);
  EXPECT_TRUE(move_assigned_to_empty.empty());
  EXPECT_TRUE(move_assigned_to_empty.begin() == nullptr);
}

}  // namespace base
}  // namespace v8
