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

#include "absl/container/linked_hash_set.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/container/internal/hash_generator_testing.h"
#include "absl/container/internal/hash_policy_testing.h"
#include "absl/container/internal/heterogeneous_lookup_testing.h"
#include "absl/container/internal/test_instance_tracker.h"
#include "absl/container/internal/unordered_set_constructor_test.h"
#include "absl/container/internal/unordered_set_lookup_test.h"
#include "absl/container/internal/unordered_set_members_test.h"
#include "absl/container/internal/unordered_set_modifiers_test.h"
#include "absl/strings/string_view.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace container_internal {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Pointee;

template <class T>
using Set =
    linked_hash_set<T, StatefulTestingHash, StatefulTestingEqual, Alloc<T>>;

using SetTypes =
    ::testing::Types<Set<int>, Set<std::string>, Set<Enum>, Set<EnumClass>>;

INSTANTIATE_TYPED_TEST_SUITE_P(LinkedHashSet, ConstructorTest, SetTypes);
INSTANTIATE_TYPED_TEST_SUITE_P(LinkedHashSet, LookupTest, SetTypes);
INSTANTIATE_TYPED_TEST_SUITE_P(LinkedHashSet, MembersTest, SetTypes);
INSTANTIATE_TYPED_TEST_SUITE_P(LinkedHashSet, ModifiersTest, SetTypes);

// Tests that the range constructor works.
TEST(LinkedHashSetTest, RangeConstruct) {
  const auto items = {1, 2, 3};
  EXPECT_THAT(linked_hash_set<int>(items.begin(), items.end()),
              ElementsAre(1, 2, 3));
}

// Tests that copying works.
TEST(LinkedHashSetTest, Copy) {
  linked_hash_set<int> m{4, 8, 15, 16, 23, 42};
  auto copy = m;

  auto found = copy.find(8);
  ASSERT_TRUE(found != copy.end());
  for (auto iter = copy.begin(); iter != copy.end(); ++iter) {
    if (iter == found) return;
  }
  FAIL() << "Copied set's find method returned an invalid iterator.";
}

// Tests that assignment works.
TEST(LinkedHashSetTest, Assign) {
  linked_hash_set<int> m{2, 3};
  linked_hash_set<int> n{4};

  n = m;
  EXPECT_TRUE(n.contains(2));
  auto found = n.find(2);
  ASSERT_TRUE(found != n.end());
  for (auto iter = n.begin(); iter != n.end(); ++iter) {
    if (iter == found) return;
  }
  FAIL() << "Assigned set's find method returned an invalid iterator.";
}

// Tests that self-assignment works.
TEST(LinkedHashSetTest, SelfAssign) {
  linked_hash_set<int> a{1, 2, 3};
  auto& a_ref = a;
  a = a_ref;

  EXPECT_TRUE(a.contains(2));
  auto found = a.find(2);
  ASSERT_TRUE(found != a.end());
  for (auto iter = a.begin(); iter != a.end(); ++iter) {
    if (iter == found) return;
  }
  FAIL() << "Assigned set's find method returned an invalid iterator.";
}

// Tests that move constructor works.
TEST(LinkedHashSetTest, Move) {
  // Use unique_ptr as an example of a non-copyable type.
  linked_hash_set<std::unique_ptr<int>> m;
  m.insert(std::make_unique<int>(2));
  m.insert(std::make_unique<int>(3));
  linked_hash_set<std::unique_ptr<int>> n = std::move(m);
  EXPECT_THAT(n, ElementsAre(Pointee(2), Pointee(3)));
}

// Tests that self-moving works.
TEST(LinkedHashSetTest, SelfMove) {
  linked_hash_set<int> a{1, 2, 3};
  auto& a_ref = a;
  a = std::move(a_ref);
  EXPECT_THAT(a, ElementsAre(1, 2, 3));
}

struct IntUniquePtrHash {
  size_t operator()(const std::unique_ptr<int>& p) const {
    return static_cast<size_t>(*p);
  }
};

struct IntUniquePtrEq {
  size_t operator()(const std::unique_ptr<int>& a,
                    const std::unique_ptr<int>& b) const {
    return *a == *b;
  }
};

