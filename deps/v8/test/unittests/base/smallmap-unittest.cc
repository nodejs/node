// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2023 the V8 project authors. All rights reserved.
// This file is a clone of "base/containers/small_map_unittest.h" in chromium.
// Keep in sync, especially when fixing bugs.

#include <algorithm>
#include <unordered_map>

#include "src/base/small-map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace base {

TEST(SmallMapTest, General) {
  SmallMap<std::unordered_map<int, int>> m;

  EXPECT_TRUE(m.empty());

  m[0] = 5;

  EXPECT_FALSE(m.empty());
  EXPECT_EQ(m.size(), 1u);

  m[9] = 2;

  EXPECT_FALSE(m.empty());
  EXPECT_EQ(m.size(), 2u);

  EXPECT_EQ(m[9], 2);
  EXPECT_EQ(m[0], 5);
  EXPECT_FALSE(m.UsingFullMap());

  SmallMap<std::unordered_map<int, int>>::iterator iter(m.begin());
  ASSERT_TRUE(iter != m.end());
  EXPECT_EQ(iter->first, 0);
  EXPECT_EQ(iter->second, 5);
  ++iter;
  ASSERT_TRUE(iter != m.end());
  EXPECT_EQ((*iter).first, 9);
  EXPECT_EQ((*iter).second, 2);
  ++iter;
  EXPECT_TRUE(iter == m.end());

  m[8] = 23;
  m[1234] = 90;
  m[-5] = 6;

  EXPECT_EQ(m[9], 2);
  EXPECT_EQ(m[0], 5);
  EXPECT_EQ(m[1234], 90);
  EXPECT_EQ(m[8], 23);
  EXPECT_EQ(m[-5], 6);
  EXPECT_EQ(m.size(), 5u);
  EXPECT_FALSE(m.empty());
  EXPECT_TRUE(m.UsingFullMap());

  iter = m.begin();
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(iter != m.end());
    ++iter;
  }
  EXPECT_TRUE(iter == m.end());

  const SmallMap<std::unordered_map<int, int>>& ref = m;
  EXPECT_TRUE(ref.find(1234) != m.end());
  EXPECT_TRUE(ref.find(5678) == m.end());
}

TEST(SmallMapTest, PostFixIteratorIncrement) {
  SmallMap<std::unordered_map<int, int>> m;
  m[0] = 5;
  m[2] = 3;

  {
    SmallMap<std::unordered_map<int, int>>::iterator iter(m.begin());
    SmallMap<std::unordered_map<int, int>>::iterator last(iter++);
    ++last;
    EXPECT_TRUE(last == iter);
  }

  {
    SmallMap<std::unordered_map<int, int>>::const_iterator iter(m.begin());
    SmallMap<std::unordered_map<int, int>>::const_iterator last(iter++);
    ++last;
    EXPECT_TRUE(last == iter);
  }
}

// Based on the General testcase.
TEST(SmallMapTest, CopyConstructor) {
  SmallMap<std::unordered_map<int, int>> src;

  {
    SmallMap<std::unordered_map<int, int>> m(src);
    EXPECT_TRUE(m.empty());
  }

  src[0] = 5;

  {
    SmallMap<std::unordered_map<int, int>> m(src);
    EXPECT_FALSE(m.empty());
    EXPECT_EQ(m.size(), 1u);
  }

  src[9] = 2;

  {
    SmallMap<std::unordered_map<int, int>> m(src);
    EXPECT_FALSE(m.empty());
    EXPECT_EQ(m.size(), 2u);

    EXPECT_EQ(m[9], 2);
    EXPECT_EQ(m[0], 5);
    EXPECT_FALSE(m.UsingFullMap());
  }

  src[8] = 23;
  src[1234] = 90;
  src[-5] = 6;

  {
    SmallMap<std::unordered_map<int, int>> m(src);
    EXPECT_EQ(m[9], 2);
    EXPECT_EQ(m[0], 5);
    EXPECT_EQ(m[1234], 90);
    EXPECT_EQ(m[8], 23);
    EXPECT_EQ(m[-5], 6);
    EXPECT_EQ(m.size(), 5u);
    EXPECT_FALSE(m.empty());
    EXPECT_TRUE(m.UsingFullMap());
  }
}

