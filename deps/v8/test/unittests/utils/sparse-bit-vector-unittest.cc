// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/sparse-bit-vector.h"

#include <vector>

#include "test/unittests/test-utils.h"
#include "testing/gmock-support.h"
#include "testing/gtest-support.h"

namespace v8::internal {

using ::testing::ElementsAre;

namespace {
class SparseBitVectorBuilder {
 public:
  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(SparseBitVectorBuilder);

  explicit SparseBitVectorBuilder(Zone* zone) : vector_(zone) {}

  template <typename... Ts>
  SparseBitVectorBuilder& Add(Ts... values) {
    (vector_.Add(values), ...);
    return *this;
  }

  template <typename... Ts>
  SparseBitVectorBuilder& Remove(Ts... values) {
    (vector_.Remove(values), ...);
    return *this;
  }

  std::vector<int> ToStdVector() const {
    return std::vector<int>(vector_.begin(), vector_.end());
  }

  SparseBitVector get() { return std::move(vector_); }

 private:
  SparseBitVector vector_;
};
}  // namespace

class SparseBitVectorTest : public TestWithZone {
 public:
  SparseBitVectorBuilder B() { return SparseBitVectorBuilder{zone()}; }

  template <typename... Ts>
  SparseBitVector Make(Ts... values) {
    return B().Add(values...).get();
  }

  template <typename... Ts>
  std::vector<int> VectorOf(Ts... values) {
    return B().Add(values...).ToStdVector();
  }
};

TEST_F(SparseBitVectorTest, ConstructionAndIteration) {
  EXPECT_THAT(VectorOf(0, 2, 4), ElementsAre(0, 2, 4));
  EXPECT_THAT(VectorOf(2000, 8000, 6000, 10000),
              ElementsAre(2000, 6000, 8000, 10000));
  EXPECT_THAT(VectorOf(0, 2, 2, 0, 4, 2, 4), ElementsAre(0, 2, 4));
  EXPECT_THAT(VectorOf(7, 15, 31, 63, 127, 255),
              ElementsAre(7, 15, 31, 63, 127, 255));
  EXPECT_THAT(VectorOf(255, 127, 63, 31, 15, 7),
              ElementsAre(7, 15, 31, 63, 127, 255));
}

TEST_F(SparseBitVectorTest, Contains) {
  EXPECT_TRUE(Make(0, 2, 4).Contains(0));
  EXPECT_FALSE(Make(0, 2, 4).Contains(1));
  EXPECT_TRUE(Make(0, 2, 4).Contains(2));
  EXPECT_FALSE(Make(0, 2, 4).Contains(3));
  EXPECT_TRUE(Make(0, 2, 4).Contains(4));
  EXPECT_TRUE(Make(2000, 8000, 6000, 10000).Contains(6000));
}

TEST_F(SparseBitVectorTest, Remove) {
  EXPECT_THAT(B().Add(0, 2, 4).Remove(0).ToStdVector(), ElementsAre(2, 4));
  EXPECT_THAT(B().Add(0, 2, 4).Remove(1).ToStdVector(), ElementsAre(0, 2, 4));
  EXPECT_THAT(B().Add(0, 2, 4).Remove(2).ToStdVector(), ElementsAre(0, 4));
  EXPECT_THAT(B().Add(0, 2, 4).Remove(3).ToStdVector(), ElementsAre(0, 2, 4));
  EXPECT_THAT(B().Add(0, 2, 4).Remove(4).ToStdVector(), ElementsAre(0, 2));
  EXPECT_THAT(B().Add(2000, 8000, 6000).Remove(kMaxInt).ToStdVector(),
              ElementsAre(2000, 6000, 8000));
  EXPECT_THAT(B().Add(2000, 8000, 6000).Remove(8000).ToStdVector(),
              ElementsAre(2000, 6000));
  EXPECT_THAT(B().Add(2000, 8000, 6000).Remove(2000).ToStdVector(),
              ElementsAre(6000, 8000));
}

TEST_F(SparseBitVectorTest, ReuseEmptySegments) {
  SparseBitVector vec(zone());
  vec.Add(1000);
  size_t allocated_with_one_segment = zone()->allocation_size();
  EXPECT_GT(allocated_with_one_segment, 0u);

  // Removing bit 1000 leaves an empty segment that should be reused for the
  // next out-of-range bits (both higher and lower).
  vec.Remove(1000);
  vec.Add(2000);
  EXPECT_EQ(allocated_with_one_segment, zone()->allocation_size());
  vec.Remove(2000);
  vec.Add(500);
  EXPECT_EQ(allocated_with_one_segment, zone()->allocation_size());
  EXPECT_THAT(std::vector<int>(vec.begin(), vec.end()), ElementsAre(500));

  // Allocate a second segment while 500 is still set, then remove 500 and add
  // 3000; the empty segment for 500 should be reused for 3000.
  vec.Add(2000);
  size_t allocated_with_two_segments = zone()->allocation_size();
  EXPECT_GT(allocated_with_two_segments, allocated_with_one_segment);
  vec.Remove(500);
  vec.Add(3000);
  EXPECT_EQ(allocated_with_two_segments, zone()->allocation_size());
  EXPECT_THAT(std::vector<int>(vec.begin(), vec.end()),
              ElementsAre(2000, 3000));

  // If both segments become empty, both are preserved and reused.
  vec.Remove(2000);
  vec.Remove(3000);
  vec.Add(4000);
  vec.Add(5000);
  EXPECT_EQ(allocated_with_two_segments, zone()->allocation_size());
  EXPECT_THAT(std::vector<int>(vec.begin(), vec.end()),
              ElementsAre(4000, 5000));
}

}  // namespace v8::internal