// Pretty artificial for a set, but unique_ptr is a convenient move-only type.
TEST(LinkedHashSetTest, CanInsertMoveOnly) {
  linked_hash_set<std::unique_ptr<int>, IntUniquePtrHash, IntUniquePtrEq> s;
  std::vector<int> data = {4, 8, 15, 16, 23, 42};
  for (int x : data) s.insert(std::make_unique<int>(x));
  EXPECT_EQ(s.size(), data.size());
  for (const std::unique_ptr<int>& elt : s) {
    EXPECT_TRUE(s.contains(elt));
    EXPECT_TRUE(s.find(elt) != s.end());
  }
}

TEST(LinkedHashSetTest, CanMoveMoveOnly) {
  linked_hash_set<std::unique_ptr<int>, IntUniquePtrHash, IntUniquePtrEq> s;
  std::vector<int> data = {4, 8, 15, 16, 23, 42};
  for (int x : data) s.insert(std::make_unique<int>(x));
  linked_hash_set<std::unique_ptr<int>, IntUniquePtrHash, IntUniquePtrEq> ss =
      std::move(s);
  EXPECT_EQ(ss.size(), data.size());
}

TEST(LinkedHashSetTest, CanEmplaceMoveOnly) {
  linked_hash_set<std::unique_ptr<int>, IntUniquePtrHash, IntUniquePtrEq> s;
  std::vector<int> data = {4, 8, 15, 16, 23, 42};
  for (const int x : data) {
    s.emplace(new int{x});
  }
  EXPECT_EQ(s.size(), data.size());
  for (const std::unique_ptr<int>& elt : s) {
    EXPECT_TRUE(s.contains(elt));
    EXPECT_TRUE(s.find(elt) != s.end());
  }
}

TEST(LinkedHashSetTest, CanInsertTransparent) {
  linked_hash_set<std::string> s;
  s.insert(absl::string_view("foo"));
  s.insert(absl::string_view("bar"));
  s.insert(absl::string_view("foo"));
  EXPECT_THAT(s, ElementsAre("foo", "bar"));
}

// Tests that iteration from begin() to end() works
TEST(LinkedHashSetTest, Iteration) {
  linked_hash_set<int> m;
  EXPECT_TRUE(m.begin() == m.end());

  m.insert(2);
  m.insert(1);
  m.insert(3);

  linked_hash_set<int>::iterator i = m.begin();
  ASSERT_TRUE(m.begin() == i);
  ASSERT_TRUE(m.end() != i);
  EXPECT_EQ(2, *i);

  ++i;
  ASSERT_TRUE(m.end() != i);
  EXPECT_EQ(1, *i);

  ++i;
  ASSERT_TRUE(m.end() != i);
  EXPECT_EQ(3, *i);

  ++i;
  ASSERT_TRUE(m.end() == i);
}

// Tests that reverse iteration from rbegin() to rend() works
TEST(LinkedHashSetTest, ReverseIteration) {
  linked_hash_set<int> m;
  EXPECT_TRUE(m.rbegin() == m.rend());

  m.insert(2);
  m.insert(1);
  m.insert(3);

  linked_hash_set<int>::reverse_iterator i = m.rbegin();
  ASSERT_TRUE(m.rbegin() == i);
  ASSERT_TRUE(m.rend() != i);
  EXPECT_EQ(3, *i);

  ++i;
  ASSERT_TRUE(m.rend() != i);
  EXPECT_EQ(1, *i);

  ++i;
  ASSERT_TRUE(m.rend() != i);
  EXPECT_EQ(2, *i);

  ++i;
  ASSERT_TRUE(m.rend() == i);
}

// Tests that clear() works
TEST(LinkedHashSetTest, Clear) {
  linked_hash_set<int> m{2, 1, 3};
  ASSERT_EQ(3, m.size());

  m.clear();
  EXPECT_EQ(0, m.size());
  EXPECT_FALSE(m.contains(1));
  EXPECT_TRUE(m.find(1) == m.end());

  // Make sure we can call it on an empty set.
  m.clear();
  EXPECT_EQ(0, m.size());
}

// Tests that size() works.
TEST(LinkedHashSetTest, Size) {
  linked_hash_set<int> m;
  EXPECT_EQ(0, m.size());
  m.insert(2);
  EXPECT_EQ(1, m.size());
  m.insert(11);
  EXPECT_EQ(2, m.size());
  m.insert(0);
  EXPECT_EQ(3, m.size());
  m.insert(0);
  EXPECT_EQ(3, m.size());
  m.clear();
  EXPECT_EQ(0, m.size());
}