template <class inner>
static bool SmallMapIsSubset(SmallMap<inner> const& a,
                             SmallMap<inner> const& b) {
  typename SmallMap<inner>::const_iterator it;
  for (it = a.begin(); it != a.end(); ++it) {
    typename SmallMap<inner>::const_iterator it_in_b = b.find(it->first);
    if (it_in_b == b.end() || it_in_b->second != it->second) return false;
  }
  return true;
}

template <class inner>
static bool SmallMapEqual(SmallMap<inner> const& a, SmallMap<inner> const& b) {
  return SmallMapIsSubset(a, b) && SmallMapIsSubset(b, a);
}

TEST(SmallMapTest, AssignmentOperator) {
  SmallMap<std::unordered_map<int, int>> src_small;
  SmallMap<std::unordered_map<int, int>> src_large;

  src_small[1] = 20;
  src_small[2] = 21;
  src_small[3] = 22;
  EXPECT_FALSE(src_small.UsingFullMap());

  src_large[1] = 20;
  src_large[2] = 21;
  src_large[3] = 22;
  src_large[5] = 23;
  src_large[6] = 24;
  src_large[7] = 25;
  EXPECT_TRUE(src_large.UsingFullMap());

  // Assignments to empty.
  SmallMap<std::unordered_map<int, int>> dest_small;
  dest_small = src_small;
  EXPECT_TRUE(SmallMapEqual(dest_small, src_small));
  EXPECT_EQ(dest_small.UsingFullMap(), src_small.UsingFullMap());

  SmallMap<std::unordered_map<int, int>> dest_large;
  dest_large = src_large;
  EXPECT_TRUE(SmallMapEqual(dest_large, src_large));
  EXPECT_EQ(dest_large.UsingFullMap(), src_large.UsingFullMap());

  // Assignments which assign from full to small, and vice versa.
  dest_small = src_large;
  EXPECT_TRUE(SmallMapEqual(dest_small, src_large));
  EXPECT_EQ(dest_small.UsingFullMap(), src_large.UsingFullMap());

  dest_large = src_small;
  EXPECT_TRUE(SmallMapEqual(dest_large, src_small));
  EXPECT_EQ(dest_large.UsingFullMap(), src_small.UsingFullMap());

  // Double check that SmallMapEqual works:
  dest_large[42] = 666;
  EXPECT_FALSE(SmallMapEqual(dest_large, src_small));
}

TEST(SmallMapTest, Insert) {
  SmallMap<std::unordered_map<int, int>> sm;

  // loop through the transition from small map to map.
  for (int i = 1; i <= 10; ++i) {
    // insert an element
    std::pair<SmallMap<std::unordered_map<int, int>>::iterator, bool> ret;
    ret = sm.insert(std::make_pair(i, 100 * i));
    EXPECT_TRUE(ret.second);
    EXPECT_TRUE(ret.first == sm.find(i));
    EXPECT_EQ(ret.first->first, i);
    EXPECT_EQ(ret.first->second, 100 * i);

    // try to insert it again with different value, fails, but we still get an
    // iterator back with the original value.
    ret = sm.insert(std::make_pair(i, -i));
    EXPECT_FALSE(ret.second);
    EXPECT_TRUE(ret.first == sm.find(i));
    EXPECT_EQ(ret.first->first, i);
    EXPECT_EQ(ret.first->second, 100 * i);

    // check the state of the map.
    for (int j = 1; j <= i; ++j) {
      SmallMap<std::unordered_map<int, int>>::iterator it = sm.find(j);
      EXPECT_TRUE(it != sm.end());
      EXPECT_EQ(it->first, j);
      EXPECT_EQ(it->second, j * 100);
    }
    EXPECT_EQ(sm.size(), static_cast<size_t>(i));
    EXPECT_FALSE(sm.empty());
  }
}

TEST(SmallMapTest, InsertRange) {
  // loop through the transition from small map to map.
  for (int elements = 0; elements <= 10; ++elements) {
    std::unordered_map<int, int> normal_map;
    for (int i = 1; i <= elements; ++i) {
      normal_map.insert(std::make_pair(i, 100 * i));
    }

    SmallMap<std::unordered_map<int, int>> sm;
    sm.insert(normal_map.begin(), normal_map.end());
    EXPECT_EQ(normal_map.size(), sm.size());
    for (int i = 1; i <= elements; ++i) {
      EXPECT_TRUE(sm.find(i) != sm.end());
      EXPECT_EQ(sm.find(i)->first, i);
      EXPECT_EQ(sm.find(i)->second, 100 * i);
    }
  }
}

