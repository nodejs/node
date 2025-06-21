// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/zone/zone-compact-set.h"

#include "src/zone/zone.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

struct HandleLike {
  int* ptr;
};

bool operator==(HandleLike lhs, HandleLike rhs) { return lhs.ptr == rhs.ptr; }

template <>
struct ZoneCompactSetTraits<HandleLike> {
  using handle_type = HandleLike;
  using data_type = int;

  static data_type* HandleToPointer(handle_type handle) { return handle.ptr; }
  static handle_type PointerToHandle(data_type* ptr) { return HandleLike{ptr}; }
};

class ZoneCompactSetTest : public TestWithZone {
 public:
  HandleLike NewHandleLike(int value) {
    return HandleLike{zone()->New<int>(value)};
  }
};

TEST_F(ZoneCompactSetTest, Empty) {
  ZoneCompactSet<HandleLike> zone_compact_set;

  EXPECT_EQ(zone_compact_set.size(), 0u);
  EXPECT_TRUE(zone_compact_set.is_empty());
}

TEST_F(ZoneCompactSetTest, SingleValue) {
  ZoneCompactSet<HandleLike> zone_compact_set;

  HandleLike handle = NewHandleLike(5);
  zone_compact_set.insert(handle, zone());

  EXPECT_EQ(zone_compact_set.size(), 1u);
  EXPECT_FALSE(zone_compact_set.is_empty());
  EXPECT_EQ(zone_compact_set.at(0), handle);
  EXPECT_TRUE(zone_compact_set.contains(handle));
}

TEST_F(ZoneCompactSetTest, MultipleValue) {
  ZoneCompactSet<HandleLike> zone_compact_set;

  HandleLike handle1 = NewHandleLike(5);
  HandleLike handle2 = NewHandleLike(8);
  HandleLike handle3 = NewHandleLike(2);
  HandleLike handle4 = NewHandleLike(1);

  zone_compact_set.insert(handle1, zone());
  zone_compact_set.insert(handle2, zone());
  zone_compact_set.insert(handle3, zone());
  zone_compact_set.insert(handle4, zone());

  EXPECT_EQ(zone_compact_set.size(), 4u);
  EXPECT_FALSE(zone_compact_set.is_empty());

  EXPECT_TRUE(zone_compact_set.contains(handle1));
  EXPECT_TRUE(zone_compact_set.contains(handle2));
  EXPECT_TRUE(zone_compact_set.contains(handle3));
  EXPECT_TRUE(zone_compact_set.contains(handle4));
}

TEST_F(ZoneCompactSetTest, DuplicateValue) {
  ZoneCompactSet<HandleLike> zone_compact_set;

  HandleLike handle1 = NewHandleLike(5);
  HandleLike handle2 = NewHandleLike(8);

  zone_compact_set.insert(handle1, zone());
  zone_compact_set.insert(handle1, zone());
  zone_compact_set.insert(handle2, zone());
  zone_compact_set.insert(handle2, zone());

  EXPECT_EQ(zone_compact_set.size(), 2u);
  EXPECT_FALSE(zone_compact_set.is_empty());

  EXPECT_TRUE(zone_compact_set.contains(handle1));
  EXPECT_TRUE(zone_compact_set.contains(handle2));
}

TEST_F(ZoneCompactSetTest, RemoveSingleValue) {
  ZoneCompactSet<HandleLike> zone_compact_set;

  HandleLike handle1 = NewHandleLike(5);

  zone_compact_set.insert(handle1, zone());

  EXPECT_EQ(zone_compact_set.size(), 1u);

  zone_compact_set.remove(handle1, zone());

  EXPECT_EQ(zone_compact_set.size(), 0u);
  EXPECT_TRUE(zone_compact_set.is_empty());

  EXPECT_FALSE(zone_compact_set.contains(handle1));
}

TEST_F(ZoneCompactSetTest, RemoveFromMultipleValue) {
  ZoneCompactSet<HandleLike> zone_compact_set;

  HandleLike handle1 = NewHandleLike(5);
  HandleLike handle2 = NewHandleLike(8);

  zone_compact_set.insert(handle1, zone());
  zone_compact_set.insert(handle2, zone());

  EXPECT_EQ(zone_compact_set.size(), 2u);

  zone_compact_set.remove(handle1, zone());

  EXPECT_EQ(zone_compact_set.size(), 1u);
  EXPECT_FALSE(zone_compact_set.is_empty());

  EXPECT_FALSE(zone_compact_set.contains(handle1));
  EXPECT_TRUE(zone_compact_set.contains(handle2));
}

