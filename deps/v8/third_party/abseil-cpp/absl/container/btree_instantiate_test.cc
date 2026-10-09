// Copyright 2026 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// -----------------------------------------------------------------------------
// File: btree_instantiate_test.cc
// -----------------------------------------------------------------------------
//
// Various tests for btree containers with diverse value types and
// configurations.
// NOTE: There are more tests in btree_test.cc.
// The main reason to have this file separate is to improve build times.

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/btree_test.h"
#include "absl/flags/flag.h"
#include "absl/strings/cord.h"

ABSL_FLAG(int, test_values, 10000, "The number of values to use for tests");

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace container_internal {

namespace {

template <typename T, typename U>
void CheckPairEquals(const T& x, const U& y) {
  ABSL_INTERNAL_CHECK(x == y, "Values are unequal.");
}

template <typename T, typename U, typename V, typename W>
void CheckPairEquals(const std::pair<T, U>& x, const std::pair<V, W>& y) {
  CheckPairEquals(x.first, y.first);
  CheckPairEquals(x.second, y.second);
}

}  // namespace

// The base class for a sorted associative container checker. TreeType is the
// container type to check and CheckerType is the container type to check
// against. TreeType is expected to be btree_{set,map,multiset,multimap} and
// CheckerType is expected to be {set,map,multiset,multimap}.
template <typename TreeType, typename CheckerType>
class base_checker {
 public:
  using key_type = typename TreeType::key_type;
  using value_type = typename TreeType::value_type;
  using key_compare = typename TreeType::key_compare;
  using pointer = typename TreeType::pointer;
  using const_pointer = typename TreeType::const_pointer;
  using reference = typename TreeType::reference;
  using const_reference = typename TreeType::const_reference;
  using size_type = typename TreeType::size_type;
  using difference_type = typename TreeType::difference_type;
  using iterator = typename TreeType::iterator;
  using const_iterator = typename TreeType::const_iterator;
  using reverse_iterator = typename TreeType::reverse_iterator;
  using const_reverse_iterator = typename TreeType::const_reverse_iterator;

 public:
  base_checker() : const_tree_(tree_) {}
  base_checker(const base_checker& other)
      : tree_(other.tree_), const_tree_(tree_), checker_(other.checker_) {}
  template <typename InputIterator>
  base_checker(InputIterator b, InputIterator e)
      : tree_(b, e), const_tree_(tree_), checker_(b, e) {}

  iterator begin() { return tree_.begin(); }
  const_iterator begin() const { return tree_.begin(); }
  iterator end() { return tree_.end(); }
  const_iterator end() const { return tree_.end(); }
  reverse_iterator rbegin() { return tree_.rbegin(); }
  const_reverse_iterator rbegin() const { return tree_.rbegin(); }
  reverse_iterator rend() { return tree_.rend(); }
  const_reverse_iterator rend() const { return tree_.rend(); }

  template <typename IterType, typename CheckerIterType>
  IterType iter_check(IterType tree_iter, CheckerIterType checker_iter) const {
    if (tree_iter == tree_.end()) {
      ABSL_INTERNAL_CHECK(checker_iter == checker_.end(),
                          "Checker iterator not at end.");
    } else {
      CheckPairEquals(*tree_iter, *checker_iter);
    }
    return tree_iter;
  }
  template <typename IterType, typename CheckerIterType>
  IterType riter_check(IterType tree_iter, CheckerIterType checker_iter) const {
    if (tree_iter == tree_.rend()) {
      ABSL_INTERNAL_CHECK(checker_iter == checker_.rend(),
                          "Checker iterator not at rend.");
    } else {
      CheckPairEquals(*tree_iter, *checker_iter);
    }
    return tree_iter;
  }
  void value_check(const value_type& v) {
    typename KeyOfValue<typename TreeType::key_type,
                        typename TreeType::value_type>::type key_of_value;
    const key_type& key = key_of_value(v);
    CheckPairEquals(*find(key), v);
    lower_bound(key);
    upper_bound(key);
    equal_range(key);
    contains(key);
    count(key);
  }
  void erase_check(const key_type& key) {
    EXPECT_FALSE(tree_.contains(key));
    EXPECT_EQ(tree_.find(key), const_tree_.end());
    EXPECT_FALSE(const_tree_.contains(key));
    EXPECT_EQ(const_tree_.find(key), tree_.end());
    EXPECT_EQ(tree_.equal_range(key).first,
              const_tree_.equal_range(key).second);
  }

  iterator lower_bound(const key_type& key) {
    return iter_check(tree_.lower_bound(key), checker_.lower_bound(key));
  }
  const_iterator lower_bound(const key_type& key) const {
    return iter_check(tree_.lower_bound(key), checker_.lower_bound(key));
  }
  iterator upper_bound(const key_type& key) {
    return iter_check(tree_.upper_bound(key), checker_.upper_bound(key));
  }
  const_iterator upper_bound(const key_type& key) const {
    return iter_check(tree_.upper_bound(key), checker_.upper_bound(key));
  }
  std::pair<iterator, iterator> equal_range(const key_type& key) {
    std::pair<typename CheckerType::iterator, typename CheckerType::iterator>
        checker_res = checker_.equal_range(key);
    std::pair<iterator, iterator> tree_res = tree_.equal_range(key);
    iter_check(tree_res.first, checker_res.first);
    iter_check(tree_res.second, checker_res.second);
    return tree_res;
  }
  std::pair<const_iterator, const_iterator> equal_range(
      const key_type& key) const {
    std::pair<typename CheckerType::const_iterator,
              typename CheckerType::const_iterator>
        checker_res = checker_.equal_range(key);
    std::pair<const_iterator, const_iterator> tree_res = tree_.equal_range(key);
    iter_check(tree_res.first, checker_res.first);
    iter_check(tree_res.second, checker_res.second);
    return tree_res;
  }
  iterator find(const key_type& key) {
    return iter_check(tree_.find(key), checker_.find(key));
  }
  const_iterator find(const key_type& key) const {
    return iter_check(tree_.find(key), checker_.find(key));
  }
  bool contains(const key_type& key) const { return find(key) != end(); }
  size_type count(const key_type& key) const {
    size_type res = checker_.count(key);
    EXPECT_EQ(res, tree_.count(key));
    return res;
  }

  base_checker& operator=(const base_checker& other) {
    tree_ = other.tree_;
    checker_ = other.checker_;
    return *this;
  }

  int erase(const key_type& key) {
    int size = tree_.size();
    int res = checker_.erase(key);
    EXPECT_EQ(res, tree_.count(key));
    EXPECT_EQ(res, tree_.erase(key));
    EXPECT_EQ(tree_.count(key), 0);
    EXPECT_EQ(tree_.size(), size - res);
    erase_check(key);
    return res;
  }
  iterator erase(iterator iter) {
    key_type key = iter.key();
    int size = tree_.size();
    int count = tree_.count(key);
    auto checker_iter = checker_.lower_bound(key);
    for (iterator tmp(tree_.lower_bound(key)); tmp != iter; ++tmp) {
      ++checker_iter;
    }
    auto checker_next = checker_iter;
    ++checker_next;
    checker_.erase(checker_iter);
    iter = tree_.erase(iter);
    EXPECT_EQ(tree_.size(), checker_.size());
    EXPECT_EQ(tree_.size(), size - 1);
    EXPECT_EQ(tree_.count(key), count - 1);
    if (count == 1) {
      erase_check(key);
    }
    return iter_check(iter, checker_next);
  }

  void erase(iterator begin, iterator end) {
    int size = tree_.size();
    int count = std::distance(begin, end);
    auto checker_begin = checker_.lower_bound(begin.key());
    for (iterator tmp(tree_.lower_bound(begin.key())); tmp != begin; ++tmp) {
      ++checker_begin;
    }
    auto checker_end =
        end == tree_.end() ? checker_.end() : checker_.lower_bound(end.key());
    if (end != tree_.end()) {
      for (iterator tmp(tree_.lower_bound(end.key())); tmp != end; ++tmp) {
        ++checker_end;
      }
    }
    const auto checker_ret = checker_.erase(checker_begin, checker_end);
    const auto tree_ret = tree_.erase(begin, end);
    EXPECT_EQ(std::distance(checker_.begin(), checker_ret),
              std::distance(tree_.begin(), tree_ret));
    EXPECT_EQ(tree_.size(), checker_.size());
    EXPECT_EQ(tree_.size(), size - count);
  }

  void clear() {
    tree_.clear();
    checker_.clear();
  }
  void swap(base_checker& other) {
    tree_.swap(other.tree_);
    checker_.swap(other.checker_);
  }

  void verify() const {
    tree_.verify();
    EXPECT_EQ(tree_.size(), checker_.size());

    // Move through the forward iterators using increment.
    auto checker_iter = checker_.begin();
    const_iterator tree_iter(tree_.begin());
    for (; tree_iter != tree_.end(); ++tree_iter, ++checker_iter) {
      CheckPairEquals(*tree_iter, *checker_iter);
    }

    // Move through the forward iterators using decrement.
    for (int n = tree_.size() - 1; n >= 0; --n) {
      iter_check(tree_iter, checker_iter);
      --tree_iter;
      --checker_iter;
    }
    EXPECT_EQ(tree_iter, tree_.begin());
    EXPECT_EQ(checker_iter, checker_.begin());

    // Move through the reverse iterators using increment.
    auto checker_riter = checker_.rbegin();
    const_reverse_iterator tree_riter(tree_.rbegin());
    for (; tree_riter != tree_.rend(); ++tree_riter, ++checker_riter) {
      CheckPairEquals(*tree_riter, *checker_riter);
    }

    // Move through the reverse iterators using decrement.
    for (int n = tree_.size() - 1; n >= 0; --n) {
      riter_check(tree_riter, checker_riter);
      --tree_riter;
      --checker_riter;
    }
    EXPECT_EQ(tree_riter, tree_.rbegin());
    EXPECT_EQ(checker_riter, checker_.rbegin());
  }

  const TreeType& tree() const { return tree_; }

  size_type size() const {
    EXPECT_EQ(tree_.size(), checker_.size());
    return tree_.size();
  }
  size_type max_size() const { return tree_.max_size(); }
  bool empty() const {
    EXPECT_EQ(tree_.empty(), checker_.empty());
    return tree_.empty();
  }

 protected:
  TreeType tree_;
  const TreeType& const_tree_;
  CheckerType checker_;
};

namespace {

// A checker for unique sorted associative containers. TreeType is expected to
// be btree_{set,map} and CheckerType is expected to be {set,map}.
template <typename TreeType, typename CheckerType>
class unique_checker : public base_checker<TreeType, CheckerType> {
  using super_type = base_checker<TreeType, CheckerType>;

 public:
  using iterator = typename super_type::iterator;
  using value_type = typename super_type::value_type;

 public:
  unique_checker() : super_type() {}
  unique_checker(const unique_checker& other) : super_type(other) {}
  template <class InputIterator>
  unique_checker(InputIterator b, InputIterator e) : super_type(b, e) {}
  unique_checker& operator=(const unique_checker&) = default;

  // Insertion routines.
  std::pair<iterator, bool> insert(const value_type& v) {
    int size = this->tree_.size();
    std::pair<typename CheckerType::iterator, bool> checker_res =
        this->checker_.insert(v);
    std::pair<iterator, bool> tree_res = this->tree_.insert(v);
    CheckPairEquals(*tree_res.first, *checker_res.first);
    EXPECT_EQ(tree_res.second, checker_res.second);
    EXPECT_EQ(this->tree_.size(), this->checker_.size());
    EXPECT_EQ(this->tree_.size(), size + tree_res.second);
    return tree_res;
  }
  iterator insert(iterator position, const value_type& v) {
    int size = this->tree_.size();
    std::pair<typename CheckerType::iterator, bool> checker_res =
        this->checker_.insert(v);
    iterator tree_res = this->tree_.insert(position, v);
    CheckPairEquals(*tree_res, *checker_res.first);
    EXPECT_EQ(this->tree_.size(), this->checker_.size());
    EXPECT_EQ(this->tree_.size(), size + checker_res.second);
    return tree_res;
  }
  template <typename InputIterator>
  void insert(InputIterator b, InputIterator e) {
    for (; b != e; ++b) {
      insert(*b);
    }
  }
};

// A checker for multiple sorted associative containers. TreeType is expected
// to be btree_{multiset,multimap} and CheckerType is expected to be
// {multiset,multimap}.
template <typename TreeType, typename CheckerType>
class multi_checker : public base_checker<TreeType, CheckerType> {
  using super_type = base_checker<TreeType, CheckerType>;

 public:
  using iterator = typename super_type::iterator;
  using value_type = typename super_type::value_type;

 public:
  multi_checker() : super_type() {}
  multi_checker(const multi_checker& other) : super_type(other) {}
  template <class InputIterator>
  multi_checker(InputIterator b, InputIterator e) : super_type(b, e) {}
  multi_checker& operator=(const multi_checker&) = default;

  // Insertion routines.
  iterator insert(const value_type& v) {
    int size = this->tree_.size();
    auto checker_res = this->checker_.insert(v);
    iterator tree_res = this->tree_.insert(v);
    CheckPairEquals(*tree_res, *checker_res);
    EXPECT_EQ(this->tree_.size(), this->checker_.size());
    EXPECT_EQ(this->tree_.size(), size + 1);
    return tree_res;
  }
  iterator insert(iterator position, const value_type& v) {
    int size = this->tree_.size();
    auto checker_res = this->checker_.insert(v);
    iterator tree_res = this->tree_.insert(position, v);
    CheckPairEquals(*tree_res, *checker_res);
    EXPECT_EQ(this->tree_.size(), this->checker_.size());
    EXPECT_EQ(this->tree_.size(), size + 1);
    return tree_res;
  }
  template <typename InputIterator>
  void insert(InputIterator b, InputIterator e) {
    for (; b != e; ++b) {
      insert(*b);
    }
  }
};

template <typename T, typename V>
void DoTest(const char* name, T* b, const std::vector<V>& values) {
  typename KeyOfValue<typename T::key_type, V>::type key_of_value;

  T& mutable_b = *b;
  const T& const_b = *b;

  // Test insert.
  for (int i = 0; i < values.size(); ++i) {
    mutable_b.insert(values[i]);
    mutable_b.value_check(values[i]);
  }
  ASSERT_EQ(mutable_b.size(), values.size());

  const_b.verify();

  // Test copy constructor.
  T b_copy(const_b);
  EXPECT_EQ(b_copy.size(), const_b.size());
  for (int i = 0; i < values.size(); ++i) {
    CheckPairEquals(*b_copy.find(key_of_value(values[i])), values[i]);
  }

  // Test range constructor.
  T b_range(const_b.begin(), const_b.end());
  EXPECT_EQ(b_range.size(), const_b.size());
  for (int i = 0; i < values.size(); ++i) {
    CheckPairEquals(*b_range.find(key_of_value(values[i])), values[i]);
  }

  // Test range insertion for values that already exist.
  b_range.insert(b_copy.begin(), b_copy.end());
  b_range.verify();

  // Test range insertion for new values.
  b_range.clear();
  b_range.insert(b_copy.begin(), b_copy.end());
  EXPECT_EQ(b_range.size(), b_copy.size());
  for (int i = 0; i < values.size(); ++i) {
    CheckPairEquals(*b_range.find(key_of_value(values[i])), values[i]);
  }

  // Test assignment to self. Nothing should change.
  b_range.operator=(b_range);
  EXPECT_EQ(b_range.size(), b_copy.size());

  // Test assignment of new values.
  b_range.clear();
  b_range = b_copy;
  EXPECT_EQ(b_range.size(), b_copy.size());

  // Test swap.
  b_range.clear();
  b_range.swap(b_copy);
  EXPECT_EQ(b_copy.size(), 0);
  EXPECT_EQ(b_range.size(), const_b.size());
  for (int i = 0; i < values.size(); ++i) {
    CheckPairEquals(*b_range.find(key_of_value(values[i])), values[i]);
  }
  b_range.swap(b_copy);

  // Test non-member function swap.
  swap(b_range, b_copy);
  EXPECT_EQ(b_copy.size(), 0);
  EXPECT_EQ(b_range.size(), const_b.size());
  for (int i = 0; i < values.size(); ++i) {
    CheckPairEquals(*b_range.find(key_of_value(values[i])), values[i]);
  }
  swap(b_range, b_copy);

  // Test erase via values.
  for (int i = 0; i < values.size(); ++i) {
    mutable_b.erase(key_of_value(values[i]));
    // Erasing a non-existent key should have no effect.
    ASSERT_EQ(mutable_b.erase(key_of_value(values[i])), 0);
  }

  const_b.verify();
  EXPECT_EQ(const_b.size(), 0);

  // Test erase via iterators.
  mutable_b = b_copy;
  for (int i = 0; i < values.size(); ++i) {
    mutable_b.erase(mutable_b.find(key_of_value(values[i])));
  }

  const_b.verify();
  EXPECT_EQ(const_b.size(), 0);

  // Test insert with hint.
  for (int i = 0; i < values.size(); i++) {
    mutable_b.insert(mutable_b.upper_bound(key_of_value(values[i])), values[i]);
  }

  const_b.verify();

  // Test range erase.
  mutable_b.erase(mutable_b.begin(), mutable_b.end());
  EXPECT_EQ(mutable_b.size(), 0);
  const_b.verify();

  // First half.
  mutable_b = b_copy;
  typename T::iterator mutable_iter_end = mutable_b.begin();
  for (int i = 0; i < values.size() / 2; ++i) ++mutable_iter_end;
  mutable_b.erase(mutable_b.begin(), mutable_iter_end);
  EXPECT_EQ(mutable_b.size(), values.size() - values.size() / 2);
  const_b.verify();

  // Second half.
  mutable_b = b_copy;
  typename T::iterator mutable_iter_begin = mutable_b.begin();
  for (int i = 0; i < values.size() / 2; ++i) ++mutable_iter_begin;
  mutable_b.erase(mutable_iter_begin, mutable_b.end());
  EXPECT_EQ(mutable_b.size(), values.size() / 2);
  const_b.verify();

  // Second quarter.
  mutable_b = b_copy;
  mutable_iter_begin = mutable_b.begin();
  for (int i = 0; i < values.size() / 4; ++i) ++mutable_iter_begin;
  mutable_iter_end = mutable_iter_begin;
  for (int i = 0; i < values.size() / 4; ++i) ++mutable_iter_end;
  mutable_b.erase(mutable_iter_begin, mutable_iter_end);
  EXPECT_EQ(mutable_b.size(), values.size() - values.size() / 4);
  const_b.verify();

  mutable_b.clear();
}

template <typename T>
void ConstTest() {
  using value_type = typename T::value_type;
  typename KeyOfValue<typename T::key_type, value_type>::type key_of_value;

  T mutable_b;
  const T& const_b = mutable_b;

  // Insert a single value into the container and test looking it up.
  value_type value = Generator<value_type>(2)(2);
  mutable_b.insert(value);
  EXPECT_TRUE(mutable_b.contains(key_of_value(value)));
  EXPECT_NE(mutable_b.find(key_of_value(value)), const_b.end());
  EXPECT_TRUE(const_b.contains(key_of_value(value)));
  EXPECT_NE(const_b.find(key_of_value(value)), mutable_b.end());
  EXPECT_EQ(*const_b.lower_bound(key_of_value(value)), value);
  EXPECT_EQ(const_b.upper_bound(key_of_value(value)), const_b.end());
  EXPECT_EQ(*const_b.equal_range(key_of_value(value)).first, value);

  // We can only create a non-const iterator from a non-const container.
  typename T::iterator mutable_iter(mutable_b.begin());
  EXPECT_EQ(mutable_iter, const_b.begin());
  EXPECT_NE(mutable_iter, const_b.end());
  EXPECT_EQ(const_b.begin(), mutable_iter);
  EXPECT_NE(const_b.end(), mutable_iter);
  typename T::reverse_iterator mutable_riter(mutable_b.rbegin());
  EXPECT_EQ(mutable_riter, const_b.rbegin());
  EXPECT_NE(mutable_riter, const_b.rend());
  EXPECT_EQ(const_b.rbegin(), mutable_riter);
  EXPECT_NE(const_b.rend(), mutable_riter);

  // We can create a const iterator from a non-const iterator.
  typename T::const_iterator const_iter(mutable_iter);
  EXPECT_EQ(const_iter, mutable_b.begin());
  EXPECT_NE(const_iter, mutable_b.end());
  EXPECT_EQ(mutable_b.begin(), const_iter);
  EXPECT_NE(mutable_b.end(), const_iter);
  typename T::const_reverse_iterator const_riter(mutable_riter);
  EXPECT_EQ(const_riter, mutable_b.rbegin());
  EXPECT_NE(const_riter, mutable_b.rend());
  EXPECT_EQ(mutable_b.rbegin(), const_riter);
  EXPECT_NE(mutable_b.rend(), const_riter);

  // Make sure various methods can be invoked on a const container.
  const_b.verify();
  ASSERT_TRUE(!const_b.empty());
  EXPECT_EQ(const_b.size(), 1);
  EXPECT_GT(const_b.max_size(), 0);
  EXPECT_TRUE(const_b.contains(key_of_value(value)));
  EXPECT_EQ(const_b.count(key_of_value(value)), 1);
}

template <typename T, typename C>
void BtreeTest() {
  ConstTest<T>();

  using V = typename remove_pair_const<typename T::value_type>::type;
  const std::vector<V> random_values = GenerateValuesWithSeed<V>(
      absl::GetFlag(FLAGS_test_values), 4 * absl::GetFlag(FLAGS_test_values),
      GTEST_FLAG_GET(random_seed));

  unique_checker<T, C> container;

  // Test key insertion/deletion in sorted order.
  std::vector<V> sorted_values(random_values);
  std::sort(sorted_values.begin(), sorted_values.end());
  DoTest("sorted:    ", &container, sorted_values);

  // Test key insertion/deletion in reverse sorted order.
  std::reverse(sorted_values.begin(), sorted_values.end());
  DoTest("rsorted:   ", &container, sorted_values);

  // Test key insertion/deletion in random order.
  DoTest("random:    ", &container, random_values);
}

template <typename T, typename C>
void BtreeMultiTest() {
  ConstTest<T>();

  using V = typename remove_pair_const<typename T::value_type>::type;
  const std::vector<V> random_values = GenerateValuesWithSeed<V>(
      absl::GetFlag(FLAGS_test_values), 4 * absl::GetFlag(FLAGS_test_values),
      GTEST_FLAG_GET(random_seed));

  multi_checker<T, C> container;

  // Test keys in sorted order.
  std::vector<V> sorted_values(random_values);
  std::sort(sorted_values.begin(), sorted_values.end());
  DoTest("sorted:    ", &container, sorted_values);

  // Test keys in reverse sorted order.
  std::reverse(sorted_values.begin(), sorted_values.end());
  DoTest("rsorted:   ", &container, sorted_values);

  // Test keys in random order.
  DoTest("random:    ", &container, random_values);

  // Test keys in random order w/ duplicates.
  std::vector<V> duplicate_values(random_values);
  duplicate_values.insert(duplicate_values.end(), random_values.begin(),
                          random_values.end());
  DoTest("duplicates:", &container, duplicate_values);

  // Test all identical keys.
  std::vector<V> identical_values(100);
  std::fill(identical_values.begin(), identical_values.end(),
            Generator<V>(2)(2));
  DoTest("identical: ", &container, identical_values);
}

template <typename T>
void BtreeMapTest() {
  using value_type = typename T::value_type;
  using mapped_type = typename T::mapped_type;

  mapped_type m = Generator<mapped_type>(0)(0);
  (void)m;

  T b;

  // Verify we can insert using operator[].
  for (int i = 0; i < 1000; i++) {
    value_type v = Generator<value_type>(1000)(i);
    b[v.first] = v.second;
  }
  EXPECT_EQ(b.size(), 1000);

  // Test whether we can use the "->" operator on iterators and
  // reverse_iterators. This stresses the btree_map_params::pair_pointer
  // mechanism.
  EXPECT_EQ(b.begin()->first, Generator<value_type>(1000)(0).first);
  EXPECT_EQ(b.begin()->second, Generator<value_type>(1000)(0).second);
  EXPECT_EQ(b.rbegin()->first, Generator<value_type>(1000)(999).first);
  EXPECT_EQ(b.rbegin()->second, Generator<value_type>(1000)(999).second);
}

template <typename T>
void BtreeMultiMapTest() {
  using mapped_type = typename T::mapped_type;
  mapped_type m = Generator<mapped_type>(0)(0);
  (void)m;
}

template <typename K, int N = 256>
void SetTest() {
  EXPECT_EQ(sizeof(absl::btree_set<K>),
            2 * sizeof(void*) + sizeof(typename absl::btree_set<K>::size_type));
  using BtreeSet = absl::btree_set<K>;
  BtreeTest<BtreeSet, std::set<K>>();
}

template <typename K, int N = 256>
void MapTest() {
  EXPECT_EQ(
      sizeof(absl::btree_map<K, K>),
      2 * sizeof(void*) + sizeof(typename absl::btree_map<K, K>::size_type));
  using BtreeMap = absl::btree_map<K, K>;
  BtreeTest<BtreeMap, std::map<K, K>>();
  BtreeMapTest<BtreeMap>();
}

TEST(Btree, set_int32) { SetTest<int32_t>(); }
TEST(Btree, set_string) { SetTest<std::string>(); }
TEST(Btree, set_cord) { SetTest<absl::Cord>(); }
TEST(Btree, map_int32) { MapTest<int32_t>(); }
TEST(Btree, map_string) { MapTest<std::string>(); }
TEST(Btree, map_cord) { MapTest<absl::Cord>(); }

template <typename K, int N = 256>
void MultiSetTest() {
  EXPECT_EQ(
      sizeof(absl::btree_multiset<K>),
      2 * sizeof(void*) + sizeof(typename absl::btree_multiset<K>::size_type));
  using BtreeMSet = absl::btree_multiset<K>;
  BtreeMultiTest<BtreeMSet, std::multiset<K>>();
}

template <typename K, int N = 256>
void MultiMapTest() {
  EXPECT_EQ(sizeof(absl::btree_multimap<K, K>),
            2 * sizeof(void*) +
                sizeof(typename absl::btree_multimap<K, K>::size_type));
  using BtreeMMap = absl::btree_multimap<K, K>;
  BtreeMultiTest<BtreeMMap, std::multimap<K, K>>();
  BtreeMultiMapTest<BtreeMMap>();
}

TEST(Btree, multiset_int32) { MultiSetTest<int32_t>(); }
TEST(Btree, multiset_string) { MultiSetTest<std::string>(); }
TEST(Btree, multiset_cord) { MultiSetTest<absl::Cord>(); }
TEST(Btree, multimap_int32) { MultiMapTest<int32_t>(); }
TEST(Btree, multimap_string) { MultiMapTest<std::string>(); }
TEST(Btree, multimap_cord) { MultiMapTest<absl::Cord>(); }

}  // namespace
}  // namespace container_internal
ABSL_NAMESPACE_END
}  // namespace absl