// Tests empty()
TEST(LinkedHashSetTest, Empty) {
  linked_hash_set<int> m;
  ASSERT_TRUE(m.empty());
  m.insert(2);
  ASSERT_FALSE(m.empty());
  m.clear();
  ASSERT_TRUE(m.empty());
}

TEST(LinkedHashSetTest, Erase) {
  linked_hash_set<int> m;
  ASSERT_EQ(0, m.size());
  EXPECT_EQ(0, m.erase(2));  // Nothing to erase yet

  m.insert(2);
  ASSERT_EQ(1, m.size());
  EXPECT_EQ(1, m.erase(2));
  EXPECT_EQ(0, m.size());
  EXPECT_TRUE(m.empty());

  EXPECT_EQ(0, m.erase(2));  // Make sure nothing bad happens if we repeat.
  EXPECT_EQ(0, m.size());
  EXPECT_TRUE(m.empty());
}

TEST(LinkedHashSetTest, Erase2) {
  linked_hash_set<int> m;
  ASSERT_EQ(0, m.size());
  EXPECT_EQ(0, m.erase(2));  // Nothing to erase yet

  m.insert(2);
  m.insert(1);
  m.insert(3);
  m.insert(4);
  ASSERT_EQ(4, m.size());

  // Erase middle two
  EXPECT_EQ(1, m.erase(1));
  EXPECT_EQ(1, m.erase(3));

  EXPECT_EQ(2, m.size());

  // Make sure we can still iterate over everything that's left.
  linked_hash_set<int>::iterator it = m.begin();
  ASSERT_TRUE(it != m.end());
  EXPECT_EQ(2, *it);
  ++it;
  ASSERT_TRUE(it != m.end());
  EXPECT_EQ(4, *it);
  ++it;
  ASSERT_TRUE(it == m.end());

  EXPECT_EQ(0, m.erase(1));  // Make sure nothing bad happens if we repeat.
  ASSERT_EQ(2, m.size());

  EXPECT_EQ(1, m.erase(2));
  EXPECT_EQ(1, m.erase(4));
  ASSERT_EQ(0, m.size());
  EXPECT_TRUE(m.empty());

  EXPECT_EQ(0, m.erase(1));  // Make sure nothing bad happens if we repeat.
  ASSERT_EQ(0, m.size());
  EXPECT_TRUE(m.empty());
}

// Test that erase(iter,iter) and erase(iter) compile and work.
TEST(LinkedHashSetTest, Erase3) {
  linked_hash_set<int> m;

  m.insert(1);
  m.insert(2);
  m.insert(3);
  m.insert(4);

  // Erase middle two
  linked_hash_set<int>::iterator it2 = m.find(2);
  linked_hash_set<int>::iterator it4 = m.find(4);
  EXPECT_EQ(m.erase(it2, it4), m.find(4));
  EXPECT_FALSE(m.contains(2));
  EXPECT_TRUE(m.find(2) == m.end());
  EXPECT_FALSE(m.contains(3));
  EXPECT_TRUE(m.find(3) == m.end());
  EXPECT_EQ(2, m.size());

  // Make sure we can still iterate over everything that's left.
  linked_hash_set<int>::iterator it = m.begin();
  ASSERT_TRUE(it != m.end());
  EXPECT_EQ(1, *it);
  ++it;
  ASSERT_TRUE(it != m.end());
  EXPECT_EQ(4, *it);
  ++it;
  ASSERT_TRUE(it == m.end());

  // Erase first one using an iterator.
  EXPECT_EQ(m.erase(m.begin()), m.find(4));
  EXPECT_FALSE(m.contains(1));
  EXPECT_TRUE(m.find(1) == m.end());

  // Only the last element should be left.
  EXPECT_TRUE(m.contains(4));
  it = m.begin();
  ASSERT_TRUE(it != m.end());
  EXPECT_EQ(4, *it);
  ++it;
  ASSERT_TRUE(it == m.end());
}