TEST_F(ZoneCompactSetTest, RemoveFromEvenMoreMultipleValue) {
  ZoneCompactSet<HandleLike> zone_compact_set;

  HandleLike handle1 = NewHandleLike(5);
  HandleLike handle2 = NewHandleLike(8);
  HandleLike handle3 = NewHandleLike(1);
  HandleLike handle4 = NewHandleLike(2);

  zone_compact_set.insert(handle1, zone());
  zone_compact_set.insert(handle2, zone());
  zone_compact_set.insert(handle3, zone());
  zone_compact_set.insert(handle4, zone());

  EXPECT_EQ(zone_compact_set.size(), 4u);

  zone_compact_set.remove(handle2, zone());

  EXPECT_EQ(zone_compact_set.size(), 3u);
  EXPECT_FALSE(zone_compact_set.is_empty());

  EXPECT_TRUE(zone_compact_set.contains(handle1));
  EXPECT_FALSE(zone_compact_set.contains(handle2));
  EXPECT_TRUE(zone_compact_set.contains(handle3));
  EXPECT_TRUE(zone_compact_set.contains(handle4));
}

TEST_F(ZoneCompactSetTest, RemoveNonExistent) {
  ZoneCompactSet<HandleLike> zone_compact_set;

  HandleLike handle1 = NewHandleLike(5);
  HandleLike handle2 = NewHandleLike(8);
  HandleLike handle3 = NewHandleLike(1);

  zone_compact_set.insert(handle1, zone());
  zone_compact_set.insert(handle2, zone());

  zone_compact_set.remove(handle3, zone());

  EXPECT_EQ(zone_compact_set.size(), 2u);
  EXPECT_FALSE(zone_compact_set.is_empty());

  EXPECT_TRUE(zone_compact_set.contains(handle1));
  EXPECT_TRUE(zone_compact_set.contains(handle2));
  EXPECT_FALSE(zone_compact_set.contains(handle3));
}

TEST_F(ZoneCompactSetTest, ContainsEmptySubset) {
  ZoneCompactSet<HandleLike> zone_compact_set;
  ZoneCompactSet<HandleLike> zone_compact_subset;

  HandleLike handle1 = NewHandleLike(5);
  HandleLike handle2 = NewHandleLike(8);

  zone_compact_set.insert(handle1, zone());
  zone_compact_set.insert(handle2, zone());

  EXPECT_TRUE(zone_compact_set.contains(zone_compact_subset));
  EXPECT_FALSE(zone_compact_subset.contains(zone_compact_set));
}

TEST_F(ZoneCompactSetTest, ContainsSingleElementSubset) {
  ZoneCompactSet<HandleLike> zone_compact_set;
  ZoneCompactSet<HandleLike> zone_compact_subset;

  HandleLike handle1 = NewHandleLike(5);
  HandleLike handle2 = NewHandleLike(8);

  zone_compact_set.insert(handle1, zone());
  zone_compact_set.insert(handle2, zone());

  zone_compact_subset.insert(handle1, zone());

  EXPECT_TRUE(zone_compact_set.contains(zone_compact_subset));
  EXPECT_FALSE(zone_compact_subset.contains(zone_compact_set));
}

TEST_F(ZoneCompactSetTest, ContainsMultiElementSubset) {
  ZoneCompactSet<HandleLike> zone_compact_set;
  ZoneCompactSet<HandleLike> zone_compact_subset;

  HandleLike handle1 = NewHandleLike(5);
  HandleLike handle2 = NewHandleLike(8);
  HandleLike handle3 = NewHandleLike(2);
  HandleLike handle4 = NewHandleLike(1);

  zone_compact_set.insert(handle1, zone());
  zone_compact_set.insert(handle2, zone());
  zone_compact_set.insert(handle3, zone());
  zone_compact_set.insert(handle4, zone());

  zone_compact_subset.insert(handle2, zone());
  zone_compact_subset.insert(handle3, zone());

  EXPECT_TRUE(zone_compact_set.contains(zone_compact_subset));
  EXPECT_FALSE(zone_compact_subset.contains(zone_compact_set));
}

TEST_F(ZoneCompactSetTest, DoesNotContainsNonSubset) {
  ZoneCompactSet<HandleLike> zone_compact_set;
  ZoneCompactSet<HandleLike> zone_compact_other_set;

  HandleLike handle1 = NewHandleLike(5);
  HandleLike handle2 = NewHandleLike(8);
  HandleLike handle3 = NewHandleLike(2);
  HandleLike handle4 = NewHandleLike(1);

  zone_compact_set.insert(handle1, zone());
  zone_compact_set.insert(handle2, zone());
  zone_compact_set.insert(handle3, zone());

  zone_compact_other_set.insert(handle2, zone());
  zone_compact_other_set.insert(handle4, zone());

  EXPECT_FALSE(zone_compact_set.contains(zone_compact_other_set));
  EXPECT_FALSE(zone_compact_other_set.contains(zone_compact_set));
}

}  // namespace internal
}  // namespace v8
