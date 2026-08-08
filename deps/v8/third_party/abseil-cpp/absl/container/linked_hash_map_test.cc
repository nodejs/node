// Copyright 2025 The Abseil Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/container/linked_hash_map.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/base/internal/exception_testing.h"
#include "absl/container/internal/hash_generator_testing.h"
#include "absl/container/internal/hash_policy_testing.h"
#include "absl/container/internal/heterogeneous_lookup_testing.h"
#include "absl/container/internal/test_instance_tracker.h"
#include "absl/container/internal/unordered_map_constructor_test.h"
#include "absl/container/internal/unordered_map_lookup_test.h"
#include "absl/container/internal/unordered_map_members_test.h"
#include "absl/container/internal/unordered_map_modifiers_test.h"
#include "absl/strings/string_view.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace container_internal {
namespace {

using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::Pointee;

template <class K, class V>
using Map = linked_hash_map<K, V, StatefulTestingHash, StatefulTestingEqual,
                            Alloc<std::pair<const K, V>>>;

static_assert(!std::is_standard_layout<NonStandardLayout>(), "");

using MapTypes =
    ::testing::Types<Map<int, int>, Map<std::string, int>,
                     Map<Enum, std::string>, Map<EnumClass, int>,
                     Map<int, NonStandardLayout>, Map<NonStandardLayout, int>>;

INSTANTIATE_TYPED_TEST_SUITE_P(LinkedHashMap, ConstructorTest, MapTypes);
INSTANTIATE_TYPED_TEST_SUITE_P(LinkedHashMap, LookupTest, MapTypes);
INSTANTIATE_TYPED_TEST_SUITE_P(LinkedHashMap, MembersTest, MapTypes);
INSTANTIATE_TYPED_TEST_SUITE_P(LinkedHashMap, ModifiersTest, MapTypes);

// Tests that the range constructor works.
TEST(LinkedHashMapTest, RangeConstruct) {
  const std::pair<int, int> items[] = {{1, 2}, {2, 3}, {3, 4}};
  EXPECT_THAT((linked_hash_map<int, int>(std::begin(items), std::end(items))),
              ElementsAre(Pair(1, 2), Pair(2, 3), Pair(3, 4)));
}

// Tests that copying works.
TEST(LinkedHashMapTest, Copy) {
  linked_hash_map<int, int> m{{2, 12}, {3, 13}};
  auto copy = m;

  EXPECT_TRUE(copy.contains(2));
  auto found = copy.find(2);
  ASSERT_TRUE(found != copy.end());
  for (auto iter = copy.begin(); iter != copy.end(); ++iter) {
    if (iter == found) return;
  }
  FAIL() << "Copied map's find method returned an invalid iterator.";
}

// Tests that assignment works.
TEST(LinkedHashMapTest, Assign) {
  linked_hash_map<int, int> m{{2, 12}, {3, 13}};
  linked_hash_map<int, int> n{{4, 14}};

  n = m;
  EXPECT_TRUE(n.contains(2));
  auto found = n.find(2);
  ASSERT_TRUE(found != n.end());
  for (auto iter = n.begin(); iter != n.end(); ++iter) {
    if (iter == found) return;
  }
  FAIL() << "Assigned map's find method returned an invalid iterator.";
}

// Tests that self-assignment works.
TEST(LinkedHashMapTest, SelfAssign) {
  linked_hash_map<int, int> a{{1, 1}, {2, 2}, {3, 3}};
  auto& a_ref = a;
  a = a_ref;
  EXPECT_THAT(a, ElementsAre(Pair(1, 1), Pair(2, 2), Pair(3, 3)));
}

// Tests that move constructor works.
TEST(LinkedHashMapTest, Move) {
  // Use unique_ptr as an example of a non-copyable type.
  linked_hash_map<int, std::unique_ptr<int>> m;
  m[2] = std::make_unique<int>(12);
  m[3] = std::make_unique<int>(13);
  linked_hash_map<int, std::unique_ptr<int>> n = std::move(m);
  EXPECT_THAT(n, ElementsAre(Pair(2, Pointee(12)), Pair(3, Pointee(13))));
}

// Tests that self-moving works.
TEST(LinkedHashMapTest, SelfMove) {
  linked_hash_map<int, int> a{{1, 1}, {2, 2}, {3, 3}};
  auto& a_ref = a;
  a = std::move(a_ref);
  EXPECT_THAT(a, ElementsAre(Pair(1, 1), Pair(2, 2), Pair(3, 3)));
}

TEST(LinkedHashMapTest, CanInsertMoveOnly) {
  linked_hash_map<int, std::unique_ptr<int>> m;
  struct Data {
    int k, v;
  };
  const Data data[] = {{1, 123}, {3, 345}, {2, 234}, {4, 456}};
  for (const auto& kv : data)
    m.insert({kv.k, std::make_unique<int>(int{kv.v})});
  EXPECT_TRUE(m.contains(2));
  auto found = m.find(2);
  ASSERT_TRUE(found != m.end());
  EXPECT_EQ(234, *found->second);
}

TEST(LinkedHashMapTest, CanEmplaceMoveOnly) {
  linked_hash_map<int, std::unique_ptr<int>> m;
  struct Data {
    int k, v;
  };
  const Data data[] = {{1, 123}, {3, 345}, {2, 234}, {4, 456}};
  for (const auto& kv : data) {
    m.emplace(std::piecewise_construct, std::make_tuple(kv.k),
              std::make_tuple(new int{kv.v}));
  }
  EXPECT_TRUE(m.contains(2));
  auto found = m.find(2);
  ASSERT_TRUE(found != m.end());
  EXPECT_EQ(234, *found->second);
}

struct NoCopy {
  explicit NoCopy(int x) : x(x) {}
  NoCopy(const NoCopy&) = delete;
  NoCopy& operator=(const NoCopy&) = delete;
  NoCopy(NoCopy&&) = delete;
  NoCopy& operator=(NoCopy&&) = delete;
  int x;
};

TEST(LinkedHashMapTest, CanEmplaceNoMoveNoCopy) {
  linked_hash_map<int, NoCopy> m;
  struct Data {
    int k, v;
  };
  const Data data[] = {{1, 123}, {3, 345}, {2, 234}, {4, 456}};
  for (const auto& kv : data) {
    m.emplace(std::piecewise_construct, std::make_tuple(kv.k),
              std::make_tuple(kv.v));
  }
  EXPECT_TRUE(m.contains(2));
  auto found = m.find(2);
  ASSERT_TRUE(found != m.end());
  EXPECT_EQ(234, found->second.x);
}

TEST(LinkedHashMapTest, ConstKeys) {
  linked_hash_map<int, int> m;
  m.insert(std::make_pair(1, 2));
  // Test that keys are const in iteration.
  std::pair<const int, int>& p = *m.begin();
  EXPECT_EQ(1, p.first);
}

// Tests that iteration from begin() to end() works
TEST(LinkedHashMapTest, Iteration) {
  linked_hash_map<int, int> m;
  EXPECT_TRUE(m.begin() == m.end());

  m.insert(std::make_pair(2, 12));
  m.insert(std::make_pair(1, 11));
  m.insert(std::make_pair(3, 13));

  linked_hash_map<int, int>::iterator i = m.begin();
  ASSERT_TRUE(m.begin() == i);
  ASSERT_TRUE(m.end() != i);
  EXPECT_EQ(2, i->first);
  EXPECT_EQ(12, i->second);

  ++i;
  ASSERT_TRUE(m.end() != i);
  EXPECT_EQ(1, i->first);
  EXPECT_EQ(11, i->second);

  ++i;
  ASSERT_TRUE(m.end() != i);
  EXPECT_EQ(3, i->first);
  EXPECT_EQ(13, i->second);

  ++i;  // Should be the end of the line.
  ASSERT_TRUE(m.end() == i);
}

// Tests that reverse iteration from rbegin() to rend() works
TEST(LinkedHashMapTest, ReverseIteration) {
  linked_hash_map<int, int> m;
  EXPECT_TRUE(m.rbegin() == m.rend());

  m.insert(std::make_pair(2, 12));
  m.insert(std::make_pair(1, 11));
  m.insert(std::make_pair(3, 13));

  linked_hash_map<int, int>::reverse_iterator i = m.rbegin();
  ASSERT_TRUE(m.rbegin() == i);
  ASSERT_TRUE(m.rend() != i);
  EXPECT_EQ(3, i->first);
  EXPECT_EQ(13, i->second);

  ++i;
  ASSERT_TRUE(m.rend() != i);
  EXPECT_EQ(1, i->first);
  EXPECT_EQ(11, i->second);

  ++i;
  ASSERT_TRUE(m.rend() != i);
  EXPECT_EQ(2, i->first);
  EXPECT_EQ(12, i->second);

  ++i;  // Should be the end of the line.
  ASSERT_TRUE(m.rend() == i);
}

// Tests that clear() works
TEST(LinkedHashMapTest, Clear) {
  linked_hash_map<int, int> m;
  m.insert(std::make_pair(2, 12));
  m.insert(std::make_pair(1, 11));
  m.insert(std::make_pair(3, 13));

  ASSERT_EQ(3, m.size());

  m.clear();

  EXPECT_EQ(0, m.size());

  m.clear();  // Make sure we can call it on an empty map.

  EXPECT_EQ(0, m.size());
}

// Tests that size() works.
TEST(LinkedHashMapTest, Size) {
  linked_hash_map<int, int> m;
  EXPECT_EQ(0, m.size());
  m.insert(std::make_pair(2, 12));
  EXPECT_EQ(1, m.size());
  m.insert(std::make_pair(1, 11));
  EXPECT_EQ(2, m.size());
  m.insert(std::make_pair(3, 13));
  EXPECT_EQ(3, m.size());
  m.clear();
  EXPECT_EQ(0, m.size());
}

// Tests empty()
TEST(LinkedHashMapTest, Empty) {
  linked_hash_map<int, int> m;
  ASSERT_TRUE(m.empty());
  m.insert(std::make_pair(2, 12));
  ASSERT_FALSE(m.empty());
  m.clear();
  ASSERT_TRUE(m.empty());
}

TEST(LinkedHashMapTest, Erase) {
  linked_hash_map<int, int> m;
  ASSERT_EQ(0, m.size());
  EXPECT_EQ(0, m.erase(2));  // Nothing to erase yet

  m.insert(std::make_pair(2, 12));
  ASSERT_EQ(1, m.size());
  EXPECT_EQ(1, m.erase(2));
  EXPECT_EQ(0, m.size());

  EXPECT_EQ(0, m.erase(2));  // Make sure nothing bad happens if we repeat.
  EXPECT_EQ(0, m.size());
}

TEST(LinkedHashMapTest, Erase2) {
  linked_hash_map<int, int> m;
  ASSERT_EQ(0, m.size());
  EXPECT_EQ(0, m.erase(2));  // Nothing to erase yet

  m.insert(std::make_pair(2, 12));
  m.insert(std::make_pair(1, 11));
  m.insert(std::make_pair(3, 13));
  m.insert(std::make_pair(4, 14));
  ASSERT_EQ(4, m.size());

  // Erase middle two
  EXPECT_EQ(1, m.erase(1));
  EXPECT_EQ(1, m.erase(3));

  EXPECT_EQ(2, m.size());

  // Make sure we can still iterate over everything that's left.
  linked_hash_map<int, int>::iterator it = m.begin();
  ASSERT_TRUE(it != m.end());
  EXPECT_EQ(12, it->second);
  ++it;
  ASSERT_TRUE(it != m.end());
  EXPECT_EQ(14, it->second);
  ++it;
  ASSERT_TRUE(it == m.end());

  EXPECT_EQ(0, m.erase(1));  // Make sure nothing bad happens if we repeat.
  ASSERT_EQ(2, m.size());

  EXPECT_EQ(1, m.erase(2));
  EXPECT_EQ(1, m.erase(4));
  ASSERT_EQ(0, m.size());

  EXPECT_EQ(0, m.erase(1));  // Make sure nothing bad happens if we repeat.
  ASSERT_EQ(0, m.size());
}

// Test that erase(iter,iter) and erase(iter) compile and work.
TEST(LinkedHashMapTest, Erase3) {
  linked_hash_map<int, int> m;

  m.insert(std::make_pair(1, 11));
  m.insert(std::make_pair(2, 12));
  m.insert(std::make_pair(3, 13));
  m.insert(std::make_pair(4, 14));

  // Erase middle two
  linked_hash_map<int, int>::iterator it2 = m.find(2);
  linked_hash_map<int, int>::iterator it4 = m.find(4);
  EXPECT_EQ(m.erase(it2, it4), m.find(4));
  EXPECT_EQ(2, m.size());

  // Make sure we can still iterate over everything that's left.
  linked_hash_map<int, int>::iterator it = m.begin();
  ASSERT_TRUE(it != m.end());
  EXPECT_EQ(11, it->second);
  ++it;
  ASSERT_TRUE(it != m.end());
  EXPECT_EQ(14, it->second);
  ++it;
  ASSERT_TRUE(it == m.end());

  // Erase first one using an iterator.
  EXPECT_EQ(m.erase(m.begin()), m.find(4));

  // Only the last element should be left.
  it = m.begin();
  ASSERT_TRUE(it != m.end());
  EXPECT_EQ(14, it->second);
  ++it;
  ASSERT_TRUE(it == m.end());
}

// Test all types of insertion
TEST(LinkedHashMapTest, Insertion) {
  linked_hash_map<int, int> m;
  ASSERT_EQ(0, m.size());
  std::pair<linked_hash_map<int, int>::iterator, bool> result;

  result = m.insert(std::make_pair(2, 12));
  ASSERT_EQ(1, m.size());
  EXPECT_TRUE(result.second);
  EXPECT_EQ(2, result.first->first);
  EXPECT_EQ(12, result.first->second);

  result = m.insert(std::make_pair(1, 11));
  ASSERT_EQ(2, m.size());
  EXPECT_TRUE(result.second);
  EXPECT_EQ(1, result.first->first);
  EXPECT_EQ(11, result.first->second);

  result = m.insert(std::make_pair(3, 13));
  linked_hash_map<int, int>::iterator result_iterator = result.first;
  ASSERT_EQ(3, m.size());
  EXPECT_TRUE(result.second);
  EXPECT_EQ(3, result.first->first);
  EXPECT_EQ(13, result.first->second);

  result = m.insert(std::make_pair(3, 13));
  EXPECT_EQ(3, m.size());
  EXPECT_FALSE(result.second) << "No insertion should have occurred.";
  EXPECT_TRUE(result_iterator == result.first)
      << "Duplicate insertion should have given us the original iterator.";

  std::vector<std::pair<int, int>> v = {{3, 13}, {4, 14}, {5, 15}};
  m.insert(v.begin(), v.end());
  // Expect 4 and 5 inserted, 3 not inserted.
  EXPECT_EQ(5, m.size());
  EXPECT_EQ(14, m.at(4));
  EXPECT_EQ(15, m.at(5));
}

static std::pair<const int, int> Pair(int i, int j) { return {i, j}; }

// Test front accessors.
TEST(LinkedHashMapTest, Front) {
  linked_hash_map<int, int> m;

  m.insert(std::make_pair(2, 12));
  m.insert(std::make_pair(1, 11));
  m.insert(std::make_pair(3, 13));

  EXPECT_EQ(3, m.size());
  EXPECT_EQ(Pair(2, 12), m.front());
  m.pop_front();
  EXPECT_EQ(2, m.size());
  EXPECT_EQ(Pair(1, 11), m.front());
  m.pop_front();
  EXPECT_EQ(1, m.size());
  EXPECT_EQ(Pair(3, 13), m.front());
  m.pop_front();
  EXPECT_TRUE(m.empty());
}

// Test back accessors.
TEST(LinkedHashMapTest, Back) {
  linked_hash_map<int, int> m;

  m.insert(std::make_pair(2, 12));
  m.insert(std::make_pair(1, 11));
  m.insert(std::make_pair(3, 13));

  EXPECT_EQ(3, m.size());
  EXPECT_EQ(Pair(3, 13), m.back());
  m.pop_back();
  EXPECT_EQ(2, m.size());
  EXPECT_EQ(Pair(1, 11), m.back());
  m.pop_back();
  EXPECT_EQ(1, m.size());
  EXPECT_EQ(Pair(2, 12), m.back());
  m.pop_back();
  EXPECT_TRUE(m.empty());
}

TEST(LinkedHashMapTest, Find) {
  linked_hash_map<int, int> m;

  EXPECT_TRUE(m.end() == m.find(1))
      << "We shouldn't find anything in an empty map.";

  m.insert(std::make_pair(2, 12));
  EXPECT_TRUE(m.end() == m.find(1))
      << "We shouldn't find an element that doesn't exist in the map.";

  std::pair<linked_hash_map<int, int>::iterator, bool> result =
      m.insert(std::make_pair(1, 11));
  ASSERT_TRUE(result.second);
  ASSERT_TRUE(m.end() != result.first);
  EXPECT_TRUE(result.first == m.find(1))
      << "We should have found an element we know exists in the map.";
  EXPECT_EQ(11, result.first->second);

  // Check that a follow-up insertion doesn't affect our original
  m.insert(std::make_pair(3, 13));
  linked_hash_map<int, int>::iterator it = m.find(1);
  ASSERT_TRUE(m.end() != it);
  EXPECT_EQ(11, it->second);

  m.clear();
  EXPECT_TRUE(m.end() == m.find(1))
      << "We shouldn't find anything in a map that we've cleared.";
}

TEST(LinkedHashMapTest, Contains) {
  linked_hash_map<int, int> m;

  EXPECT_FALSE(m.contains(1)) << "An empty map shouldn't contain anything.";

  m.insert(std::make_pair(2, 12));
  EXPECT_FALSE(m.contains(1))
      << "The map shouldn't contain an element that doesn't exist.";

  m.insert(std::make_pair(1, 11));
  EXPECT_TRUE(m.contains(1))
      << "The map should contain an element that we know exists.";

  m.clear();
  EXPECT_FALSE(m.contains(1))
      << "A map that we've cleared shouldn't contain anything.";
}

TEST(LinkedHashMapTest, At) {
  linked_hash_map<int, int> m = {{1, 2}};
  EXPECT_EQ(2, m.at(1));
  ABSL_BASE_INTERNAL_EXPECT_FAIL(m.at(3), std::out_of_range,
                                 "linked_hash_map::at");

  const linked_hash_map<int, int> cm = {{1, 2}};
  EXPECT_EQ(2, cm.at(1));
  ABSL_BASE_INTERNAL_EXPECT_FAIL(cm.at(3), std::out_of_range,
                                 "linked_hash_map::at");
}

TEST(LinkedHashMapTest, Swap) {
  linked_hash_map<int, int> m1;
  linked_hash_map<int, int> m2;
  m1.insert(std::make_pair(1, 1));
  m1.insert(std::make_pair(2, 2));
  m2.insert(std::make_pair(3, 3));
  ASSERT_EQ(2, m1.size());
  ASSERT_EQ(1, m2.size());
  m1.swap(m2);
  ASSERT_EQ(1, m1.size());
  ASSERT_EQ(2, m2.size());
}

TEST(LinkedHashMapTest, SelfSwap) {
  linked_hash_map<int, int> a{{1, 1}, {2, 2}, {3, 3}};
  using std::swap;
  swap(a, a);
  EXPECT_THAT(a, ElementsAre(Pair(1, 1), Pair(2, 2), Pair(3, 3)));
}

TEST(LinkedHashMapTest, InitializerList) {
  linked_hash_map<int, int> m{{1, 2}, {3, 4}};
  ASSERT_EQ(2, m.size());
  EXPECT_TRUE(m.contains(1));
  linked_hash_map<int, int>::iterator it = m.find(1);
  ASSERT_TRUE(m.end() != it);
  EXPECT_EQ(2, it->second);
  EXPECT_TRUE(m.contains(3));
  it = m.find(3);
  ASSERT_TRUE(m.end() != it);
  EXPECT_EQ(4, it->second);
}

TEST(LinkedHashMapTest, CustomHashAndEquality) {
  struct CustomIntHash {
    size_t operator()(int x) const { return x; }
  };
  struct CustomIntEq {
    bool operator()(int x, int y) const { return x == y; }
  };
  linked_hash_map<int, int, CustomIntHash, CustomIntEq> m;
  m.insert(std::make_pair(1, 1));
  EXPECT_TRUE(m.contains(1));
  EXPECT_EQ(1, m[1]);
}

TEST(LinkedHashMapTest, EqualRange) {
  linked_hash_map<int, int> m{{3, 11}, {1, 13}};
  const auto& const_m = m;

  EXPECT_THAT(m.equal_range(2), testing::Pair(m.end(), m.end()));
  EXPECT_THAT(const_m.equal_range(2),
              testing::Pair(const_m.end(), const_m.end()));

  EXPECT_THAT(m.equal_range(1), testing::Pair(m.find(1), ++m.find(1)));
  EXPECT_THAT(const_m.equal_range(1),
              testing::Pair(const_m.find(1), ++const_m.find(1)));
}

TEST(LinkedHashMapTest, ReserveWorks) {
  linked_hash_map<int, int> m;
  EXPECT_EQ(0, m.size());
  EXPECT_EQ(0.0, m.load_factor());
  m.reserve(10);
  EXPECT_LE(10, m.capacity());
  EXPECT_EQ(0, m.size());
  EXPECT_EQ(0.0, m.load_factor());
  m.emplace(1, 1);
  m.emplace(2, 2);
  EXPECT_LE(10, m.capacity());
  EXPECT_EQ(2, m.size());
  EXPECT_LT(0.0, m.load_factor());
}

// We require key types to have hash and equals.
class CopyableMovableType
    : public absl::test_internal::CopyableMovableInstance {
 public:
  using CopyableMovableInstance::CopyableMovableInstance;

  bool operator==(const CopyableMovableType& o) const {
    return value() == o.value();
  }

  template <typename H>
  friend H AbslHashValue(H h, const CopyableMovableType& c) {
    return H::combine(std::move(h), c.value());
  }
};

TEST(LinkedHashMapTest, RValueOperatorAt) {
  absl::test_internal::InstanceTracker tracker;

  linked_hash_map<CopyableMovableType, int> map;
  map[CopyableMovableType(1)] = 1;
  EXPECT_EQ(tracker.copies(), 0);
  CopyableMovableType c(2);
  map[std::move(c)] = 2;
  EXPECT_EQ(tracker.copies(), 0);
  EXPECT_THAT(map, ElementsAre(Pair(CopyableMovableType(1), 1),
                               Pair(CopyableMovableType(2), 2)));
}

TEST(LinkedHashMapTest, HeterogeneousTests) {
  absl::test_internal::InstanceTracker tracker;

  using MapType = linked_hash_map<ExpensiveType, int, HeterogeneousHash,
                                  HeterogeneousEqual>;
  MapType map;
  ExpensiveType one(1);
  map[one] = 1;
  // Two instances: 'one' var and an instance in the map.
  EXPECT_EQ(2, tracker.instances());
  EXPECT_EQ(1, tracker.copies());
  EXPECT_EQ(0, tracker.moves());

  tracker.ResetCopiesMovesSwaps();
  map[one] = 5;
  // No construction since key==1 exists.
  EXPECT_EQ(2, tracker.instances());
  EXPECT_EQ(0, tracker.copies());
  EXPECT_EQ(0, tracker.moves());

  tracker.ResetCopiesMovesSwaps();
  map[CheapType(1)] = 10;
  // No construction since key==1 exists.
  EXPECT_EQ(2, tracker.instances());
  EXPECT_EQ(0, tracker.copies());
  EXPECT_EQ(0, tracker.moves());

  tracker.ResetCopiesMovesSwaps();
  map[CheapType(2)] = 20;
  // Construction since key==2 doesn't exist in the map.
  EXPECT_EQ(3, tracker.instances());
  EXPECT_EQ(0, tracker.copies());
  EXPECT_EQ(0, tracker.moves());
  EXPECT_THAT(map, ElementsAre(Pair(HasExpensiveValue(1), 10),
                               Pair(HasExpensiveValue(2), 20)));

  // find
  auto itr = map.find(CheapType(1));
  tracker.ResetCopiesMovesSwaps();
  ASSERT_NE(itr, map.end());
  EXPECT_EQ(10, itr->second);
  // contains
  EXPECT_TRUE(map.contains(CheapType(2)));
  // count
  EXPECT_EQ(1, map.count(CheapType(2)));
  // equal_range
  auto eq_itr_pair = map.equal_range(CheapType(2));
  ASSERT_NE(eq_itr_pair.first, map.end());
  EXPECT_EQ(20, eq_itr_pair.first->second);
  // No construction for find, contains, count or equal_range.
  EXPECT_EQ(3, tracker.instances());
  EXPECT_EQ(0, tracker.copies());
  EXPECT_EQ(0, tracker.moves());

  // insert
  auto three = MapType::value_type(CheapType(3), 30);
  tracker.ResetCopiesMovesSwaps();
  map.insert(three);
  // Two instances: 'three' var and an instance in the map.
  EXPECT_EQ(5, tracker.instances());
  EXPECT_EQ(1, tracker.copies());
  EXPECT_EQ(0, tracker.moves());

  tracker.ResetCopiesMovesSwaps();
  map.insert(three);
  // No additional construction since key==3 exists.
  EXPECT_EQ(5, tracker.instances());
  EXPECT_EQ(0, tracker.copies());
  EXPECT_EQ(0, tracker.moves());
  EXPECT_THAT(map, ElementsAre(Pair(HasExpensiveValue(1), 10),
                               Pair(HasExpensiveValue(2), 20),
                               Pair(HasExpensiveValue(3), 30)));

  // Test std::move() using operator[] and insert().
  ExpensiveType four(4);
  tracker.ResetCopiesMovesSwaps();
  map[std::move(four)] = 40;
  // Two constructions (regular and move).
  EXPECT_EQ(7, tracker.instances());
  EXPECT_EQ(0, tracker.copies());
  EXPECT_EQ(1, tracker.moves());

  auto five = MapType::value_type(CheapType(5), 50);
  tracker.ResetCopiesMovesSwaps();
  map.insert(std::move(five));
  // Two constructions (regular and move).
  EXPECT_EQ(9, tracker.instances());
  EXPECT_EQ(1, tracker.copies());
  EXPECT_EQ(0, tracker.moves());
  EXPECT_THAT(map, ElementsAre(Pair(HasExpensiveValue(1), 10),
                               Pair(HasExpensiveValue(2), 20),
                               Pair(HasExpensiveValue(3), 30),
                               Pair(HasExpensiveValue(4), 40),
                               Pair(HasExpensiveValue(5), 50)));

  // Insert using std::pair.
  tracker.ResetCopiesMovesSwaps();
  map.insert(std::make_pair(ExpensiveType(6), 60));
  EXPECT_EQ(10, tracker.instances());
  EXPECT_EQ(1, tracker.copies());
  EXPECT_EQ(2, tracker.moves());

  // Heterogeneous erase.
  map.erase(CheapType(1));
  // No construction and instance reduced by one.
  tracker.ResetCopiesMovesSwaps();
  EXPECT_EQ(9, tracker.instances());
  EXPECT_EQ(0, tracker.copies());
  EXPECT_EQ(0, tracker.moves());
  EXPECT_THAT(map, ElementsAre(Pair(HasExpensiveValue(2), 20),
                               Pair(HasExpensiveValue(3), 30),
                               Pair(HasExpensiveValue(4), 40),
                               Pair(HasExpensiveValue(5), 50),
                               Pair(HasExpensiveValue(6), 60)));
}

TEST(LinkedHashMapTest, HeterogeneousStringViewLookup) {
  linked_hash_map<std::string, int> map;
  map["foo"] = 1;
  map["bar"] = 2;
  map["blah"] = 3;

  {
    absl::string_view lookup("foo");
    auto itr = map.find(lookup);
    ASSERT_NE(itr, map.end());
    EXPECT_EQ(1, itr->second);
  }

  // Not found.
  {
    absl::string_view lookup("foobar");
    EXPECT_EQ(map.end(), map.find(lookup));
  }

  {
    absl::string_view lookup("blah");
    auto itr = map.find(lookup);
    ASSERT_NE(itr, map.end());
    EXPECT_EQ(3, itr->second);
  }
}

TEST(LinkedHashMapTest, UniversalReferenceWorks) {
  linked_hash_map<std::string, int> map;
  std::string s = "very very very very very long string";
  map[s] = 1;
  EXPECT_EQ(s, "very very very very very long string");
}

TEST(LinkedHashMap, ExtractInsert) {
  linked_hash_map<int, int> m = {{1, 7}, {2, 9}};
  auto node = m.extract(1);
  EXPECT_TRUE(node);
  EXPECT_EQ(node.key(), 1);
  EXPECT_EQ(node.mapped(), 7);
  EXPECT_THAT(m, ElementsAre(Pair(2, 9)));
  EXPECT_FALSE(m.contains(1));
  EXPECT_TRUE(m.contains(2));

  node.mapped() = 17;
  m.insert(std::move(node));
  EXPECT_FALSE(node);
  EXPECT_THAT(m, ElementsAre(Pair(2, 9), Pair(1, 17)));
  EXPECT_TRUE(m.contains(2));
  EXPECT_TRUE(m.contains(1));

  node = m.extract(m.find(1));
  EXPECT_TRUE(node);
  EXPECT_EQ(node.key(), 1);
  EXPECT_EQ(node.mapped(), 17);
  EXPECT_THAT(m, ElementsAre(Pair(2, 9)));
}

TEST(LinkedHashMap, Merge) {
  linked_hash_map<int, int> m = {{1, 7}, {3, 6}};
  linked_hash_map<int, int> src = {{1, 10}, {2, 9}, {4, 16}};

  m.merge(src);

  EXPECT_THAT(m, ElementsAre(Pair(1, 7), Pair(3, 6), Pair(2, 9), Pair(4, 16)));
  EXPECT_THAT(src, ElementsAre(Pair(1, 10)));
  for (int i : {2, 9, 4}) {
    EXPECT_FALSE(src.contains(i));
  }
}

TEST(LinkedHashMap, Splice) {
  linked_hash_map<int, int> m = {{1, 7}, {3, 6}};
  linked_hash_map<int, int> src = {{1, 10}, {2, 9}, {4, 16}};
  m.splice(m.end(), m, m.begin());

  EXPECT_THAT(m, ElementsAre(Pair(3, 6), Pair(1, 7)));

  m.splice(m.end(), src, ++src.begin());
  EXPECT_THAT(m, ElementsAre(Pair(3, 6), Pair(1, 7), Pair(2, 9)));
  EXPECT_THAT(src, ElementsAre(Pair(1, 10), Pair(4, 16)));
}

TEST(LinkedHashMap, EraseRange) {
  linked_hash_map<int, int> map = {{1, 1},  {2, 4},  {3, 9},  {4, 16}, {5, 25},
                                   {6, 36}, {7, 49}, {8, 64}, {9, 81}};
  auto start = map.find(3);
  auto end = map.find(8);
  auto itr = map.erase(start, end);
  ASSERT_NE(itr, map.end());
  EXPECT_THAT(*itr, Pair(8, 64));
  EXPECT_THAT(map,
              ElementsAre(Pair(1, 1), Pair(2, 4), Pair(8, 64), Pair(9, 81)));
  for (int i : {1, 2, 8, 9}) {
    EXPECT_TRUE(map.contains(i));
  }
  for (int i : {3, 4, 5, 6, 7}) {
    EXPECT_FALSE(map.contains(i));
  }
}

TEST(LinkedHashMap, InsertInitializerList) {
  linked_hash_map<int, int> m;
  m.insert({{1, 7}, {2, 9}, {3, 29}});
  EXPECT_THAT(m, ElementsAre(Pair(1, 7), Pair(2, 9), Pair(3, 29)));
}

TEST(LinkedHashMap, InsertOrAssign) {
  linked_hash_map<int, int> m;
  {
    auto [itr, elem_inserted] = m.insert_or_assign(10, 20);
    EXPECT_THAT(*itr, Pair(10, 20));
    EXPECT_EQ(elem_inserted, true);
  }
  {
    auto [itr, elem_inserted] = m.insert_or_assign(10, 30);
    EXPECT_THAT(*itr, Pair(10, 30));
    EXPECT_EQ(elem_inserted, false);
  }
}

TEST(LinkedHashMap, TryEmplace) {
  linked_hash_map<int, std::pair<int, std::string>> m;
  {
    auto [itr, elem_inserted] = m.try_emplace(10, 20, "string");
    EXPECT_THAT(*itr, Pair(10, Pair(20, "string")));
    EXPECT_EQ(elem_inserted, true);
  }
  {
    absl::test_internal::InstanceTracker tracker;
    std::string s = "some string";
    auto [itr, elem_inserted] = m.try_emplace(10, 2, std::move(s));
    EXPECT_THAT(*itr, Pair(10, Pair(20, "string")));
    EXPECT_EQ(elem_inserted, false);
    EXPECT_THAT(tracker.moves(), 0);
  }
}

TEST(LinkedHashMapTest, TryEmplace) {
  linked_hash_map<int, std::tuple<int, std::string, std::unique_ptr<float>>>
      map;
  auto result = map.try_emplace(3, 2, "foo", new float(1.0));
  EXPECT_TRUE(result.second);
  EXPECT_EQ(std::get<0>(result.first->second), 2);
  EXPECT_EQ(std::get<1>(result.first->second), "foo");
  EXPECT_EQ(*std::get<2>(result.first->second), 1.0);

  // Ptr should not be moved.
  auto ptr = std::make_unique<float>(3.0);
  auto result2 = map.try_emplace(3, 22, "foo-bar", std::move(ptr));
  EXPECT_FALSE(result2.second);
  EXPECT_EQ(std::get<0>(result.first->second), 2);
  EXPECT_EQ(std::get<1>(result.first->second), "foo");
  EXPECT_EQ(*std::get<2>(result.first->second), 1.0);
  EXPECT_NE(ptr.get(), nullptr);
}

struct CountedHash {
  explicit CountedHash(int* count) : count(count) {}
  size_t operator()(int value) const {
    ++(*count);
    return value;
  }
  int* count = nullptr;
};

// Makes a map too big for small object optimization.  Counts the number of
// hashes in `count`, but leaves `count` set to 0.
linked_hash_map<int, std::string, CountedHash> MakeNonSmallMap(int* count) {
  const int kFirstKey = -1000;
  linked_hash_map<int, std::string, CountedHash> m(0, CountedHash(count));
  for (int i = kFirstKey; i < kFirstKey + 100; ++i) {
    m[i] = "foo";
  }
  *count = 0;
  return m;
}

constexpr bool BuildHasDebugModeRehashes() {
#if !defined(NDEBUG) || defined(ABSL_HAVE_ADDRESS_SANITIZER) || \
    defined(ABSL_HAVE_MEMORY_SANITIZER) || defined(ABSL_HAVE_THREAD_SANITIZER)
  return true;
#else
  return false;
#endif
}

TEST(LinkedHashMapTest, HashCountInOptBuilds) {
  if (BuildHasDebugModeRehashes()) {
    GTEST_SKIP() << "Only run under NDEBUG: `assert` statements and sanitizer "
                    "rehashing may cause redundant hashing.";
  }

  using Map = linked_hash_map<int, std::string, CountedHash>;
  {
    int count = 0;
    Map m = MakeNonSmallMap(&count);
    m.insert({1, "foo"});
    EXPECT_EQ(count, 1);
    m.erase(1);
    EXPECT_EQ(count, 2);
  }
  {
    int count = 0;
    Map m = MakeNonSmallMap(&count);
    m[2] = "bar";
    EXPECT_EQ(count, 1);
  }
  {
    int count = 0;
    Map m = MakeNonSmallMap(&count);
    m.insert({3, "baz"});
    EXPECT_EQ(count, 1);
    auto node = m.extract(3);
    EXPECT_EQ(count, 2);
    m.insert(std::move(node));
    EXPECT_EQ(count, 3);
  }
  {
    int count = 0;
    Map m = MakeNonSmallMap(&count);
    m.insert_or_assign(4, "qux");
    EXPECT_EQ(count, 1);
  }
  {
    int count = 0;
    Map m = MakeNonSmallMap(&count);
    m.emplace(5, "vog");
    EXPECT_EQ(count, 1);
  }
  {
    int count = 0;
    Map m = MakeNonSmallMap(&count);
    m.try_emplace(6, 'x', 42);
    EXPECT_EQ(count, 1);
  }
  {
    int src_count = 0, dst_count = 0;
    Map src = MakeNonSmallMap(&src_count);
    Map dst = MakeNonSmallMap(&dst_count);
    src[7] = "siete";
    dst.merge(src);
    EXPECT_LE(src_count, 200);
    EXPECT_LE(dst_count, 200);
  }
}

}  // namespace
}  // namespace container_internal
ABSL_NAMESPACE_END
}  // namespace absl