// Test all types of insertion
TEST(LinkedHashSetTest, Insertion) {
  linked_hash_set<int> m;
  ASSERT_EQ(0, m.size());
  std::pair<linked_hash_set<int>::iterator, bool> result;

  result = m.insert(2);
  ASSERT_EQ(1, m.size());
  EXPECT_TRUE(result.second);
  EXPECT_EQ(2, *result.first);
  EXPECT_TRUE(m.contains(2));
  EXPECT_TRUE(m.find(2) != m.end());

  result = m.insert(1);
  ASSERT_EQ(2, m.size());
  EXPECT_TRUE(result.second);
  EXPECT_EQ(1, *result.first);
  EXPECT_TRUE(m.contains(1));
  EXPECT_TRUE(m.find(1) != m.end());

  result = m.insert(3);
  linked_hash_set<int>::iterator result_iterator = result.first;
  ASSERT_EQ(3, m.size());
  EXPECT_TRUE(result.second);
  EXPECT_EQ(3, *result.first);
  EXPECT_TRUE(m.contains(3));
  EXPECT_TRUE(m.find(3) != m.end());

  result = m.insert(3);
  EXPECT_EQ(3, m.size());
  EXPECT_FALSE(result.second) << "No insertion should have occurred.";
  EXPECT_TRUE(result_iterator == result.first)
      << "Duplicate insertion should have given us the original iterator.";
  EXPECT_TRUE(m.contains(3));
  EXPECT_TRUE(m.find(3) != m.end());

  std::vector<int> v = {3, 4, 5};
  m.insert(v.begin(), v.end());
  // Expect 4 and 5 inserted, 3 not inserted.
  EXPECT_EQ(5, m.size());
  EXPECT_TRUE(m.contains(4));
  EXPECT_NE(m.find(4), m.end());
  EXPECT_TRUE(m.contains(5));
  EXPECT_NE(m.find(5), m.end());
}

TEST(LinkedHashSetTest, HintedInsertionMoveable) {
  linked_hash_set<int> m = {1, 3};
  m.insert(m.find(3), 2);
  EXPECT_THAT(m, ElementsAre(1, 2, 3));
}

TEST(LinkedHashSetTest, HintedInsertionReference) {
  linked_hash_set<int> m = {1, 3};
  const int val = 2;
  m.insert(m.find(3), val);
  EXPECT_THAT(m, ElementsAre(1, 2, 3));
}

TEST(LinkedHashSetTest, HintedEmplaceMoveable) {
  linked_hash_set<int> m = {1, 3};
  m.emplace_hint(m.find(3), 2);
  EXPECT_THAT(m, ElementsAre(1, 2, 3));
}

TEST(LinkedHashSetTest, HintedEmplaceReference) {
  linked_hash_set<int> m = {1, 3};
  const int val = 2;
  m.emplace_hint(m.find(3), val);
  EXPECT_THAT(m, ElementsAre(1, 2, 3));
}

// Test front accessors.
TEST(LinkedHashSetTest, Front) {
  linked_hash_set<int> m;

  m.insert(222);
  m.insert(111);
  m.insert(333);

  EXPECT_EQ(3, m.size());
  EXPECT_EQ(222, m.front());
  m.pop_front();
  EXPECT_EQ(2, m.size());
  EXPECT_EQ(111, m.front());
  m.pop_front();
  EXPECT_EQ(1, m.size());
  EXPECT_EQ(333, m.front());
  m.pop_front();
  EXPECT_TRUE(m.empty());
}

// Test back accessors.
TEST(LinkedHashSetTest, Back) {
  linked_hash_set<int> m;

  m.insert(222);
  m.insert(111);
  m.insert(333);

  EXPECT_EQ(3, m.size());
  EXPECT_EQ(333, m.back());
  m.pop_back();
  EXPECT_EQ(2, m.size());
  EXPECT_EQ(111, m.back());
  m.pop_back();
  EXPECT_EQ(1, m.size());
  EXPECT_EQ(222, m.back());
  m.pop_back();
  EXPECT_TRUE(m.empty());
}