TEST(SmallMapTest, Erase) {
  SmallMap<std::unordered_map<std::string, int>> m;
  SmallMap<std::unordered_map<std::string, int>>::iterator iter;

  m["monday"] = 1;
  m["tuesday"] = 2;
  m["wednesday"] = 3;

  EXPECT_EQ(m["monday"], 1);
  EXPECT_EQ(m["tuesday"], 2);
  EXPECT_EQ(m["wednesday"], 3);
  EXPECT_EQ(m.count("tuesday"), 1u);
  EXPECT_FALSE(m.UsingFullMap());

  iter = m.begin();
  ASSERT_TRUE(iter != m.end());
  EXPECT_EQ(iter->first, "monday");
  EXPECT_EQ(iter->second, 1);
  ++iter;
  ASSERT_TRUE(iter != m.end());
  EXPECT_EQ(iter->first, "tuesday");
  EXPECT_EQ(iter->second, 2);
  ++iter;
  ASSERT_TRUE(iter != m.end());
  EXPECT_EQ(iter->first, "wednesday");
  EXPECT_EQ(iter->second, 3);
  ++iter;
  EXPECT_TRUE(iter == m.end());

  EXPECT_EQ(m.erase("tuesday"), 1u);

  EXPECT_EQ(m["monday"], 1);
  EXPECT_EQ(m["wednesday"], 3);
  EXPECT_EQ(m.count("tuesday"), 0u);
  EXPECT_EQ(m.erase("tuesday"), 0u);

  iter = m.begin();
  ASSERT_TRUE(iter != m.end());
  EXPECT_EQ(iter->first, "monday");
  EXPECT_EQ(iter->second, 1);
  ++iter;
  ASSERT_TRUE(iter != m.end());
  EXPECT_EQ(iter->first, "wednesday");
  EXPECT_EQ(iter->second, 3);
  ++iter;
  EXPECT_TRUE(iter == m.end());

  m["thursday"] = 4;
  m["friday"] = 5;
  EXPECT_EQ(m.size(), 4u);
  EXPECT_FALSE(m.empty());
  EXPECT_FALSE(m.UsingFullMap());

  m["saturday"] = 6;
  EXPECT_TRUE(m.UsingFullMap());

  EXPECT_EQ(m.count("friday"), 1u);
  EXPECT_EQ(m.erase("friday"), 1u);
  EXPECT_TRUE(m.UsingFullMap());
  EXPECT_EQ(m.count("friday"), 0u);
  EXPECT_EQ(m.erase("friday"), 0u);

  EXPECT_EQ(m.size(), 4u);
  EXPECT_FALSE(m.empty());
  EXPECT_EQ(m.erase("monday"), 1u);
  EXPECT_EQ(m.size(), 3u);
  EXPECT_FALSE(m.empty());

  m.clear();
  EXPECT_FALSE(m.UsingFullMap());
  EXPECT_EQ(m.size(), 0u);
  EXPECT_TRUE(m.empty());
}

TEST(SmallMapTest, EraseReturnsIteratorFollowingRemovedElement) {
  SmallMap<std::unordered_map<std::string, int>> m;
  SmallMap<std::unordered_map<std::string, int>>::iterator iter;

  m["a"] = 0;
  m["b"] = 1;
  m["c"] = 2;

  // Erase first item.
  auto following_iter = m.erase(m.begin());
  EXPECT_EQ(m.begin(), following_iter);
  EXPECT_EQ(2u, m.size());
  EXPECT_EQ(m.count("a"), 0u);
  EXPECT_EQ(m.count("b"), 1u);
  EXPECT_EQ(m.count("c"), 1u);

  // Iterate to last item and erase it.
  ++following_iter;
  following_iter = m.erase(following_iter);
  ASSERT_EQ(1u, m.size());
  EXPECT_EQ(m.end(), following_iter);
  EXPECT_EQ(m.count("b"), 0u);
  EXPECT_EQ(m.count("c"), 1u);

  // Erase remaining item.
  following_iter = m.erase(m.begin());
  EXPECT_TRUE(m.empty());
  EXPECT_EQ(m.end(), following_iter);
}