TEST(LinkedHashSetTest, Find) {
  linked_hash_set<int> m;

  EXPECT_TRUE(m.end() == m.find(1))
      << "We shouldn't find anything in an empty set.";

  m.insert(2);
  EXPECT_TRUE(m.end() == m.find(1))
      << "We shouldn't find an element that doesn't exist in the set.";

  std::pair<linked_hash_set<int>::iterator, bool> result = m.insert(1);
  ASSERT_TRUE(result.second);
  ASSERT_TRUE(m.end() != result.first);
  EXPECT_TRUE(result.first == m.find(1))
      << "We should have found an element we know exists in the set.";
  EXPECT_EQ(1, *result.first);

  // Check that a follow-up insertion doesn't affect our original
  m.insert(3);
  linked_hash_set<int>::iterator it = m.find(1);
  ASSERT_TRUE(m.end() != it);
  EXPECT_EQ(1, *it);

  m.clear();
  EXPECT_TRUE(m.end() == m.find(1))
      << "We shouldn't find anything in a set that we've cleared.";
}

TEST(LinkedHashSetTest, Contains) {
  linked_hash_set<int> m;

  EXPECT_FALSE(m.contains(1)) << "The empty set shouldn't contain anything.";

  m.insert(2);
  EXPECT_FALSE(m.contains(1))
      << "contains() should not return true for an element that doesn't exist "
      << "in the set.";

  m.insert(1);
  EXPECT_TRUE(m.contains(1))
      << "contains() should return true for an element we know exists in the "
      << "set.";

  m.clear();
  EXPECT_FALSE(m.contains(1))
      << "A set that we've cleared shouldn't contain anything.";
}

TEST(LinkedHashSetTest, Swap) {
  linked_hash_set<int> m1;
  linked_hash_set<int> m2;
  m1.insert(1);
  m1.insert(2);
  m2.insert(3);
  ASSERT_EQ(2, m1.size());
  ASSERT_EQ(1, m2.size());
  m1.swap(m2);
  ASSERT_EQ(1, m1.size());
  ASSERT_EQ(2, m2.size());
}

TEST(LinkedHashSetTest, SelfSwap) {
  linked_hash_set<int> a{1, 2, 3};
  using std::swap;
  swap(a, a);
  EXPECT_THAT(a, ElementsAre(1, 2, 3));
}

TEST(LinkedHashSetTest, InitializerList) {
  linked_hash_set<int> m{1, 3};
  ASSERT_EQ(2, m.size());
  EXPECT_TRUE(m.contains(1));
  linked_hash_set<int>::iterator it = m.find(1);
  ASSERT_TRUE(m.end() != it);
  EXPECT_EQ(1, *it);
  it = m.find(3);
  EXPECT_TRUE(m.contains(3));
  ASSERT_TRUE(m.end() != it);
  EXPECT_EQ(3, *it);
}

TEST(LinkedHashSetTest, CustomHashAndEquality) {
  struct CustomIntHash {
    size_t operator()(int x) const { return 0; }
  };
  struct CustomIntEq {
    bool operator()(int x, int y) const { return abs(x) == abs(y); }
  };
  linked_hash_set<int, CustomIntHash, CustomIntEq> m;
  m.insert(1);
  EXPECT_EQ(1, m.size());
  m.insert(2);
  EXPECT_EQ(2, m.size());
  EXPECT_FALSE(m.insert(-2).second);
  EXPECT_EQ(2, m.size());
  EXPECT_TRUE(m.contains(-1));
  EXPECT_TRUE(m.find(-1) != m.end());
}

TEST(LinkedHashSetTest, EqualRange) {
  linked_hash_set<int> m{3, 1};
  const auto& const_m = m;

  EXPECT_THAT(m.equal_range(2), testing::Pair(m.end(), m.end()));
  EXPECT_THAT(const_m.equal_range(2),
              testing::Pair(const_m.end(), const_m.end()));

  EXPECT_THAT(m.equal_range(1), testing::Pair(m.find(1), ++m.find(1)));
  EXPECT_THAT(const_m.equal_range(1),
              testing::Pair(const_m.find(1), ++const_m.find(1)));
}

TEST(LinkedHashSetTest, ReserveWorks) {
  linked_hash_set<int> m;
  EXPECT_EQ(0, m.size());
  EXPECT_EQ(0.0, m.load_factor());
  m.reserve(10);
  EXPECT_LE(10, m.capacity());
  EXPECT_EQ(0, m.size());
  EXPECT_EQ(0.0, m.load_factor());
  m.insert(1);
  m.insert(2);
  EXPECT_LE(10, m.capacity());
  EXPECT_EQ(2, m.size());
  EXPECT_LT(0.0, m.load_factor());
}

TEST(LinkedHashSetTest, HeterogeneousTests) {
  absl::test_internal::InstanceTracker tracker;

  linked_hash_set<ExpensiveType, HeterogeneousHash, HeterogeneousEqual> set;
  ExpensiveType one(1);
  tracker.ResetCopiesMovesSwaps();
  set.insert(one);
  // Two instances: 'one' var and an instance in the set.
  EXPECT_EQ(2, tracker.instances());
  EXPECT_EQ(1, tracker.copies());
  EXPECT_EQ(0, tracker.moves());

  tracker.ResetCopiesMovesSwaps();
  set.insert(one);
  // No construction since key==1 exists.
  EXPECT_EQ(2, tracker.instances());
  EXPECT_EQ(0, tracker.copies());
  EXPECT_EQ(0, tracker.moves());

  tracker.ResetCopiesMovesSwaps();
  set.emplace(CheapType(1));
  // No construction since key==1 exists.
  EXPECT_EQ(2, tracker.instances());
  EXPECT_EQ(0, tracker.copies());
  EXPECT_EQ(0, tracker.moves());

  tracker.ResetCopiesMovesSwaps();
  set.emplace(CheapType(2));
  // Construction since key==2 doesn't exist in the set.
  EXPECT_EQ(3, tracker.instances());
  EXPECT_EQ(0, tracker.copies());
  EXPECT_EQ(0, tracker.moves());
  EXPECT_THAT(set, ElementsAre(HasExpensiveValue(1), HasExpensiveValue(2)));

  // find
  tracker.ResetCopiesMovesSwaps();
  auto itr = set.find(CheapType(1));
  ASSERT_NE(itr, set.end());
  EXPECT_EQ(1, itr->value());
  // contains
  EXPECT_TRUE(set.contains(CheapType(2)));
  // count
  EXPECT_EQ(1, set.count(CheapType(2)));
  // equal_range
  auto eq_itr_pair = set.equal_range(CheapType(2));
  ASSERT_NE(eq_itr_pair.first, set.end());
  EXPECT_EQ(2, eq_itr_pair.first->value());
  // No construction for find, contains, count or equal_range.
  EXPECT_EQ(3, tracker.instances());
  EXPECT_EQ(0, tracker.copies());
  EXPECT_EQ(0, tracker.moves());

  // emplace
  tracker.ResetCopiesMovesSwaps();
  set.emplace(3);
  // Just one construction.
  EXPECT_EQ(4, tracker.instances());
  EXPECT_EQ(0, tracker.copies());
  EXPECT_EQ(0, tracker.moves());

  tracker.ResetCopiesMovesSwaps();
  set.emplace(3);
  // No additional construction since key==3 exists.
  EXPECT_EQ(4, tracker.instances());
  EXPECT_EQ(0, tracker.copies());
  EXPECT_EQ(0, tracker.moves());
  EXPECT_THAT(set, ElementsAre(HasExpensiveValue(1), HasExpensiveValue(2),
                               HasExpensiveValue(3)));

  // Test std::move() using insert().
  ExpensiveType four(4);
  tracker.ResetCopiesMovesSwaps();
  set.insert(std::move(four));
  // Two constructions (regular and move).
  EXPECT_EQ(6, tracker.instances());
  EXPECT_EQ(0, tracker.copies());
  EXPECT_EQ(1, tracker.moves());

  EXPECT_THAT(set, ElementsAre(HasExpensiveValue(1), HasExpensiveValue(2),
                               HasExpensiveValue(3), HasExpensiveValue(4)));

  tracker.ResetCopiesMovesSwaps();
  set.erase(CheapType(1));
  // No construction and instance reduced by one.
  EXPECT_EQ(5, tracker.instances());
  EXPECT_EQ(0, tracker.copies());
  EXPECT_EQ(0, tracker.moves());
  EXPECT_THAT(set, ElementsAre(HasExpensiveValue(2), HasExpensiveValue(3),
                               HasExpensiveValue(4)));
}