TEST(SmallMapTest, NonHashMap) {
  SmallMap<std::map<int, int>, 4, std::equal_to<int>> m;
  EXPECT_TRUE(m.empty());

  m[9] = 2;
  m[0] = 5;

  EXPECT_EQ(m[9], 2);
  EXPECT_EQ(m[0], 5);
  EXPECT_EQ(m.size(), 2u);
  EXPECT_FALSE(m.empty());
  EXPECT_FALSE(m.UsingFullMap());

  SmallMap<std::map<int, int>, 4, std::equal_to<int>>::iterator iter(m.begin());
  ASSERT_TRUE(iter != m.end());
  EXPECT_EQ(iter->first, 9);
  EXPECT_EQ(iter->second, 2);
  ++iter;
  ASSERT_TRUE(iter != m.end());
  EXPECT_EQ(iter->first, 0);
  EXPECT_EQ(iter->second, 5);
  ++iter;
  EXPECT_TRUE(iter == m.end());
  --iter;
  ASSERT_TRUE(iter != m.end());
  EXPECT_EQ(iter->first, 0);
  EXPECT_EQ(iter->second, 5);

  m[8] = 23;
  m[1234] = 90;
  m[-5] = 6;

  EXPECT_EQ(m[9], 2);
  EXPECT_EQ(m[0], 5);
  EXPECT_EQ(m[1234], 90);
  EXPECT_EQ(m[8], 23);
  EXPECT_EQ(m[-5], 6);
  EXPECT_EQ(m.size(), 5u);
  EXPECT_FALSE(m.empty());
  EXPECT_TRUE(m.UsingFullMap());

  iter = m.begin();
  ASSERT_TRUE(iter != m.end());
  EXPECT_EQ(iter->first, -5);
  EXPECT_EQ(iter->second, 6);
  ++iter;
  ASSERT_TRUE(iter != m.end());
  EXPECT_EQ(iter->first, 0);
  EXPECT_EQ(iter->second, 5);
  ++iter;
  ASSERT_TRUE(iter != m.end());
  EXPECT_EQ(iter->first, 8);
  EXPECT_EQ(iter->second, 23);
  ++iter;
  ASSERT_TRUE(iter != m.end());
  EXPECT_EQ(iter->first, 9);
  EXPECT_EQ(iter->second, 2);
  ++iter;
  ASSERT_TRUE(iter != m.end());
  EXPECT_EQ(iter->first, 1234);
  EXPECT_EQ(iter->second, 90);
  ++iter;
  EXPECT_TRUE(iter == m.end());
  --iter;
  ASSERT_TRUE(iter != m.end());
  EXPECT_EQ(iter->first, 1234);
  EXPECT_EQ(iter->second, 90);
}

TEST(SmallMapTest, DefaultEqualKeyWorks) {
  // If these tests compile, they pass. The EXPECT calls are only there to avoid
  // unused variable warnings.
  SmallMap<std::unordered_map<int, int>> hm;
  EXPECT_EQ(0u, hm.size());
  SmallMap<std::map<int, int>> m;
  EXPECT_EQ(0u, m.size());
}

namespace {

class unordered_map_add_item : public std::unordered_map<int, int> {
 public:
  unordered_map_add_item() = default;
  explicit unordered_map_add_item(const std::pair<int, int>& item) {
    insert(item);
  }
};

void InitMap(unordered_map_add_item* map_ctor) {
  new (map_ctor) unordered_map_add_item(std::make_pair(0, 0));
}

class unordered_map_add_item_initializer {
 public:
  explicit unordered_map_add_item_initializer(int item_to_add)
      : item_(item_to_add) {}
  unordered_map_add_item_initializer() : item_(0) {}
  void operator()(unordered_map_add_item* map_ctor) const {
    new (map_ctor) unordered_map_add_item(std::make_pair(item_, item_));
  }

  int item_;
};

}  // anonymous namespace