TEST(LinkedHashSetTest, HeterogeneousStringViewLookup) {
  linked_hash_set<std::string> set;
  set.insert("foo");
  set.insert("bar");
  set.insert("blah");

  {
    absl::string_view lookup("foo");
    auto itr = set.find(lookup);
    ASSERT_NE(itr, set.end());
    EXPECT_EQ("foo", *itr);
  }

  // Not found.
  {
    absl::string_view lookup("foobar");
    EXPECT_EQ(set.end(), set.find(lookup));
  }

  {
    absl::string_view lookup("blah");
    auto itr = set.find(lookup);
    ASSERT_NE(itr, set.end());
    EXPECT_EQ("blah", *itr);
  }
}

TEST(LinkedHashSetTest, EmplaceString) {
  std::vector<std::string> v = {"a", "b"};
  linked_hash_set<absl::string_view> hs(v.begin(), v.end());
  EXPECT_THAT(hs, ElementsAreArray(v));
}

TEST(LinkedHashSetTest, BitfieldArgument) {
  union {
    int n : 1;
  };
  n = 0;
  linked_hash_set<int> s = {n};
  s.insert(n);
  s.insert(s.end(), n);
  s.insert({n});
  s.erase(n);
  s.count(n);
  s.find(n);
  s.contains(n);
  s.equal_range(n);
}

TEST(LinkedHashSetTest, MergeExtractInsert) {
  struct Hash {
    size_t operator()(const std::unique_ptr<int>& p) const { return *p; }
  };
  struct Eq {
    bool operator()(const std::unique_ptr<int>& a,
                    const std::unique_ptr<int>& b) const {
      return *a == *b;
    }
  };
  linked_hash_set<std::unique_ptr<int>, Hash, Eq> set1, set2;
  set1.insert(std::make_unique<int>(7));
  set1.insert(std::make_unique<int>(17));

  set2.insert(std::make_unique<int>(7));
  set2.insert(std::make_unique<int>(19));

  EXPECT_THAT(set1, ElementsAre(Pointee(7), Pointee(17)));
  EXPECT_THAT(set2, ElementsAre(Pointee(7), Pointee(19)));

  set1.merge(set2);

  EXPECT_THAT(set1, ElementsAre(Pointee(7), Pointee(17), Pointee(19)));
  EXPECT_THAT(set2, ElementsAre(Pointee(7)));

  auto node = set1.extract(std::make_unique<int>(7));
  EXPECT_TRUE(node);
  EXPECT_THAT(node.value(), Pointee(7));
  EXPECT_THAT(set1, ElementsAre(Pointee(17), Pointee(19)));

  auto insert_result = set2.insert(std::move(node));
  EXPECT_FALSE(node);
  EXPECT_FALSE(insert_result.inserted);
  EXPECT_TRUE(insert_result.node);
  EXPECT_THAT(insert_result.node.value(), Pointee(7));
  EXPECT_EQ(**insert_result.position, 7);
  EXPECT_NE(insert_result.position->get(), insert_result.node.value().get());
  EXPECT_THAT(set2, ElementsAre(Pointee(7)));

  node = set1.extract(std::make_unique<int>(17));
  EXPECT_TRUE(node);
  EXPECT_THAT(node.value(), Pointee(17));
  EXPECT_THAT(set1, ElementsAre(Pointee(19)));

  node.value() = std::make_unique<int>(23);

  insert_result = set2.insert(std::move(node));
  EXPECT_FALSE(node);
  EXPECT_TRUE(insert_result.inserted);
  EXPECT_FALSE(insert_result.node);
  EXPECT_EQ(**insert_result.position, 23);
  EXPECT_THAT(set2, ElementsAre(Pointee(7), Pointee(23)));
}

TEST(LinkedHashSet, ExtractInsert) {
  linked_hash_set<int> s = {1, 7, 2, 9};
  auto node = s.extract(1);
  EXPECT_TRUE(node);
  EXPECT_EQ(node.value(), 1);
  EXPECT_THAT(s, ElementsAre(7, 2, 9));
  EXPECT_FALSE(s.contains(1));

  node.value() = 17;
  s.insert(std::move(node));
  EXPECT_FALSE(node);
  EXPECT_THAT(s, ElementsAre(7, 2, 9, 17));
  EXPECT_TRUE(s.contains(17));

  node = s.extract(s.find(9));
  EXPECT_TRUE(node);
  EXPECT_EQ(node.value(), 9);
  EXPECT_THAT(s, ElementsAre(7, 2, 17));
  EXPECT_FALSE(s.contains(9));
}