TEST(SmallMapTest, SubclassInitializationWithFunctionPointer) {
  SmallMap<unordered_map_add_item, 4, std::equal_to<int>,
           void (&)(unordered_map_add_item*)>
      m(InitMap);

  EXPECT_TRUE(m.empty());

  m[1] = 1;
  m[2] = 2;
  m[3] = 3;
  m[4] = 4;

  EXPECT_EQ(4u, m.size());
  EXPECT_EQ(0u, m.count(0));

  m[5] = 5;
  EXPECT_EQ(6u, m.size());
  // Our function adds an extra item when we convert to a map.
  EXPECT_EQ(1u, m.count(0));
}

TEST(SmallMapTest, SubclassInitializationWithFunctionObject) {
  SmallMap<unordered_map_add_item, 4, std::equal_to<int>,
           unordered_map_add_item_initializer>
      m(unordered_map_add_item_initializer(-1));

  EXPECT_TRUE(m.empty());

  m[1] = 1;
  m[2] = 2;
  m[3] = 3;
  m[4] = 4;

  EXPECT_EQ(4u, m.size());
  EXPECT_EQ(0u, m.count(-1));

  m[5] = 5;
  EXPECT_EQ(6u, m.size());
  // Our functor adds an extra item when we convert to a map.
  EXPECT_EQ(1u, m.count(-1));
}

namespace {

// This class acts as a basic implementation of a move-only type. The canonical
// example of such a type is scoped_ptr/unique_ptr.
template <typename V>
class MoveOnlyType {
 public:
  MoveOnlyType() : value_(0) {}
  explicit MoveOnlyType(V value) : value_(value) {}

  MoveOnlyType(MoveOnlyType&& other) { *this = std::move(other); }

  MoveOnlyType& operator=(MoveOnlyType&& other) {
    value_ = other.value_;
    other.value_ = 0;
    return *this;
  }

  MoveOnlyType(const MoveOnlyType&) = delete;
  MoveOnlyType& operator=(const MoveOnlyType&) = delete;

  V value() const { return value_; }

 private:
  V value_;
};

}  // namespace

TEST(SmallMapTest, MoveOnlyValueType) {
  SmallMap<std::map<int, MoveOnlyType<int>>, 2> m;

  m[0] = MoveOnlyType<int>(1);
  m[1] = MoveOnlyType<int>(2);
  m.erase(m.begin());

  // SmallMap will move m[1] to an earlier index in the internal array.
  EXPECT_EQ(m.size(), 1u);
  EXPECT_EQ(m[1].value(), 2);

  m[0] = MoveOnlyType<int>(1);
  // SmallMap must move the values from the array into the internal std::map.
  m[2] = MoveOnlyType<int>(3);

  EXPECT_EQ(m.size(), 3u);
  EXPECT_EQ(m[0].value(), 1);
  EXPECT_EQ(m[1].value(), 2);
  EXPECT_EQ(m[2].value(), 3);

  m.erase(m.begin());

  // SmallMap should also let internal std::map erase with a move-only type.
  EXPECT_EQ(m.size(), 2u);
  EXPECT_EQ(m[1].value(), 2);
  EXPECT_EQ(m[2].value(), 3);
}

TEST(SmallMapTest, Emplace) {
  SmallMap<std::map<size_t, MoveOnlyType<size_t>>> sm;

  // loop through the transition from small map to map.
  for (size_t i = 1; i <= 10; ++i) {
    // insert an element
    auto ret = sm.emplace(i, MoveOnlyType<size_t>(100 * i));
    EXPECT_TRUE(ret.second);
    EXPECT_TRUE(ret.first == sm.find(i));
    EXPECT_EQ(ret.first->first, i);
    EXPECT_EQ(ret.first->second.value(), 100 * i);

    // try to insert it again with different value, fails, but we still get an
    // iterator back with the original value.
    ret = sm.emplace(i, MoveOnlyType<size_t>(i));
    EXPECT_FALSE(ret.second);
    EXPECT_TRUE(ret.first == sm.find(i));
    EXPECT_EQ(ret.first->first, i);
    EXPECT_EQ(ret.first->second.value(), 100 * i);

    // check the state of the map.
    for (size_t j = 1; j <= i; ++j) {
      const auto it = sm.find(j);
      EXPECT_TRUE(it != sm.end());
      EXPECT_EQ(it->first, j);
      EXPECT_EQ(it->second.value(), j * 100);
    }
    EXPECT_EQ(sm.size(), i);
    EXPECT_FALSE(sm.empty());
  }
}

}  // namespace base
}  // namespace v8