TEST(LinkedHashSet, Merge) {
  linked_hash_set<int> m = {1, 7, 3, 6, 10};
  linked_hash_set<int> src = {1, 2, 9, 10, 4, 16};

  m.merge(src);

  EXPECT_THAT(m, ElementsAre(1, 7, 3, 6, 10, 2, 9, 4, 16));
  for (int i : {1, 7, 3, 6, 10, 2, 9, 4, 16}) {
    EXPECT_TRUE(m.contains(i));
  }
  EXPECT_THAT(src, ElementsAre(1, 10));
  for (int i : {1, 10}) {
    EXPECT_TRUE(src.contains(i));
  }
  for (int i : {2, 9, 4, 16}) {
    EXPECT_FALSE(src.contains(i));
  }
}

TEST(LinkedHashSet, EraseRange) {
  linked_hash_set<int> set = {1, 2, 3, 4, 5, 25, 36, 7, 8, 9, 81};
  auto start = set.find(3);
  auto end = set.find(8);
  auto itr = set.erase(start, end);
  ASSERT_NE(itr, set.end());
  EXPECT_THAT(*itr, 8);
  EXPECT_THAT(set, ElementsAre(1, 2, 8, 9, 81));
  for (int i : {1, 2, 8, 9, 81}) {
    EXPECT_TRUE(set.contains(i));
  }
  for (int i : {3, 4, 5, 25, 36, 7}) {
    EXPECT_FALSE(set.contains(i));
  }
}

TEST(LinkedHashSet, InsertInitializerList) {
  linked_hash_set<int> set;
  set.insert({1, 7, 2, 9, 3, 29});
  EXPECT_THAT(set, ElementsAre(1, 7, 2, 9, 3, 29));
  for (int i : {1, 7, 2, 9, 3, 29}) {
    EXPECT_TRUE(set.contains(i));
  }
}

struct CountedHash {
  explicit CountedHash(int* count) : count(count) {}
  size_t operator()(int value) const {
    ++(*count);
    return value;
  }
  int* count = nullptr;
};

// Makes a set too big for small object optimization.  Counts the number of
// hashes in `count`, but leaves `count` set to 0.
linked_hash_set<int, CountedHash> MakeNonSmallSet(int* count) {
  const int kFirstKey = -1000;
  linked_hash_set<int, CountedHash> s(0, CountedHash(count));
  for (int i = kFirstKey; i < kFirstKey + 100; ++i) {
    s.insert(i);
  }
  *count = 0;
  return s;
}

constexpr bool BuildHasDebugModeRehashes() {
#if !defined(NDEBUG) || defined(ABSL_HAVE_ADDRESS_SANITIZER) || \
    defined(ABSL_HAVE_MEMORY_SANITIZER) || defined(ABSL_HAVE_THREAD_SANITIZER)
  return true;
#else
  return false;
#endif
}

TEST(LinkedHashSetTest, HashCountInOptBuilds) {
  if (BuildHasDebugModeRehashes()) {
    GTEST_SKIP() << "Only run under NDEBUG: `assert` statements and sanitizer "
                    "rehashing may cause redundant hashing.";
  }

  using Set = linked_hash_set<int, CountedHash>;
  {
    int count = 0;
    Set s = MakeNonSmallSet(&count);
    s.insert(1);
    EXPECT_EQ(count, 1);
    s.erase(1);
    EXPECT_EQ(count, 2);
  }
  {
    int count = 0;
    Set s = MakeNonSmallSet(&count);
    s.insert(3);
    EXPECT_EQ(count, 1);
    auto node = s.extract(3);
    EXPECT_EQ(count, 2);
    s.insert(std::move(node));
    EXPECT_EQ(count, 3);
  }
  {
    int count = 0;
    Set s = MakeNonSmallSet(&count);
    s.emplace(5);
    EXPECT_EQ(count, 1);
  }
  {
    int src_count = 0, dst_count = 0;
    Set src = MakeNonSmallSet(&src_count);
    Set dst = MakeNonSmallSet(&dst_count);
    src.insert(7);
    dst.merge(src);
    EXPECT_LE(src_count, 200);
    EXPECT_LE(dst_count, 200);
  }
}

}  // namespace
}  // namespace container_internal
ABSL_NAMESPACE_END
}  // namespace absl
