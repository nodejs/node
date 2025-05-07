// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/small-vector.h"

#include <algorithm>
#include <array>
#include <initializer_list>
#include <vector>

#include "testing/gmock-support.h"

namespace v8::base {

namespace {
template <size_t n, int start, int... I>
std::initializer_list<int> make_int_list_impl(
    std::integer_sequence<int, I...> s) {
  static std::initializer_list<int> result{(start + I)...};
  return result;
}

template <size_t n, int start = 0>
constexpr std::initializer_list<int> make_int_list() {
  return make_int_list_impl<n, start>(std::make_integer_sequence<int, n>());
}
}  // anonymous namespace

// Tests with vector elements that are trivially constructible/destructible.

TEST(SmallVectorTest, SimpleConstructTrivial) {
  // A vector with zero capacity.
  {
    SmallVector<int, 0> v;
    EXPECT_EQ(0UL, v.size());
    EXPECT_EQ(0UL, v.capacity());
    EXPECT_TRUE(v.empty());
  }

  // An implicitly empty small vector.
  {
    SmallVector<int, 10> v;
    EXPECT_EQ(0UL, v.size());
    EXPECT_EQ(10UL, v.capacity());
    EXPECT_TRUE(v.empty());
  }

  // An explicitly empty small vector.
  {
    SmallVector<int, 10> v(0);
    EXPECT_EQ(0UL, v.size());
    EXPECT_EQ(10UL, v.capacity());
    EXPECT_TRUE(v.empty());
  }

  // A non-empty, default-initialized small vector.
  {
    SmallVector<int, 10> v(5);
    EXPECT_EQ(5UL, v.size());
    EXPECT_EQ(10UL, v.capacity());
    EXPECT_FALSE(v.empty());
  }

  // A non-empty, default-initialized big vector.
  {
    SmallVector<int, 10> v(15);
    EXPECT_EQ(15UL, v.size());
    EXPECT_LE(15UL, v.capacity());
    EXPECT_FALSE(v.empty());
  }

  // A non-empty, explicitly initialized small vector.
  {
    SmallVector<int, 10> v(5, 42);
    EXPECT_EQ(5UL, v.size());
    EXPECT_EQ(10UL, v.capacity());
    EXPECT_FALSE(v.empty());
    EXPECT_EQ(42, v[3]);
  }

  // A non-empty, explicitly initialized big vector.
  {
    SmallVector<int, 10> v(15, 42);
    EXPECT_EQ(15UL, v.size());
    EXPECT_LE(15UL, v.capacity());
    EXPECT_FALSE(v.empty());
    EXPECT_EQ(42, v[13]);
  }
}

TEST(SmallVectorTest, ConstructFromListTrivial) {
  // Constructor from initializer list, small.
  {
    SmallVector<int, 10> v{make_int_list<7>()};
    EXPECT_EQ(7UL, v.size());
    EXPECT_EQ(10UL, v.capacity());
    EXPECT_FALSE(v.empty());
    EXPECT_EQ(5, v[5]);
  }

  // Constructor from initializer list, big.
  {
    SmallVector<int, 10> v{make_int_list<14>()};
    EXPECT_EQ(14UL, v.size());
    EXPECT_LE(14UL, v.capacity());
    EXPECT_FALSE(v.empty());
    EXPECT_EQ(3, v[3]);
    EXPECT_EQ(11, v[11]);
  }
}

TEST(SmallVectorTest, ConstructFromVectorTrivial) {
  // Constructor from base::Vector, small.
  {
    SmallVector<int, 10> v(base::VectorOf(make_int_list<7>()));
    EXPECT_EQ(7UL, v.size());
    EXPECT_EQ(10UL, v.capacity());
    EXPECT_FALSE(v.empty());
    EXPECT_EQ(5, v[5]);
  }

  // Constructor from base::Vector, big.
  {
    SmallVector<int, 10> v(base::VectorOf(make_int_list<14>()));
    EXPECT_EQ(14UL, v.size());
    EXPECT_LE(14UL, v.capacity());
    EXPECT_FALSE(v.empty());
    EXPECT_EQ(3, v[3]);
    EXPECT_EQ(11, v[11]);
  }
}

TEST(SmallVectorTest, CopyConstructTrivial) {
  // Copy constructor, small.
  {
    SmallVector<int, 10> v1{make_int_list<7>()};
    SmallVector<int, 10> v2 = v1;
    EXPECT_EQ(7UL, v1.size());
    EXPECT_EQ(10UL, v1.capacity());
    EXPECT_FALSE(v1.empty());
    EXPECT_EQ(5, v1[5]);
    EXPECT_EQ(7UL, v2.size());
    EXPECT_EQ(10UL, v2.capacity());
    EXPECT_FALSE(v2.empty());
    EXPECT_EQ(5, v2[5]);
  }

  // Copy constructor, big.
  {
    SmallVector<int, 10> v1{make_int_list<14>()};
    SmallVector<int, 10> v2 = v1;
    EXPECT_EQ(14UL, v1.size());
    EXPECT_LE(14UL, v1.capacity());
    EXPECT_FALSE(v1.empty());
    EXPECT_EQ(3, v1[3]);
    EXPECT_EQ(11, v1[11]);
    EXPECT_EQ(14UL, v2.size());
    EXPECT_LE(14UL, v2.capacity());
    EXPECT_FALSE(v2.empty());
    EXPECT_EQ(3, v2[3]);
    EXPECT_EQ(11, v2[11]);
  }
}

TEST(SmallVectorTest, MoveConstructTrivial) {
  // Move constructor, small.
  {
    SmallVector<int, 10> v1{make_int_list<7>()};
    SmallVector<int, 10> v2 = std::move(v1);
    EXPECT_EQ(0UL, v1.size());
    EXPECT_EQ(10UL, v1.capacity());
    EXPECT_TRUE(v1.empty());
    EXPECT_EQ(7UL, v2.size());
    EXPECT_EQ(10UL, v2.capacity());
    EXPECT_FALSE(v2.empty());
    EXPECT_EQ(5, v2[5]);
  }

  // Move constructor, big.
  {
    SmallVector<int, 10> v1{make_int_list<14>()};
    SmallVector<int, 10> v2 = std::move(v1);
    EXPECT_EQ(0UL, v1.size());
    EXPECT_EQ(10UL, v1.capacity());
    EXPECT_TRUE(v1.empty());
    EXPECT_EQ(14UL, v2.size());
    EXPECT_LE(14UL, v2.capacity());
    EXPECT_FALSE(v2.empty());
    EXPECT_EQ(3, v2[3]);
    EXPECT_EQ(11, v2[11]);
  }
}

namespace {
template <size_t capacity, size_t src_size, size_t trg_size>
void TestAssignTrivial() {
  constexpr int src_isize = static_cast<int>(src_size);
  SmallVector<int, capacity> src{make_int_list<src_size>()};
  SmallVector<int, capacity> trg{make_int_list<trg_size, 1000>()};
  trg = src;
  EXPECT_EQ(src_size, src.size());
  EXPECT_LE(capacity, src.capacity());
  EXPECT_TRUE((src_size == 0) == src.empty());
  EXPECT_EQ(src_size, trg.size());
  EXPECT_LE(src_size, trg.capacity());
  EXPECT_LE(capacity, trg.capacity());
  EXPECT_TRUE((src_size == 0) == trg.empty());
  for (int i = 0; i < src_isize; ++i) {
    EXPECT_EQ(i, src[i]);
    EXPECT_EQ(i, trg[i]);
  }
}
}  // anonymous namespace

TEST(SmallVectorTest, AssignTrivial) {
  // Small vectors.
  TestAssignTrivial<10, 7, 7>();
  TestAssignTrivial<10, 5, 7>();
  TestAssignTrivial<10, 9, 7>();
  TestAssignTrivial<0, 0, 0>();
  // Big vectors.
  TestAssignTrivial<10, 17, 17>();
  TestAssignTrivial<10, 15, 17>();
  TestAssignTrivial<10, 19, 17>();
  TestAssignTrivial<0, 7, 7>();
  TestAssignTrivial<0, 7, 5>();
  TestAssignTrivial<0, 5, 7>();
  // Small assigned to big.
  TestAssignTrivial<10, 17, 7>();
  TestAssignTrivial<0, 17, 0>();
  // Big assigned to small.
  TestAssignTrivial<10, 7, 17>();
  TestAssignTrivial<0, 0, 17>();
}

namespace {
template <size_t capacity, size_t src_size, size_t trg_size>
void TestMoveAssignTrivial() {
  constexpr int src_isize = static_cast<int>(src_size);
  SmallVector<int, capacity> src{make_int_list<src_size>()};
  SmallVector<int, capacity> trg{make_int_list<trg_size, 1000>()};
  trg = std::move(src);
  EXPECT_EQ(0UL, src.size());
  EXPECT_EQ(capacity, src.capacity());
  EXPECT_TRUE(src.empty());
  EXPECT_EQ(src_size, trg.size());
  EXPECT_LE(src_size, trg.capacity());
  EXPECT_LE(capacity, trg.capacity());
  EXPECT_TRUE((src_size == 0) == trg.empty());
  for (int i = 0; i < src_isize; ++i) {
    EXPECT_EQ(i, trg[i]);
  }
}
}  // anonymous namespace

TEST(SmallVectorTest, MoveAssignTrivial) {
  // Small vectors.
  TestMoveAssignTrivial<10, 7, 7>();
  TestMoveAssignTrivial<10, 5, 7>();
  TestMoveAssignTrivial<10, 9, 7>();
  TestMoveAssignTrivial<0, 0, 0>();
  // Big vectors.
  TestMoveAssignTrivial<10, 17, 17>();
  TestMoveAssignTrivial<10, 15, 17>();
  TestMoveAssignTrivial<10, 19, 17>();
  TestMoveAssignTrivial<0, 7, 7>();
  TestMoveAssignTrivial<0, 7, 5>();
  TestMoveAssignTrivial<0, 5, 7>();
  // Small assigned to big.
  TestMoveAssignTrivial<10, 17, 7>();
  TestMoveAssignTrivial<0, 17, 0>();
  // Big assigned to small.
  TestMoveAssignTrivial<10, 7, 17>();
  TestMoveAssignTrivial<0, 0, 17>();
}

namespace {
template <size_t capacity, size_t src_size, size_t trg_size>
void TestCopySeparation() {
  constexpr int src_isize = static_cast<int>(src_size);
  static_assert(src_size >= 3);
  static_assert(trg_size >= 3);

  // Copy constructor results in vector with different backing store.
  {
    SmallVector<int, capacity> src{make_int_list<src_size>()};
    SmallVector<int, capacity> trg = src;
    for (int i = 0; i < src_isize; ++i) {
      EXPECT_EQ(i, src[i]);
      EXPECT_EQ(i, trg[i]);
    }
    src[1] = 17;
    trg[2] = 42;
    EXPECT_EQ(17, src[1]);
    EXPECT_EQ(2, src[2]);
    EXPECT_EQ(1, trg[1]);
    EXPECT_EQ(42, trg[2]);
  }

  // Copy assignment results in vector with different backing store.
  {
    SmallVector<int, capacity> src{make_int_list<src_size>()};
    SmallVector<int, capacity> trg{make_int_list<trg_size, 1000>()};
    trg = src;
    for (int i = 0; i < src_isize; ++i) {
      EXPECT_EQ(i, src[i]);
      EXPECT_EQ(i, trg[i]);
    }
    src[1] = 17;
    trg[2] = 42;
    EXPECT_EQ(17, src[1]);
    EXPECT_EQ(2, src[2]);
    EXPECT_EQ(1, trg[1]);
    EXPECT_EQ(42, trg[2]);
  }
}
}  // anonymous namespace

TEST(SmallVectorTest, CopySeparation) {
  // Small vectors.
  TestCopySeparation<10, 7, 7>();
  TestCopySeparation<10, 5, 7>();
  TestCopySeparation<10, 9, 7>();
  // Big vectors.
  TestCopySeparation<10, 17, 17>();
  TestCopySeparation<10, 15, 17>();
  TestCopySeparation<10, 19, 17>();
  // Small assigned to big.
  TestCopySeparation<10, 17, 7>();
  // Big assigned to small.
  TestCopySeparation<10, 7, 17>();
}

namespace {
template <size_t capacity, size_t size>
void TestIteratorsTrivial() {
  constexpr int isize = static_cast<int>(size);
  SmallVector<int, capacity> v{make_int_list<size, 1>()};

  // Forward.
  {
    auto it = v.begin();
    for (int i = 0; i < isize; ++i) {
      if (i == 0) EXPECT_EQ(*it, v.front());
      if (i == isize - 1) EXPECT_EQ(*it, v.back());
      EXPECT_EQ(*it, v[i]);
      EXPECT_EQ(*it, v.at(i));
      EXPECT_EQ(*it, i + 1);
      ++it;
    }
    EXPECT_EQ(it, v.end());
    for (int i = isize - 1; i >= 0; --i) {
      --it;
      if (i == 0) EXPECT_EQ(*it, v.front());
      if (i == isize - 1) EXPECT_EQ(*it, v.back());
      EXPECT_EQ(*it, v[i]);
      EXPECT_EQ(*it, v.at(i));
      EXPECT_EQ(*it, i + 1);
    }
    EXPECT_EQ(it, v.begin());
  }

  // Reverse.
  {
    auto rit = v.rbegin();
    for (int i = isize - 1; i >= 0; --i) {
      if (i == 0) EXPECT_EQ(*rit, v.front());
      if (i == isize - 1) EXPECT_EQ(*rit, v.back());
      EXPECT_EQ(*rit, v[i]);
      EXPECT_EQ(*rit, v.at(i));
      EXPECT_EQ(*rit, i + 1);
      ++rit;
    }
    EXPECT_EQ(rit, v.rend());
    for (int i = 0; i < isize; ++i) {
      --rit;
      if (i == 0) EXPECT_EQ(*rit, v.front());
      if (i == isize - 1) EXPECT_EQ(*rit, v.back());
      EXPECT_EQ(*rit, v[i]);
      EXPECT_EQ(*rit, v.at(i));
      EXPECT_EQ(*rit, i + 1);
    }
    EXPECT_EQ(rit, v.rbegin());
  }

  // For loops.
  {
    int s = 0;
    for (int x : v) s += x;
    EXPECT_EQ(isize * (isize + 1) / 2, s);
    s = 0;
    for (const int& x : v) s += x;
    EXPECT_EQ(isize * (isize + 1) / 2, s);
    for (int& x : v) x += 1000;
    for (int i = 0; i < isize; ++i) EXPECT_EQ(i + 1001, v[i]);
  }
}
}  // anonymous namespace

TEST(SmallVectorTest, IteratorsTrivial) {
  TestIteratorsTrivial<10, 2>();
  TestIteratorsTrivial<10, 2>();
  TestIteratorsTrivial<10, 7>();
  TestIteratorsTrivial<10, 17>();
  TestIteratorsTrivial<0, 7>();
}

namespace {
template <size_t capacity, size_t size>
void TestAccessorsTrivial() {
  constexpr int isize = static_cast<int>(size);
  static_assert(size >= 2);
  SmallVector<int, capacity> v{make_int_list<size, 1>()};

  // Front.
  {
    EXPECT_EQ(v[0], v.front());
    EXPECT_EQ(1, v.front());
    v.front() += 1000;
    EXPECT_EQ(v[0], v.front());
    EXPECT_EQ(1001, v.front());
    v.front() -= 1000;
    EXPECT_EQ(v[0], v.front());
    EXPECT_EQ(1, v.front());
  }

  // Back.
  {
    EXPECT_EQ(v[size - 1], v.back());
    EXPECT_EQ(isize, v.back());
    v.back() += 1000;
    EXPECT_EQ(v[size - 1], v.back());
    EXPECT_EQ(1000 + isize, v.back());
    v.back() -= 1000;
    EXPECT_EQ(v[size - 1], v.back());
    EXPECT_EQ(isize, v.back());
  }

  // At, operator[].
  {
    for (int i = 0; i < isize; ++i) {
      EXPECT_EQ(v[i], v.at(i));
      EXPECT_EQ(i + 1, v.at(i));
      v.at(i) += 1000;
      EXPECT_EQ(v[i], v.at(i));
      EXPECT_EQ(i + 1001, v.at(i));
      v.at(i) -= 1000;
      EXPECT_EQ(v[i], v.at(i));
      EXPECT_EQ(i + 1, v.at(i));
    }
  }
}
}  // anonymous namespace

TEST(SmallVectorTest, AccessorsTrivial) {
  TestAccessorsTrivial<10, 2>();
  TestAccessorsTrivial<10, 7>();
  TestAccessorsTrivial<10, 17>();
  TestAccessorsTrivial<0, 7>();
}

namespace {
template <size_t capacity, size_t size, size_t added>
void TestPushPopTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int iadded = static_cast<int>(added);
  static_assert(added >= 2);
  SmallVector<int, capacity> v{make_int_list<size, 1>()};

  // Push a few elements.
  for (int i = 0; i < iadded; ++i) {
    v.push_back(1000 + i);
    EXPECT_EQ(size + i + 1, v.size());
    EXPECT_LE(size + i + 1, v.capacity());
    EXPECT_EQ(1000 + i, v[size + i]);
  }

  // Pop one element.
  EXPECT_EQ(1000 + iadded - 1, v.back());
  v.pop_back();
  EXPECT_EQ(size + added - 1, v.size());
  EXPECT_LE(size + added - 1, v.capacity());
  EXPECT_EQ(1000 + iadded - 2, v.back());

  // Pop the remaining elements that were added.
  v.pop_back(added - 1);
  EXPECT_EQ(size, v.size());
  EXPECT_LE(size, v.capacity());
  if constexpr (size > 0) EXPECT_EQ(isize, v.back());

  // Pop all elements.
  v.pop_back(size);
  EXPECT_EQ(0UL, v.size());
  EXPECT_LE(size, v.capacity());
  EXPECT_TRUE(v.empty());
  EXPECT_EQ(v.begin(), v.end());
  EXPECT_EQ(v.rbegin(), v.rend());
}
}  // anonymous namespace

TEST(SmallVectorTest, PushPopTrivial) {
  TestPushPopTrivial<10, 7, 2>();
  TestPushPopTrivial<10, 7, 3>();
  TestPushPopTrivial<10, 7, 5>();
  TestPushPopTrivial<10, 10, 2>();
  TestPushPopTrivial<10, 17, 2>();
  TestPushPopTrivial<0, 0, 7>();
  TestPushPopTrivial<0, 0, 17>();
}

namespace {
template <size_t capacity, size_t size, size_t added>
void TestEmplacePopTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int iadded = static_cast<int>(added);
  static_assert(added >= 2);
  SmallVector<int, capacity> v{make_int_list<size, 1>()};

  // Push a few elements.
  for (int i = 0; i < iadded; ++i) {
    v.emplace_back(1000 + i);
    EXPECT_EQ(size + i + 1, v.size());
    EXPECT_LE(size + i + 1, v.capacity());
    EXPECT_EQ(1000 + i, v[size + i]);
  }

  // Pop one element.
  EXPECT_EQ(1000 + iadded - 1, v.back());
  v.pop_back();
  EXPECT_EQ(size + added - 1, v.size());
  EXPECT_LE(size + added - 1, v.capacity());
  EXPECT_EQ(1000 + iadded - 2, v.back());

  // Pop the remaining elements that were added.
  v.pop_back(added - 1);
  EXPECT_EQ(size, v.size());
  EXPECT_LE(size, v.capacity());
  if constexpr (size > 0) EXPECT_EQ(isize, v.back());

  // Pop all elements.
  v.pop_back(size);
  EXPECT_EQ(0UL, v.size());
  EXPECT_LE(size, v.capacity());
  EXPECT_TRUE(v.empty());
  EXPECT_EQ(v.begin(), v.end());
  EXPECT_EQ(v.rbegin(), v.rend());
}
}  // anonymous namespace

TEST(SmallVectorTest, EmplacePopTrivial) {
  TestEmplacePopTrivial<10, 7, 2>();
  TestEmplacePopTrivial<10, 7, 3>();
  TestEmplacePopTrivial<10, 7, 5>();
  TestEmplacePopTrivial<10, 10, 2>();
  TestEmplacePopTrivial<10, 17, 2>();
  TestEmplacePopTrivial<0, 0, 7>();
  TestEmplacePopTrivial<0, 0, 17>();
}

namespace {
template <size_t capacity, size_t size, size_t pos>
void TestInsertSimpleTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int ipos = static_cast<int>(pos);
  static_assert(pos <= size);
  SmallVector<int, capacity> v{make_int_list<size, 1>()};

  v.insert(v.begin() + pos, 1000);
  EXPECT_EQ(size + 1, v.size());
  EXPECT_LE(size + 1, v.capacity());
  for (int i = 0; i < ipos; ++i) EXPECT_EQ(i + 1, v[i]);
  EXPECT_EQ(1000, v[pos]);
  for (int i = ipos + 1; i < isize + 1; ++i) EXPECT_EQ(i, v[i]);
}
}  // anonymous namespace

TEST(SmallVectorTest, InsertSimpleTrivial) {
  TestInsertSimpleTrivial<10, 7, 0>();
  TestInsertSimpleTrivial<10, 7, 4>();
  TestInsertSimpleTrivial<10, 7, 7>();
  TestInsertSimpleTrivial<10, 10, 0>();
  TestInsertSimpleTrivial<10, 10, 4>();
  TestInsertSimpleTrivial<10, 10, 7>();
  TestInsertSimpleTrivial<10, 17, 0>();
  TestInsertSimpleTrivial<10, 17, 13>();
  TestInsertSimpleTrivial<10, 17, 17>();
  TestInsertSimpleTrivial<0, 0, 0>();
  TestInsertSimpleTrivial<0, 7, 0>();
  TestInsertSimpleTrivial<0, 7, 4>();
  TestInsertSimpleTrivial<0, 7, 7>();
}

namespace {
template <size_t capacity, size_t size, size_t pos, size_t added>
void TestInsertMultipleTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int ipos = static_cast<int>(pos);
  constexpr int iadded = static_cast<int>(added);
  static_assert(pos <= size);
  SmallVector<int, capacity> v{make_int_list<size, 1>()};

  v.insert(v.begin() + pos, added, 1000);
  EXPECT_EQ(size + added, v.size());
  EXPECT_LE(size + added, v.capacity());
  for (int i = 0; i < ipos; ++i) EXPECT_EQ(i + 1, v[i]);
  for (int i = ipos; i < ipos + iadded; ++i) EXPECT_EQ(1000, v[i]);
  for (int i = ipos + iadded; i < isize + iadded; ++i)
    EXPECT_EQ(i - iadded + 1, v[i]);
}
}  // anonymous namespace

TEST(SmallVectorTest, InsertMultipleTrivial) {
  TestInsertMultipleTrivial<10, 7, 0, 2>();
  TestInsertMultipleTrivial<10, 7, 4, 2>();
  TestInsertMultipleTrivial<10, 7, 7, 2>();
  TestInsertMultipleTrivial<10, 7, 0, 3>();
  TestInsertMultipleTrivial<10, 7, 4, 3>();
  TestInsertMultipleTrivial<10, 7, 7, 3>();
  TestInsertMultipleTrivial<10, 7, 0, 4>();
  TestInsertMultipleTrivial<10, 7, 4, 4>();
  TestInsertMultipleTrivial<10, 7, 7, 4>();
  TestInsertMultipleTrivial<10, 10, 0, 2>();
  TestInsertMultipleTrivial<10, 10, 4, 2>();
  TestInsertMultipleTrivial<10, 10, 7, 2>();
  TestInsertMultipleTrivial<10, 17, 0, 2>();
  TestInsertMultipleTrivial<10, 17, 13, 2>();
  TestInsertMultipleTrivial<10, 17, 17, 2>();
  TestInsertMultipleTrivial<0, 0, 0, 2>();
  TestInsertMultipleTrivial<0, 7, 0, 2>();
  TestInsertMultipleTrivial<0, 7, 4, 2>();
  TestInsertMultipleTrivial<0, 7, 7, 2>();
}

namespace {
template <size_t capacity, size_t size, size_t pos, size_t added>
void TestInsertListTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int ipos = static_cast<int>(pos);
  constexpr int iadded = static_cast<int>(added);
  static_assert(pos <= size);
  SmallVector<int, capacity> v{make_int_list<size, 1>()};

  v.insert(v.begin() + pos, make_int_list<added, 1000>());
  EXPECT_EQ(size + added, v.size());
  EXPECT_LE(size + added, v.capacity());
  for (int i = 0; i < ipos; ++i) EXPECT_EQ(i + 1, v[i]);
  for (int i = ipos; i < ipos + iadded; ++i) EXPECT_EQ(1000 + i - ipos, v[i]);
  for (int i = ipos + iadded; i < isize + iadded; ++i)
    EXPECT_EQ(i - iadded + 1, v[i]);
}
}  // anonymous namespace

TEST(SmallVectorTest, InsertListTrivial) {
  TestInsertListTrivial<10, 7, 0, 2>();
  TestInsertListTrivial<10, 7, 4, 2>();
  TestInsertListTrivial<10, 7, 7, 2>();
  TestInsertListTrivial<10, 7, 0, 3>();
  TestInsertListTrivial<10, 7, 4, 3>();
  TestInsertListTrivial<10, 7, 7, 3>();
  TestInsertListTrivial<10, 7, 0, 4>();
  TestInsertListTrivial<10, 7, 4, 4>();
  TestInsertListTrivial<10, 7, 7, 4>();
  TestInsertListTrivial<10, 10, 0, 2>();
  TestInsertListTrivial<10, 10, 4, 2>();
  TestInsertListTrivial<10, 10, 7, 2>();
  TestInsertListTrivial<10, 17, 0, 2>();
  TestInsertListTrivial<10, 17, 13, 2>();
  TestInsertListTrivial<10, 17, 17, 2>();
  TestInsertListTrivial<0, 0, 0, 2>();
  TestInsertListTrivial<0, 7, 0, 2>();
  TestInsertListTrivial<0, 7, 4, 2>();
  TestInsertListTrivial<0, 7, 7, 2>();
}

namespace {
template <size_t capacity, size_t size, size_t pos, size_t added>
void TestInsertIterTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int ipos = static_cast<int>(pos);
  constexpr int iadded = static_cast<int>(added);
  static_assert(pos <= size);
  SmallVector<int, capacity> v{make_int_list<size, 1>()};
  std::vector<int> vec(make_int_list<added, 1000>());

  v.insert(v.begin() + pos, vec.begin(), vec.end());
  EXPECT_EQ(size + added, v.size());
  EXPECT_LE(size + added, v.capacity());
  for (int i = 0; i < ipos; ++i) EXPECT_EQ(i + 1, v[i]);
  for (int i = ipos; i < ipos + iadded; ++i) EXPECT_EQ(1000 + i - ipos, v[i]);
  for (int i = ipos + iadded; i < isize + iadded; ++i)
    EXPECT_EQ(i - iadded + 1, v[i]);
}
}  // anonymous namespace

TEST(SmallVectorTest, InsertIterTrivial) {
  TestInsertIterTrivial<10, 7, 0, 2>();
  TestInsertIterTrivial<10, 7, 4, 2>();
  TestInsertIterTrivial<10, 7, 7, 2>();
  TestInsertIterTrivial<10, 7, 0, 3>();
  TestInsertIterTrivial<10, 7, 4, 3>();
  TestInsertIterTrivial<10, 7, 7, 3>();
  TestInsertIterTrivial<10, 7, 0, 4>();
  TestInsertIterTrivial<10, 7, 4, 4>();
  TestInsertIterTrivial<10, 7, 7, 4>();
  TestInsertIterTrivial<10, 10, 0, 2>();
  TestInsertIterTrivial<10, 10, 4, 2>();
  TestInsertIterTrivial<10, 10, 7, 2>();
  TestInsertIterTrivial<10, 17, 0, 2>();
  TestInsertIterTrivial<10, 17, 13, 2>();
  TestInsertIterTrivial<10, 17, 17, 2>();
  TestInsertIterTrivial<0, 0, 0, 2>();
  TestInsertIterTrivial<0, 7, 0, 2>();
  TestInsertIterTrivial<0, 7, 4, 2>();
  TestInsertIterTrivial<0, 7, 7, 2>();
}

namespace {
template <size_t capacity, size_t size, size_t pos>
void TestEraseTrivial() {
  constexpr int ipos = static_cast<int>(pos);
  static_assert(pos <= size);
  SmallVector<int, capacity> v{make_int_list<size, 1>()};
  const size_t real_capacity = v.capacity();

  v.erase(v.begin() + pos);
  EXPECT_EQ(pos, v.size());
  EXPECT_EQ(real_capacity, v.capacity());
  for (int i = 0; i < ipos; ++i) EXPECT_EQ(i + 1, v[i]);
}
}  // anonymous namespace

TEST(SmallVectorTest, EraseTrivial) {
  TestEraseTrivial<10, 7, 0>();
  TestEraseTrivial<10, 7, 4>();
  TestEraseTrivial<10, 7, 7>();
  TestEraseTrivial<10, 10, 0>();
  TestEraseTrivial<10, 10, 4>();
  TestEraseTrivial<10, 10, 7>();
  TestEraseTrivial<10, 17, 0>();
  TestEraseTrivial<10, 17, 13>();
  TestEraseTrivial<10, 17, 17>();
  TestEraseTrivial<0, 0, 0>();
  TestEraseTrivial<0, 7, 0>();
  TestEraseTrivial<0, 7, 4>();
  TestEraseTrivial<0, 7, 7>();
}

namespace {
template <size_t capacity, size_t size, size_t new_size>
void TestResizeTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int new_isize = static_cast<int>(new_size);
  SmallVector<int, capacity> v{make_int_list<size, 1>()};
  const size_t real_capacity = v.capacity();

  v.resize(new_size);
  EXPECT_EQ(new_size, v.size());
  if (new_size <= real_capacity) {
    EXPECT_EQ(real_capacity, v.capacity());
  } else {
    EXPECT_LE(new_size, v.capacity());
  }
  for (int i = 0; i < std::min(isize, new_isize); ++i) EXPECT_EQ(i + 1, v[i]);
}
}  // anonymous namespace

TEST(SmallVectorTest, ResizeTrivial) {
  TestResizeTrivial<10, 7, 0>();
  TestResizeTrivial<10, 7, 4>();
  TestResizeTrivial<10, 7, 7>();
  TestResizeTrivial<10, 7, 9>();
  TestResizeTrivial<10, 7, 10>();
  TestResizeTrivial<10, 7, 12>();
  TestResizeTrivial<10, 17, 0>();
  TestResizeTrivial<10, 17, 7>();
  TestResizeTrivial<10, 17, 17>();
  TestResizeTrivial<10, 17, 20>();
  TestResizeTrivial<10, 17, 100>();
  TestResizeTrivial<0, 0, 0>();
  TestResizeTrivial<0, 0, 5>();
  TestResizeTrivial<0, 7, 0>();
  TestResizeTrivial<0, 7, 4>();
  TestResizeTrivial<0, 7, 7>();
  TestResizeTrivial<0, 7, 10>();
  TestResizeTrivial<0, 7, 100>();
}

namespace {
template <size_t capacity, size_t size, size_t new_size>
void TestResizeInitTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int new_isize = static_cast<int>(new_size);
  SmallVector<int, capacity> v{make_int_list<size, 1>()};
  const size_t real_capacity = v.capacity();

  v.resize(new_size, 42);
  EXPECT_EQ(new_size, v.size());
  if (new_size <= real_capacity) {
    EXPECT_EQ(real_capacity, v.capacity());
  } else {
    EXPECT_LE(new_size, v.capacity());
  }
  for (int i = 0; i < std::min(isize, new_isize); ++i) EXPECT_EQ(i + 1, v[i]);
  for (int i = isize; i < new_isize; ++i) EXPECT_EQ(42, v[i]);
}
}  // anonymous namespace

TEST(SmallVectorTest, ResizeInitTrivial) {
  TestResizeInitTrivial<10, 7, 0>();
  TestResizeInitTrivial<10, 7, 4>();
  TestResizeInitTrivial<10, 7, 7>();
  TestResizeInitTrivial<10, 7, 9>();
  TestResizeInitTrivial<10, 7, 10>();
  TestResizeInitTrivial<10, 7, 12>();
  TestResizeInitTrivial<10, 17, 0>();
  TestResizeInitTrivial<10, 17, 7>();
  TestResizeInitTrivial<10, 17, 17>();
  TestResizeInitTrivial<10, 17, 20>();
  TestResizeInitTrivial<10, 17, 100>();
  TestResizeInitTrivial<0, 0, 0>();
  TestResizeInitTrivial<0, 0, 5>();
  TestResizeInitTrivial<0, 7, 0>();
  TestResizeInitTrivial<0, 7, 4>();
  TestResizeInitTrivial<0, 7, 7>();
  TestResizeInitTrivial<0, 7, 10>();
  TestResizeInitTrivial<0, 7, 100>();
}

namespace {
template <size_t capacity, size_t size, size_t new_size>
void TestReserveTrivial() {
  constexpr int isize = static_cast<int>(size);
  SmallVector<int, capacity> v{make_int_list<size, 1>()};
  const size_t real_capacity = v.capacity();

  v.reserve(new_size);
  EXPECT_EQ(size, v.size());
  if (new_size <= real_capacity) {
    EXPECT_EQ(real_capacity, v.capacity());
  } else {
    EXPECT_LE(new_size, v.capacity());
  }
  for (int i = 0; i < isize; ++i) EXPECT_EQ(i + 1, v[i]);
}
}  // anonymous namespace

TEST(SmallVectorTest, ReserveTrivial) {
  TestReserveTrivial<10, 7, 0>();
  TestReserveTrivial<10, 7, 4>();
  TestReserveTrivial<10, 7, 7>();
  TestReserveTrivial<10, 7, 9>();
  TestReserveTrivial<10, 7, 10>();
  TestReserveTrivial<10, 7, 12>();
  TestReserveTrivial<10, 17, 0>();
  TestReserveTrivial<10, 17, 7>();
  TestReserveTrivial<10, 17, 17>();
  TestReserveTrivial<10, 17, 20>();
  TestReserveTrivial<10, 17, 100>();
  TestReserveTrivial<0, 0, 0>();
  TestReserveTrivial<0, 0, 5>();
  TestReserveTrivial<0, 7, 0>();
  TestReserveTrivial<0, 7, 4>();
  TestReserveTrivial<0, 7, 7>();
  TestReserveTrivial<0, 7, 10>();
  TestReserveTrivial<0, 7, 100>();
}

namespace {
template <size_t capacity, size_t size>
void TestClearTrivial() {
  SmallVector<int, capacity> v{make_int_list<size, 1>()};
  const size_t real_capacity = v.capacity();

  v.clear();
  EXPECT_EQ(0UL, v.size());
  EXPECT_EQ(real_capacity, v.capacity());
  EXPECT_TRUE(v.empty());
  EXPECT_EQ(v.begin(), v.end());
  EXPECT_EQ(v.rbegin(), v.rend());
}
}  // anonymous namespace

TEST(SmallVectorTest, ClearTrivial) {
  TestClearTrivial<10, 7>();
  TestClearTrivial<10, 17>();
  TestClearTrivial<0, 0>();
  TestClearTrivial<0, 7>();
}

// Tests with vector elements that are non-trivially constructible/destructible.

namespace {

enum CounterType : int {
  kDefaultConstructor,
  kExplicitConstructor,
  kCopyConstructor,
  kMoveConstructor,
  kDestructor,
  kCopyAssignment,
  kMoveAssignment,
  // The number of immediate counters (array size).
  kSize,
  // The rest is derived counters.
  kAll = kSize,
  kConstructor,
  kAssignment,
};

// A class representing non-trivial object, that count the total number of
// constructor, destructor, and assignment operator calls.
template <typename T>
class NonTrivial {
 public:
  NonTrivial() : constructed_(true), value_() {
    ++counter_[kDefaultConstructor];
  }

  explicit NonTrivial(const T& val) : constructed_(true), value_(val) {
    ++counter_[kExplicitConstructor];
  }

  NonTrivial(const NonTrivial& src) : constructed_(true), value_(src.value()) {
    ++counter_[kCopyConstructor];
  }

  NonTrivial(NonTrivial&& src) : constructed_(true), value_(src.value()) {
    src.value_ = 0;
    ++counter_[kMoveConstructor];
  }

  ~NonTrivial() {
    EXPECT_TRUE(constructed_);
    ++counter_[kDestructor];
    constructed_ = false;
  }

  NonTrivial& operator=(const NonTrivial& src) {
    EXPECT_TRUE(constructed_);
    value_ = src.value();
    ++counter_[kCopyAssignment];
    return *this;
  }

  NonTrivial& operator=(NonTrivial&& src) {
    EXPECT_TRUE(constructed_);
    value_ = src.value();
    src.value_ = 0;
    ++counter_[kMoveAssignment];
    return *this;
  }

  friend bool operator==(const NonTrivial& x, const NonTrivial& y) {
    return x.value() == y.value();
  }

  bool constructed() const { return constructed_; }

  const T& value() const {
    EXPECT_TRUE(constructed_);
    return value_;
  }

  static void ResetCounters() {
    for (int i = 0; i < kSize; ++i) counter_[i] = 0;
  }

  static int GetCounter(CounterType i) {
    switch (i) {
      case kAll:
        return counter_[kDefaultConstructor] + counter_[kExplicitConstructor] +
               counter_[kCopyConstructor] + counter_[kMoveConstructor] +
               counter_[kDestructor] + counter_[kCopyAssignment] +
               counter_[kMoveAssignment];
      case kConstructor:
        return counter_[kDefaultConstructor] + counter_[kExplicitConstructor] +
               counter_[kCopyConstructor] + counter_[kMoveConstructor];
      case kAssignment:
        return counter_[kCopyAssignment] + counter_[kMoveAssignment];
      default:
        CHECK_LT(i, kSize);
        return counter_[i];
    }
  }

 private:
  static std::array<int, kSize> counter_;

  bool constructed_;
  T value_;
};

template <typename T>
std::array<int, kSize> NonTrivial<T>::counter_ = {0};

template <size_t n, int start, int... I>
std::array<NonTrivial<int>, n> make_non_trivial_array_impl(
    std::integer_sequence<int, I...> s) {
  return std::array<NonTrivial<int>, n>{NonTrivial<int>(start + I)...};
}

template <size_t n, int start = 0>
constexpr std::array<NonTrivial<int>, n> make_non_trivial_array() {
  return make_non_trivial_array_impl<n, start>(
      std::make_integer_sequence<int, n>());
}

}  // anonymous namespace

TEST(SmallVectorTest, SimpleConstructNonTrivial) {
  using ElementType = NonTrivial<int>;
  const ElementType nt42(42);
  EXPECT_EQ(1, ElementType::GetCounter(kExplicitConstructor));
  EXPECT_EQ(1, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // A vector with zero capacity.
  {
    SmallVector<ElementType, 0> v;
    EXPECT_EQ(0UL, v.size());
    EXPECT_EQ(0UL, v.capacity());
    EXPECT_TRUE(v.empty());
  }
  EXPECT_EQ(0, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // An implicitly empty small vector.
  {
    SmallVector<ElementType, 10> v;
    EXPECT_EQ(0UL, v.size());
    EXPECT_EQ(10UL, v.capacity());
    EXPECT_TRUE(v.empty());
  }
  EXPECT_EQ(0, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // An explicitly empty small vector.
  {
    SmallVector<ElementType, 10> v(0);
    EXPECT_EQ(0UL, v.size());
    EXPECT_EQ(10UL, v.capacity());
    EXPECT_TRUE(v.empty());
  }
  EXPECT_EQ(0, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // A non-empty, default-initialized small vector.
  {
    SmallVector<ElementType, 10> v(5);
    EXPECT_EQ(5UL, v.size());
    EXPECT_EQ(10UL, v.capacity());
    EXPECT_FALSE(v.empty());
  }
  EXPECT_EQ(5, ElementType::GetCounter(kDefaultConstructor));
  EXPECT_EQ(5, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(10, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // A non-empty, default-initialized big vector.
  {
    SmallVector<ElementType, 10> v(15);
    EXPECT_EQ(15UL, v.size());
    EXPECT_LE(15UL, v.capacity());
    EXPECT_FALSE(v.empty());
  }
  EXPECT_EQ(15, ElementType::GetCounter(kDefaultConstructor));
  EXPECT_EQ(15, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(30, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // A non-empty, explicitly initialized small vector.
  {
    SmallVector<ElementType, 10> v(5, nt42);
    EXPECT_EQ(5UL, v.size());
    EXPECT_EQ(10UL, v.capacity());
    EXPECT_FALSE(v.empty());
    EXPECT_EQ(42, v[3].value());
  }
  EXPECT_EQ(5, ElementType::GetCounter(kCopyConstructor));
  EXPECT_EQ(5, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(10, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // A non-empty, explicitly initialized big vector.
  {
    SmallVector<ElementType, 10> v(15, nt42);
    EXPECT_EQ(15UL, v.size());
    EXPECT_LE(15UL, v.capacity());
    EXPECT_FALSE(v.empty());
    EXPECT_EQ(42, v[13].value());
  }
  EXPECT_EQ(15, ElementType::GetCounter(kCopyConstructor));
  EXPECT_EQ(15, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(30, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();
}

TEST(SmallVectorTest, ConstructFromListNonTrivial) {
  using ElementType = NonTrivial<int>;
  std::initializer_list<ElementType> small{
      ElementType(0), ElementType(1), ElementType(2), ElementType(3),
      ElementType(4), ElementType(5), ElementType(6)};
  std::initializer_list<ElementType> big{
      ElementType(0),  ElementType(1), ElementType(2),  ElementType(3),
      ElementType(4),  ElementType(5), ElementType(6),  ElementType(7),
      ElementType(8),  ElementType(9), ElementType(10), ElementType(11),
      ElementType(12), ElementType(13)};
  EXPECT_EQ(21, ElementType::GetCounter(kExplicitConstructor));
  EXPECT_EQ(21, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // Constructor from initializer list, small.
  {
    SmallVector<ElementType, 10> v{small};
    EXPECT_EQ(7UL, v.size());
    EXPECT_EQ(10UL, v.capacity());
    EXPECT_FALSE(v.empty());
    EXPECT_EQ(5, v[5].value());
  }
  EXPECT_EQ(7, ElementType::GetCounter(kCopyConstructor));
  EXPECT_EQ(7, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(14, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // Constructor from initializer list, big.
  {
    SmallVector<ElementType, 10> v{big};
    EXPECT_EQ(14UL, v.size());
    EXPECT_LE(14UL, v.capacity());
    EXPECT_FALSE(v.empty());
    EXPECT_EQ(3, v[3].value());
    EXPECT_EQ(11, v[11].value());
  }
  EXPECT_EQ(14, ElementType::GetCounter(kCopyConstructor));
  EXPECT_EQ(14, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(28, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();
}

TEST(SmallVectorTest, ConstructFromVectorNonTrivial) {
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<14>();
  EXPECT_EQ(14, ElementType::GetCounter(kExplicitConstructor));
  EXPECT_EQ(14, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // Constructor from base::Vector, small.
  {
    SmallVector<ElementType, 10> v{base::VectorOf(arr.data(), 7)};
    EXPECT_EQ(7UL, v.size());
    EXPECT_EQ(10UL, v.capacity());
    EXPECT_FALSE(v.empty());
    EXPECT_EQ(5, v[5].value());
  }
  EXPECT_EQ(7, ElementType::GetCounter(kCopyConstructor));
  EXPECT_EQ(7, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(14, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // Constructor from base::Vector, big.
  {
    SmallVector<ElementType, 10> v{base::VectorOf(arr.data(), 14)};
    EXPECT_EQ(14UL, v.size());
    EXPECT_LE(14UL, v.capacity());
    EXPECT_FALSE(v.empty());
    EXPECT_EQ(3, v[3].value());
    EXPECT_EQ(11, v[11].value());
  }
  EXPECT_EQ(14, ElementType::GetCounter(kCopyConstructor));
  EXPECT_EQ(14, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(28, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();
}

TEST(SmallVectorTest, CopyConstructNonTrivial) {
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<14>();
  EXPECT_EQ(14, ElementType::GetCounter(kExplicitConstructor));
  EXPECT_EQ(14, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // Copy constructor, small.
  {
    SmallVector<ElementType, 10> v1{base::VectorOf(arr.data(), 7)};
    EXPECT_EQ(7, ElementType::GetCounter(kCopyConstructor));
    EXPECT_EQ(7, ElementType::GetCounter(kAll));
    ElementType::ResetCounters();

    SmallVector<ElementType, 10> v2 = v1;
    EXPECT_EQ(7UL, v1.size());
    EXPECT_EQ(10UL, v1.capacity());
    EXPECT_FALSE(v1.empty());
    EXPECT_EQ(5, v1[5].value());
    EXPECT_EQ(7UL, v2.size());
    EXPECT_EQ(10UL, v2.capacity());
    EXPECT_FALSE(v2.empty());
    EXPECT_EQ(5, v2[5].value());
  }
  EXPECT_EQ(7, ElementType::GetCounter(kCopyConstructor));
  EXPECT_EQ(14, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(21, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // Copy constructor, big.
  {
    SmallVector<ElementType, 10> v1{{base::VectorOf(arr.data(), 14)}};
    EXPECT_EQ(14, ElementType::GetCounter(kCopyConstructor));
    EXPECT_EQ(14, ElementType::GetCounter(kAll));
    ElementType::ResetCounters();

    SmallVector<ElementType, 10> v2 = v1;
    EXPECT_EQ(14UL, v1.size());
    EXPECT_LE(14UL, v1.capacity());
    EXPECT_FALSE(v1.empty());
    EXPECT_EQ(3, v1[3].value());
    EXPECT_EQ(11, v1[11].value());
    EXPECT_EQ(14UL, v2.size());
    EXPECT_LE(14UL, v2.capacity());
    EXPECT_FALSE(v2.empty());
    EXPECT_EQ(3, v2[3].value());
    EXPECT_EQ(11, v2[11].value());
  }
  EXPECT_EQ(14, ElementType::GetCounter(kCopyConstructor));
  EXPECT_EQ(28, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(42, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();
}

TEST(SmallVectorTest, MoveConstructNonTrivial) {
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<14>();
  EXPECT_EQ(14, ElementType::GetCounter(kExplicitConstructor));
  EXPECT_EQ(14, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // Move constructor, small.
  {
    SmallVector<ElementType, 10> v1{base::VectorOf(arr.data(), 7)};
    EXPECT_EQ(7, ElementType::GetCounter(kCopyConstructor));
    EXPECT_EQ(7, ElementType::GetCounter(kAll));
    ElementType::ResetCounters();

    SmallVector<ElementType, 10> v2 = std::move(v1);
    EXPECT_EQ(0UL, v1.size());
    EXPECT_EQ(10UL, v1.capacity());
    EXPECT_TRUE(v1.empty());
    EXPECT_EQ(7UL, v2.size());
    EXPECT_EQ(10UL, v2.capacity());
    EXPECT_FALSE(v2.empty());
    EXPECT_EQ(5, v2[5].value());
  }
  EXPECT_EQ(7, ElementType::GetCounter(kMoveConstructor));
  EXPECT_EQ(14, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(21, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // Move constructor, big.
  {
    SmallVector<ElementType, 10> v1{{base::VectorOf(arr.data(), 14)}};
    EXPECT_EQ(14, ElementType::GetCounter(kCopyConstructor));
    EXPECT_EQ(14, ElementType::GetCounter(kAll));
    ElementType::ResetCounters();

    SmallVector<ElementType, 10> v2 = std::move(v1);
    EXPECT_EQ(0UL, v1.size());
    EXPECT_EQ(10UL, v1.capacity());
    EXPECT_TRUE(v1.empty());
    EXPECT_EQ(14UL, v2.size());
    EXPECT_LE(14UL, v2.capacity());
    EXPECT_FALSE(v2.empty());
    EXPECT_EQ(3, v2[3].value());
    EXPECT_EQ(11, v2[11].value());
  }
  EXPECT_EQ(0, ElementType::GetCounter(kMoveConstructor));
  EXPECT_EQ(14, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(14, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();
}

namespace {
template <size_t capacity, size_t src_size, size_t trg_size>
void TestAssignNonTrivial() {
  constexpr int src_isize = static_cast<int>(src_size);
  constexpr int trg_isize = static_cast<int>(trg_size);
  using ElementType = NonTrivial<int>;
  auto arr_src = make_non_trivial_array<src_size>();
  auto arr_trg = make_non_trivial_array<trg_size, 1000>();
  constexpr bool src_big = src_size > capacity;

  SmallVector<ElementType, capacity> src{base::VectorOf(arr_src)};
  SmallVector<ElementType, capacity> trg{base::VectorOf(arr_trg)};
  ElementType::ResetCounters();
  {
    trg = src;
    EXPECT_EQ(src_size, src.size());
    EXPECT_LE(capacity, src.capacity());
    EXPECT_TRUE((src_size == 0) == src.empty());
    EXPECT_EQ(src_size, trg.size());
    EXPECT_LE(src_size, trg.capacity());
    EXPECT_LE(capacity, trg.capacity());
    EXPECT_TRUE((src_isize == 0) == trg.empty());
    for (int i = 0; i < src_isize; ++i) {
      EXPECT_EQ(i, src[i].value());
      EXPECT_EQ(i, trg[i].value());
    }
  }
  EXPECT_EQ(src_isize, ElementType::GetCounter(kCopyConstructor) +
                           ElementType::GetCounter(kCopyAssignment));
  if constexpr (!src_big) {
    EXPECT_EQ(std::min(src_isize, trg_isize),
              ElementType::GetCounter(kCopyAssignment));
  }
  EXPECT_EQ(trg_isize, ElementType::GetCounter(kDestructor) +
                           ElementType::GetCounter(kCopyAssignment));
  EXPECT_EQ(ElementType::GetCounter(kAll),
            ElementType::GetCounter(kCopyConstructor) +
                ElementType::GetCounter(kDestructor) +
                ElementType::GetCounter(kCopyAssignment));
  ElementType::ResetCounters();
}
}  // anonymous namespace

TEST(SmallVectorTest, AssignNonTrivial) {
  // Small vectors.
  TestAssignNonTrivial<10, 7, 7>();
  TestAssignNonTrivial<10, 5, 7>();
  TestAssignNonTrivial<10, 9, 7>();
  TestAssignNonTrivial<0, 0, 0>();
  // Big vectors.
  TestAssignNonTrivial<10, 17, 17>();
  TestAssignNonTrivial<10, 15, 17>();
  TestAssignNonTrivial<10, 19, 17>();
  TestAssignNonTrivial<0, 7, 7>();
  TestAssignNonTrivial<0, 7, 5>();
  TestAssignNonTrivial<0, 5, 7>();
  // Small assigned to big.
  TestAssignNonTrivial<10, 17, 7>();
  TestAssignNonTrivial<0, 17, 0>();
  // Big assigned to small.
  TestAssignNonTrivial<10, 7, 17>();
  TestAssignNonTrivial<0, 0, 17>();
}

namespace {
template <size_t capacity, size_t src_size, size_t trg_size>
void TestMoveAssignNonTrivial() {
  constexpr int src_isize = static_cast<int>(src_size);
  constexpr int trg_isize = static_cast<int>(trg_size);
  using ElementType = NonTrivial<int>;
  auto arr_src = make_non_trivial_array<src_size>();
  auto arr_trg = make_non_trivial_array<trg_size, 1000>();
  constexpr bool src_big = src_size > capacity;

  SmallVector<ElementType, capacity> src{base::VectorOf(arr_src)};
  SmallVector<ElementType, capacity> trg{base::VectorOf(arr_trg)};
  ElementType::ResetCounters();
  {
    trg = std::move(src);
    EXPECT_EQ(0UL, src.size());
    EXPECT_EQ(capacity, src.capacity());
    EXPECT_TRUE(src.empty());
    EXPECT_EQ(src_size, trg.size());
    EXPECT_LE(src_size, trg.capacity());
    EXPECT_LE(capacity, trg.capacity());
    EXPECT_TRUE((src_size == 0) == trg.empty());
    for (int i = 0; i < src_isize; ++i) {
      EXPECT_EQ(i, trg[i].value());
    }
  }

  if constexpr (!src_big) {
    EXPECT_EQ(src_isize, ElementType::GetCounter(kMoveConstructor) +
                             ElementType::GetCounter(kMoveAssignment));
    EXPECT_EQ(std::min(src_isize, trg_isize),
              ElementType::GetCounter(kMoveAssignment));
    EXPECT_EQ(std::max(src_isize, trg_isize),
              ElementType::GetCounter(kDestructor));
  } else {
    EXPECT_EQ(0, ElementType::GetCounter(kMoveConstructor) +
                     ElementType::GetCounter(kMoveAssignment));
    EXPECT_EQ(trg_isize, ElementType::GetCounter(kDestructor));
  }
  EXPECT_EQ(ElementType::GetCounter(kAll),
            ElementType::GetCounter(kMoveConstructor) +
                ElementType::GetCounter(kDestructor) +
                ElementType::GetCounter(kMoveAssignment));
  ElementType::ResetCounters();
}
}  // anonymous namespace

TEST(SmallVectorTest, MoveAssignNonTrivial) {
  // Small vectors.
  TestMoveAssignNonTrivial<10, 7, 7>();
  TestMoveAssignNonTrivial<10, 5, 7>();
  TestMoveAssignNonTrivial<10, 9, 7>();
  TestMoveAssignNonTrivial<0, 0, 0>();
  // Big vectors.
  TestMoveAssignNonTrivial<10, 17, 17>();
  TestMoveAssignNonTrivial<10, 15, 17>();
  TestMoveAssignNonTrivial<10, 19, 17>();
  TestMoveAssignNonTrivial<0, 7, 7>();
  TestMoveAssignNonTrivial<0, 7, 5>();
  TestMoveAssignNonTrivial<0, 5, 7>();
  // Small assigned to big.
  TestMoveAssignNonTrivial<10, 17, 7>();
  TestMoveAssignNonTrivial<0, 17, 0>();
  // Big assigned to small.
  TestMoveAssignNonTrivial<10, 7, 17>();
  TestMoveAssignNonTrivial<0, 0, 17>();
}

namespace {
template <size_t capacity, size_t size>
void TestIteratorsNonTrivial() {
  constexpr int isize = static_cast<int>(size);
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<size, 1>();
  SmallVector<ElementType, capacity> v{base::VectorOf(arr)};
  ElementType::ResetCounters();

  // Forward.
  {
    auto it = v.begin();
    for (int i = 0; i < isize; ++i) {
      if (i == 0) EXPECT_EQ(*it, v.front());
      if (i == isize - 1) EXPECT_EQ(*it, v.back());
      EXPECT_EQ(*it, v[i]);
      EXPECT_EQ(*it, v.at(i));
      EXPECT_EQ(it->value(), i + 1);
      ++it;
    }
    EXPECT_EQ(it, v.end());
    for (int i = isize - 1; i >= 0; --i) {
      --it;
      if (i == 0) EXPECT_EQ(*it, v.front());
      if (i == isize - 1) EXPECT_EQ(*it, v.back());
      EXPECT_EQ(*it, v[i]);
      EXPECT_EQ(*it, v.at(i));
      EXPECT_EQ(it->value(), i + 1);
    }
    EXPECT_EQ(it, v.begin());
  }
  EXPECT_EQ(0, ElementType::GetCounter(kAll));

  // Reverse.
  {
    auto rit = v.rbegin();
    for (int i = isize - 1; i >= 0; --i) {
      if (i == 0) EXPECT_EQ(*rit, v.front());
      if (i == isize - 1) EXPECT_EQ(*rit, v.back());
      EXPECT_EQ(*rit, v[i]);
      EXPECT_EQ(*rit, v.at(i));
      EXPECT_EQ(rit->value(), i + 1);
      ++rit;
    }
    EXPECT_EQ(rit, v.rend());
    for (int i = 0; i < isize; ++i) {
      --rit;
      if (i == 0) EXPECT_EQ(*rit, v.front());
      if (i == isize - 1) EXPECT_EQ(*rit, v.back());
      EXPECT_EQ(*rit, v[i]);
      EXPECT_EQ(*rit, v.at(i));
      EXPECT_EQ(rit->value(), i + 1);
    }
    EXPECT_EQ(rit, v.rbegin());
  }
  EXPECT_EQ(0, ElementType::GetCounter(kAll));

  // For loops.
  {
    int s = 0;
    for (ElementType x : v) s += x.value();
    EXPECT_EQ(isize * (isize + 1) / 2, s);
  }
  EXPECT_EQ(isize, ElementType::GetCounter(kCopyConstructor));
  EXPECT_EQ(isize, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(2 * isize, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();
  {
    int s = 0;
    for (const ElementType& x : v) s += x.value();
    EXPECT_EQ(isize * (isize + 1) / 2, s);
  }
  EXPECT_EQ(0, ElementType::GetCounter(kAll));
  {
    for (ElementType& x : v) x = ElementType(x.value() + 1000);
    for (int i = 0; i < isize; ++i) EXPECT_EQ(i + 1001, v[i].value());
  }
  EXPECT_EQ(isize, ElementType::GetCounter(kExplicitConstructor));
  EXPECT_EQ(isize, ElementType::GetCounter(kMoveAssignment));
  EXPECT_EQ(isize, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(3 * isize, ElementType::GetCounter(kAll));
}
}  // anonymous namespace

TEST(SmallVectorTest, IteratorsNonTrivial) {
  TestIteratorsNonTrivial<10, 2>();
  TestIteratorsNonTrivial<10, 7>();
  TestIteratorsNonTrivial<10, 17>();
  TestIteratorsNonTrivial<0, 7>();
}

namespace {
template <size_t capacity, size_t size>
void TestAccessorsNonTrivial() {
  constexpr int isize = static_cast<int>(size);
  static_assert(size >= 2);
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<size, 1>();
  SmallVector<ElementType, capacity> v{base::VectorOf(arr)};
  ElementType::ResetCounters();

  // Front.
  {
    EXPECT_EQ(v[0], v.front());
    EXPECT_EQ(1, v.front().value());
    v.front() = ElementType(v.front().value() + 1000);
    EXPECT_EQ(v[0], v.front());
    EXPECT_EQ(1001, v.front().value());
    v.front() = ElementType(v.front().value() - 1000);
    EXPECT_EQ(v[0], v.front());
    EXPECT_EQ(1, v.front().value());
  }
  EXPECT_EQ(2, ElementType::GetCounter(kExplicitConstructor));
  EXPECT_EQ(2, ElementType::GetCounter(kMoveAssignment));
  EXPECT_EQ(2, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(6, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // Back.
  {
    EXPECT_EQ(v[size - 1], v.back());
    EXPECT_EQ(isize, v.back().value());
    v.back() = ElementType(v.back().value() + 1000);
    EXPECT_EQ(v[size - 1], v.back());
    EXPECT_EQ(1000 + isize, v.back().value());
    v.back() = ElementType(v.back().value() - 1000);
    EXPECT_EQ(v[size - 1], v.back());
    EXPECT_EQ(isize, v.back().value());
  }
  EXPECT_EQ(2, ElementType::GetCounter(kExplicitConstructor));
  EXPECT_EQ(2, ElementType::GetCounter(kMoveAssignment));
  EXPECT_EQ(2, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(6, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // At, operator[].
  {
    for (int i = 0; i < isize; ++i) {
      EXPECT_EQ(v[i], v.at(i));
      EXPECT_EQ(i + 1, v.at(i).value());
      v.at(i) = ElementType(v.at(i).value() + 1000);
      EXPECT_EQ(v[i], v.at(i));
      EXPECT_EQ(i + 1001, v.at(i).value());
      v.at(i) = ElementType(v.at(i).value() - 1000);
      EXPECT_EQ(v[i], v.at(i));
      EXPECT_EQ(i + 1, v.at(i).value());
    }
  }
  EXPECT_EQ(2 * isize, ElementType::GetCounter(kExplicitConstructor));
  EXPECT_EQ(2 * isize, ElementType::GetCounter(kMoveAssignment));
  EXPECT_EQ(2 * isize, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(6 * isize, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();
}
}  // anonymous namespace

TEST(SmallVectorTest, AccessorsNonTrivial) {
  TestAccessorsNonTrivial<10, 2>();
  TestAccessorsNonTrivial<10, 7>();
  TestAccessorsNonTrivial<10, 17>();
  TestAccessorsNonTrivial<0, 7>();
}

namespace {
template <size_t capacity, size_t size, size_t added>
void TestPushPopNonTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int iadded = static_cast<int>(added);
  static_assert(added >= 2);
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<size, 1>();
  SmallVector<ElementType, capacity> v{base::VectorOf(arr)};
  ElementType::ResetCounters();

  // Push a few elements.
  for (int i = 0; i < iadded; ++i) {
    v.push_back(ElementType(1000 + i));
    EXPECT_EQ(size + i + 1, v.size());
    EXPECT_LE(size + i + 1, v.capacity());
    EXPECT_EQ(1000 + i, v[size + i].value());
  }
  EXPECT_EQ(iadded, ElementType::GetCounter(kExplicitConstructor));
  if constexpr (size + added <= capacity) {
    EXPECT_EQ(iadded, ElementType::GetCounter(kMoveConstructor));
    EXPECT_EQ(iadded, ElementType::GetCounter(kDestructor));
    EXPECT_EQ(3 * iadded, ElementType::GetCounter(kAll));
  } else {
    EXPECT_LE(iadded, ElementType::GetCounter(kMoveConstructor));
    EXPECT_LE(iadded, ElementType::GetCounter(kDestructor));
  }
  ElementType::ResetCounters();

  // Pop one element.
  {
    EXPECT_EQ(1000 + iadded - 1, v.back().value());
    v.pop_back();
    EXPECT_EQ(size + added - 1, v.size());
    EXPECT_LE(size + added - 1, v.capacity());
    EXPECT_EQ(1000 + iadded - 2, v.back().value());
  }

  // Pop the remaining elements that were added.
  {
    v.pop_back(added - 1);
    EXPECT_EQ(size, v.size());
    EXPECT_LE(size, v.capacity());
    if constexpr (size > 0) EXPECT_EQ(isize, v.back().value());
  }
  EXPECT_EQ(iadded, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(iadded, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // Pop all elements.
  {
    v.pop_back(size);
    EXPECT_EQ(0UL, v.size());
    EXPECT_LE(size, v.capacity());
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.begin(), v.end());
    EXPECT_EQ(v.rbegin(), v.rend());
  }
  EXPECT_EQ(isize, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(isize, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();
}
}  // anonymous namespace

TEST(SmallVectorTest, PushPopNonTrivial) {
  TestPushPopNonTrivial<10, 7, 2>();
  TestPushPopNonTrivial<10, 7, 3>();
  TestPushPopNonTrivial<10, 7, 5>();
  TestPushPopNonTrivial<10, 10, 2>();
  TestPushPopNonTrivial<10, 17, 2>();
  TestPushPopNonTrivial<0, 0, 7>();
  TestPushPopNonTrivial<0, 0, 17>();
}

namespace {
template <size_t capacity, size_t size, size_t added>
void TestEmplacePopNonTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int iadded = static_cast<int>(added);
  static_assert(added >= 2);
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<size, 1>();
  SmallVector<ElementType, capacity> v{base::VectorOf(arr)};
  ElementType::ResetCounters();

  // Push a few elements.
  for (int i = 0; i < iadded; ++i) {
    v.emplace_back(1000 + i);
    EXPECT_EQ(size + i + 1, v.size());
    EXPECT_LE(size + i + 1, v.capacity());
    EXPECT_EQ(1000 + i, v[size + i].value());
  }
  EXPECT_EQ(iadded, ElementType::GetCounter(kExplicitConstructor));
  if constexpr (size + added <= capacity) {
    EXPECT_EQ(iadded, ElementType::GetCounter(kAll));
  }
  ElementType::ResetCounters();

  // Pop one element.
  {
    EXPECT_EQ(1000 + iadded - 1, v.back().value());
    v.pop_back();
    EXPECT_EQ(size + added - 1, v.size());
    EXPECT_LE(size + added - 1, v.capacity());
    EXPECT_EQ(1000 + iadded - 2, v.back().value());
  }

  // Pop the remaining elements that were added.
  {
    v.pop_back(added - 1);
    EXPECT_EQ(size, v.size());
    EXPECT_LE(size, v.capacity());
    if constexpr (size > 0) EXPECT_EQ(isize, v.back().value());
  }
  EXPECT_EQ(iadded, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(iadded, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  // Pop all elements.
  {
    v.pop_back(size);
    EXPECT_EQ(0UL, v.size());
    EXPECT_LE(size, v.capacity());
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.begin(), v.end());
    EXPECT_EQ(v.rbegin(), v.rend());
  }
  EXPECT_EQ(isize, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(isize, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();
}
}  // anonymous namespace

TEST(SmallVectorTest, EmplacePopNonTrivial) {
  TestEmplacePopNonTrivial<10, 7, 2>();
  TestEmplacePopNonTrivial<10, 7, 3>();
  TestEmplacePopNonTrivial<10, 7, 5>();
  TestEmplacePopNonTrivial<10, 10, 2>();
  TestEmplacePopNonTrivial<10, 17, 2>();
  TestEmplacePopNonTrivial<0, 0, 7>();
  TestEmplacePopNonTrivial<0, 0, 17>();
}

namespace {
template <size_t capacity, size_t size, size_t pos>
void TestInsertSimpleNonTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int ipos = static_cast<int>(pos);
  static_assert(pos <= size);
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<size, 1>();
  SmallVector<ElementType, capacity> v{base::VectorOf(arr)};
  const size_t real_capacity = v.capacity();
  ElementType::ResetCounters();

  {
    v.insert(v.begin() + pos, ElementType(1000));
    EXPECT_EQ(size + 1, v.size());
    EXPECT_LE(size + 1, v.capacity());
    for (int i = 0; i < ipos; ++i) EXPECT_EQ(i + 1, v[i].value());
    EXPECT_EQ(1000, v[pos].value());
    for (int i = ipos + 1; i < isize + 1; ++i) EXPECT_EQ(i, v[i].value());
  }
  if (size + 1 <= real_capacity) {
    EXPECT_EQ(1, ElementType::GetCounter(kDefaultConstructor));
    EXPECT_EQ(1, ElementType::GetCounter(kExplicitConstructor));
    EXPECT_EQ(1, ElementType::GetCounter(kCopyAssignment));
    EXPECT_EQ(isize - ipos, ElementType::GetCounter(kMoveAssignment));
    EXPECT_EQ(1, ElementType::GetCounter(kDestructor));
    EXPECT_EQ(isize - ipos + 4, ElementType::GetCounter(kAll));
  } else {
    EXPECT_EQ(1, ElementType::GetCounter(kDefaultConstructor));
    EXPECT_EQ(1, ElementType::GetCounter(kExplicitConstructor));
    EXPECT_EQ(isize, ElementType::GetCounter(kMoveConstructor));
    EXPECT_EQ(1, ElementType::GetCounter(kCopyAssignment));
    EXPECT_EQ(isize - ipos, ElementType::GetCounter(kMoveAssignment));
    EXPECT_EQ(isize + 1, ElementType::GetCounter(kDestructor));
    EXPECT_EQ(3 * isize - ipos + 4, ElementType::GetCounter(kAll));
  }
  ElementType::ResetCounters();
}
}  // anonymous namespace

TEST(SmallVectorTest, InsertSimpleNonTrivial) {
  TestInsertSimpleNonTrivial<10, 7, 0>();
  TestInsertSimpleNonTrivial<10, 7, 4>();
  TestInsertSimpleNonTrivial<10, 7, 7>();
  TestInsertSimpleNonTrivial<10, 10, 0>();
  TestInsertSimpleNonTrivial<10, 10, 4>();
  TestInsertSimpleNonTrivial<10, 10, 7>();
  TestInsertSimpleNonTrivial<10, 17, 0>();
  TestInsertSimpleNonTrivial<10, 17, 13>();
  TestInsertSimpleNonTrivial<10, 17, 17>();
  TestInsertSimpleNonTrivial<0, 0, 0>();
  TestInsertSimpleNonTrivial<0, 7, 0>();
  TestInsertSimpleNonTrivial<0, 7, 4>();
  TestInsertSimpleNonTrivial<0, 7, 7>();
}

namespace {
template <size_t capacity, size_t size, size_t pos, size_t added>
void TestInsertMultipleNonTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int ipos = static_cast<int>(pos);
  constexpr int iadded = static_cast<int>(added);
  static_assert(pos <= size);
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<size, 1>();
  SmallVector<ElementType, capacity> v{base::VectorOf(arr)};
  const size_t real_capacity = v.capacity();
  ElementType::ResetCounters();

  {
    v.insert(v.begin() + pos, added, ElementType(1000));
    EXPECT_EQ(size + added, v.size());
    EXPECT_LE(size + added, v.capacity());
    for (int i = 0; i < ipos; ++i) EXPECT_EQ(i + 1, v[i].value());
    for (int i = ipos; i < ipos + iadded; ++i) EXPECT_EQ(1000, v[i].value());
    for (int i = ipos + iadded; i < isize + iadded; ++i)
      EXPECT_EQ(i - iadded + 1, v[i].value());
  }
  if (size + added <= real_capacity) {
    EXPECT_EQ(iadded, ElementType::GetCounter(kDefaultConstructor));
    EXPECT_EQ(1, ElementType::GetCounter(kExplicitConstructor));
    EXPECT_EQ(iadded, ElementType::GetCounter(kCopyAssignment));
    EXPECT_EQ(isize - ipos, ElementType::GetCounter(kMoveAssignment));
    EXPECT_EQ(1, ElementType::GetCounter(kDestructor));
    EXPECT_EQ(isize - ipos + 2 * iadded + 2, ElementType::GetCounter(kAll));
  } else {
    EXPECT_EQ(iadded, ElementType::GetCounter(kDefaultConstructor));
    EXPECT_EQ(1, ElementType::GetCounter(kExplicitConstructor));
    EXPECT_EQ(isize, ElementType::GetCounter(kMoveConstructor));
    EXPECT_EQ(iadded, ElementType::GetCounter(kCopyAssignment));
    EXPECT_EQ(isize - ipos, ElementType::GetCounter(kMoveAssignment));
    EXPECT_EQ(isize + 1, ElementType::GetCounter(kDestructor));
    EXPECT_EQ(3 * isize - ipos + 2 * iadded + 2, ElementType::GetCounter(kAll));
  }
  ElementType::ResetCounters();
}
}  // anonymous namespace

TEST(SmallVectorTest, InsertMultipleNonTrivial) {
  TestInsertMultipleNonTrivial<10, 7, 0, 2>();
  TestInsertMultipleNonTrivial<10, 7, 4, 2>();
  TestInsertMultipleNonTrivial<10, 7, 7, 2>();
  TestInsertMultipleNonTrivial<10, 7, 0, 3>();
  TestInsertMultipleNonTrivial<10, 7, 4, 3>();
  TestInsertMultipleNonTrivial<10, 7, 7, 3>();
  TestInsertMultipleNonTrivial<10, 7, 0, 4>();
  TestInsertMultipleNonTrivial<10, 7, 4, 4>();
  TestInsertMultipleNonTrivial<10, 7, 7, 4>();
  TestInsertMultipleNonTrivial<10, 10, 0, 2>();
  TestInsertMultipleNonTrivial<10, 10, 4, 2>();
  TestInsertMultipleNonTrivial<10, 10, 7, 2>();
  TestInsertMultipleNonTrivial<10, 17, 0, 2>();
  TestInsertMultipleNonTrivial<10, 17, 13, 2>();
  TestInsertMultipleNonTrivial<10, 17, 17, 2>();
  TestInsertMultipleNonTrivial<0, 0, 0, 2>();
  TestInsertMultipleNonTrivial<0, 7, 0, 2>();
  TestInsertMultipleNonTrivial<0, 7, 4, 2>();
  TestInsertMultipleNonTrivial<0, 7, 7, 2>();
}

namespace {
template <size_t capacity, size_t size, size_t pos, size_t added>
void TestInsertListNonTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int ipos = static_cast<int>(pos);
  constexpr int iadded = static_cast<int>(added);
  static_assert(pos <= size);
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<size, 1>();
  SmallVector<ElementType, capacity> v{base::VectorOf(arr)};
  const size_t real_capacity = v.capacity();
  std::initializer_list<ElementType> v2{ElementType(1000), ElementType(1001)};
  std::initializer_list<ElementType> v3{ElementType(1000), ElementType(1001),
                                        ElementType(1002)};
  std::initializer_list<ElementType> v4{ElementType(1000), ElementType(1001),
                                        ElementType(1002), ElementType(1003)};
  ElementType::ResetCounters();

  {
    switch (added) {
      case 2:
        v.insert(v.begin() + pos, v2);
        break;
      case 3:
        v.insert(v.begin() + pos, v3);
        break;
      case 4:
        v.insert(v.begin() + pos, v4);
        break;
      default:
        UNREACHABLE();
    }
    EXPECT_EQ(size + added, v.size());
    EXPECT_LE(size + added, v.capacity());
    for (int i = 0; i < ipos; ++i) EXPECT_EQ(i + 1, v[i].value());
    for (int i = ipos; i < ipos + iadded; ++i)
      EXPECT_EQ(1000 + i - ipos, v[i].value());
    for (int i = ipos + iadded; i < isize + iadded; ++i)
      EXPECT_EQ(i - iadded + 1, v[i].value());
  }
  if (size + added <= real_capacity) {
    EXPECT_EQ(iadded, ElementType::GetCounter(kDefaultConstructor));
    EXPECT_EQ(iadded, ElementType::GetCounter(kCopyAssignment));
    EXPECT_EQ(isize - ipos, ElementType::GetCounter(kMoveAssignment));
    EXPECT_EQ(isize - ipos + 2 * iadded, ElementType::GetCounter(kAll));
  } else {
    EXPECT_EQ(iadded, ElementType::GetCounter(kDefaultConstructor));
    EXPECT_EQ(isize, ElementType::GetCounter(kMoveConstructor));
    EXPECT_EQ(iadded, ElementType::GetCounter(kCopyAssignment));
    EXPECT_EQ(isize - ipos, ElementType::GetCounter(kMoveAssignment));
    EXPECT_EQ(isize, ElementType::GetCounter(kDestructor));
    EXPECT_EQ(3 * isize - ipos + 2 * iadded, ElementType::GetCounter(kAll));
  }
  ElementType::ResetCounters();
}
}  // anonymous namespace

TEST(SmallVectorTest, InsertListNonTrivial) {
  TestInsertListNonTrivial<10, 7, 0, 2>();
  TestInsertListNonTrivial<10, 7, 4, 2>();
  TestInsertListNonTrivial<10, 7, 7, 2>();
  TestInsertListNonTrivial<10, 7, 0, 3>();
  TestInsertListNonTrivial<10, 7, 4, 3>();
  TestInsertListNonTrivial<10, 7, 7, 3>();
  TestInsertListNonTrivial<10, 7, 0, 4>();
  TestInsertListNonTrivial<10, 7, 4, 4>();
  TestInsertListNonTrivial<10, 7, 7, 4>();
  TestInsertListNonTrivial<10, 10, 0, 2>();
  TestInsertListNonTrivial<10, 10, 4, 2>();
  TestInsertListNonTrivial<10, 10, 7, 2>();
  TestInsertListNonTrivial<10, 17, 0, 2>();
  TestInsertListNonTrivial<10, 17, 13, 2>();
  TestInsertListNonTrivial<10, 17, 17, 2>();
  TestInsertListNonTrivial<0, 0, 0, 2>();
  TestInsertListNonTrivial<0, 7, 0, 2>();
  TestInsertListNonTrivial<0, 7, 4, 2>();
  TestInsertListNonTrivial<0, 7, 7, 2>();
}

namespace {
template <size_t capacity, size_t size, size_t pos, size_t added>
void TestInsertIterNonTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int ipos = static_cast<int>(pos);
  constexpr int iadded = static_cast<int>(added);
  static_assert(pos <= size);
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<size, 1>();
  SmallVector<ElementType, capacity> v{base::VectorOf(arr)};
  const size_t real_capacity = v.capacity();
  auto append = make_non_trivial_array<added, 1000>();
  ElementType::ResetCounters();

  {
    v.insert(v.begin() + pos, append.begin(), append.end());
    EXPECT_EQ(size + added, v.size());
    EXPECT_LE(size + added, v.capacity());
    for (int i = 0; i < ipos; ++i) EXPECT_EQ(i + 1, v[i].value());
    for (int i = ipos; i < ipos + iadded; ++i)
      EXPECT_EQ(1000 + i - ipos, v[i].value());
    for (int i = ipos + iadded; i < isize + iadded; ++i)
      EXPECT_EQ(i - iadded + 1, v[i].value());
  }
  if (size + added <= real_capacity) {
    EXPECT_EQ(iadded, ElementType::GetCounter(kDefaultConstructor));
    EXPECT_EQ(iadded, ElementType::GetCounter(kCopyAssignment));
    EXPECT_EQ(isize - ipos, ElementType::GetCounter(kMoveAssignment));
    EXPECT_EQ(isize - ipos + 2 * iadded, ElementType::GetCounter(kAll));
  } else {
    EXPECT_EQ(iadded, ElementType::GetCounter(kDefaultConstructor));
    EXPECT_EQ(isize, ElementType::GetCounter(kMoveConstructor));
    EXPECT_EQ(iadded, ElementType::GetCounter(kCopyAssignment));
    EXPECT_EQ(isize - ipos, ElementType::GetCounter(kMoveAssignment));
    EXPECT_EQ(isize, ElementType::GetCounter(kDestructor));
    EXPECT_EQ(3 * isize - ipos + 2 * iadded, ElementType::GetCounter(kAll));
  }
  ElementType::ResetCounters();
}
}  // anonymous namespace

TEST(SmallVectorTest, InsertIterNonTrivial) {
  TestInsertIterNonTrivial<10, 7, 0, 2>();
  TestInsertIterNonTrivial<10, 7, 4, 2>();
  TestInsertIterNonTrivial<10, 7, 7, 2>();
  TestInsertIterNonTrivial<10, 7, 0, 3>();
  TestInsertIterNonTrivial<10, 7, 4, 3>();
  TestInsertIterNonTrivial<10, 7, 7, 3>();
  TestInsertIterNonTrivial<10, 7, 0, 4>();
  TestInsertIterNonTrivial<10, 7, 4, 4>();
  TestInsertIterNonTrivial<10, 7, 7, 4>();
  TestInsertIterNonTrivial<10, 10, 0, 2>();
  TestInsertIterNonTrivial<10, 10, 4, 2>();
  TestInsertIterNonTrivial<10, 10, 7, 2>();
  TestInsertIterNonTrivial<10, 17, 0, 2>();
  TestInsertIterNonTrivial<10, 17, 13, 2>();
  TestInsertIterNonTrivial<10, 17, 17, 2>();
  TestInsertIterNonTrivial<0, 0, 0, 2>();
  TestInsertIterNonTrivial<0, 7, 0, 2>();
  TestInsertIterNonTrivial<0, 7, 4, 2>();
  TestInsertIterNonTrivial<0, 7, 7, 2>();
}

namespace {
template <size_t capacity, size_t size, size_t pos>
void TestEraseNonTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int ipos = static_cast<int>(pos);
  static_assert(pos <= size);
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<size, 1>();
  SmallVector<ElementType, capacity> v{base::VectorOf(arr)};
  const size_t real_capacity = v.capacity();
  ElementType::ResetCounters();

  {
    v.erase(v.begin() + pos);
    EXPECT_EQ(pos, v.size());
    EXPECT_EQ(real_capacity, v.capacity());
    for (int i = 0; i < ipos; ++i) EXPECT_EQ(i + 1, v[i].value());
  }
  EXPECT_EQ(isize - ipos, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(isize - ipos, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();
}
}  // anonymous namespace

TEST(SmallVectorTest, EraseNonTrivial) {
  TestEraseNonTrivial<10, 7, 0>();
  TestEraseNonTrivial<10, 7, 4>();
  TestEraseNonTrivial<10, 7, 7>();
  TestEraseNonTrivial<10, 10, 0>();
  TestEraseNonTrivial<10, 10, 4>();
  TestEraseNonTrivial<10, 10, 7>();
  TestEraseNonTrivial<10, 17, 0>();
  TestEraseNonTrivial<10, 17, 13>();
  TestEraseNonTrivial<10, 17, 17>();
  TestEraseNonTrivial<0, 0, 0>();
  TestEraseNonTrivial<0, 7, 0>();
  TestEraseNonTrivial<0, 7, 4>();
  TestEraseNonTrivial<0, 7, 7>();
}

namespace {
constexpr int size_diff(int x, int y) { return x > y ? x - y : 0; }

template <size_t capacity, size_t size, size_t new_size>
void TestResizeNonTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int new_isize = static_cast<int>(new_size);
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<size, 1>();
  SmallVector<ElementType, capacity> v{base::VectorOf(arr)};
  const size_t real_capacity = v.capacity();
  ElementType::ResetCounters();

  {
    v.resize(new_size);
    EXPECT_EQ(new_size, v.size());
    if (new_size <= real_capacity) {
      EXPECT_EQ(real_capacity, v.capacity());
    } else {
      EXPECT_LE(new_size, v.capacity());
    }
    for (int i = 0; i < std::min(isize, new_isize); ++i)
      EXPECT_EQ(i + 1, v[i].value());
  }
  if (new_size <= real_capacity) {
    EXPECT_EQ(size_diff(new_isize, isize),
              ElementType::GetCounter(kDefaultConstructor));
    EXPECT_EQ(size_diff(isize, new_isize),
              ElementType::GetCounter(kDestructor));
    EXPECT_EQ(ElementType::GetCounter(kDefaultConstructor) +
                  ElementType::GetCounter(kDestructor),
              ElementType::GetCounter(kAll));
  } else {
    EXPECT_EQ(size_diff(new_isize, isize),
              ElementType::GetCounter(kDefaultConstructor));
    EXPECT_EQ(isize, ElementType::GetCounter(kMoveConstructor));
    EXPECT_EQ(isize, ElementType::GetCounter(kDestructor));
    EXPECT_EQ(ElementType::GetCounter(kDefaultConstructor) +
                  ElementType::GetCounter(kMoveConstructor) +
                  ElementType::GetCounter(kDestructor),
              ElementType::GetCounter(kAll));
  }
  ElementType::ResetCounters();
}
}  // anonymous namespace

TEST(SmallVectorTest, ResizeNonTrivial) {
  TestResizeNonTrivial<10, 7, 0>();
  TestResizeNonTrivial<10, 7, 4>();
  TestResizeNonTrivial<10, 7, 7>();
  TestResizeNonTrivial<10, 7, 9>();
  TestResizeNonTrivial<10, 7, 10>();
  TestResizeNonTrivial<10, 7, 12>();
  TestResizeNonTrivial<10, 17, 0>();
  TestResizeNonTrivial<10, 17, 7>();
  TestResizeNonTrivial<10, 17, 17>();
  TestResizeNonTrivial<10, 17, 20>();
  TestResizeNonTrivial<10, 17, 100>();
  TestResizeNonTrivial<0, 0, 0>();
  TestResizeNonTrivial<0, 0, 5>();
  TestResizeNonTrivial<0, 7, 0>();
  TestResizeNonTrivial<0, 7, 4>();
  TestResizeNonTrivial<0, 7, 7>();
  TestResizeNonTrivial<0, 7, 10>();
  TestResizeNonTrivial<0, 7, 100>();
}

namespace {
template <size_t capacity, size_t size, size_t new_size>
void TestResizeInitNonTrivial() {
  constexpr int isize = static_cast<int>(size);
  constexpr int new_isize = static_cast<int>(new_size);
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<size, 1>();
  SmallVector<ElementType, capacity> v{base::VectorOf(arr)};
  const size_t real_capacity = v.capacity();
  ElementType::ResetCounters();

  {
    v.resize(new_size, ElementType(42));
    EXPECT_EQ(new_size, v.size());
    if (new_size <= real_capacity) {
      EXPECT_EQ(real_capacity, v.capacity());
    } else {
      EXPECT_LE(new_size, v.capacity());
    }
    for (int i = 0; i < std::min(isize, new_isize); ++i)
      EXPECT_EQ(i + 1, v[i].value());
    for (int i = isize; i < new_isize; ++i) EXPECT_EQ(42, v[i].value());
  }
  if (new_size <= real_capacity) {
    EXPECT_EQ(1, ElementType::GetCounter(kExplicitConstructor));
    EXPECT_EQ(size_diff(new_isize, isize),
              ElementType::GetCounter(kCopyConstructor));
    EXPECT_EQ(size_diff(isize, new_isize) + 1,
              ElementType::GetCounter(kDestructor));
    EXPECT_EQ(ElementType::GetCounter(kExplicitConstructor) +
                  ElementType::GetCounter(kCopyConstructor) +
                  ElementType::GetCounter(kDestructor),
              ElementType::GetCounter(kAll));
  } else {
    EXPECT_EQ(1, ElementType::GetCounter(kExplicitConstructor));
    EXPECT_EQ(size_diff(new_isize, isize),
              ElementType::GetCounter(kCopyConstructor));
    EXPECT_EQ(isize, ElementType::GetCounter(kMoveConstructor));
    EXPECT_EQ(isize + 1, ElementType::GetCounter(kDestructor));
    EXPECT_EQ(ElementType::GetCounter(kExplicitConstructor) +
                  ElementType::GetCounter(kCopyConstructor) +
                  ElementType::GetCounter(kMoveConstructor) +
                  ElementType::GetCounter(kDestructor),
              ElementType::GetCounter(kAll));
  }
  ElementType::ResetCounters();
}
}  // anonymous namespace

TEST(SmallVectorTest, ResizeInitNonTrivial) {
  TestResizeInitNonTrivial<10, 7, 0>();
  TestResizeInitNonTrivial<10, 7, 4>();
  TestResizeInitNonTrivial<10, 7, 7>();
  TestResizeInitNonTrivial<10, 7, 9>();
  TestResizeInitNonTrivial<10, 7, 10>();
  TestResizeInitNonTrivial<10, 7, 12>();
  TestResizeInitNonTrivial<10, 17, 0>();
  TestResizeInitNonTrivial<10, 17, 7>();
  TestResizeInitNonTrivial<10, 17, 17>();
  TestResizeInitNonTrivial<10, 17, 20>();
  TestResizeInitNonTrivial<10, 17, 100>();
  TestResizeInitNonTrivial<0, 0, 0>();
  TestResizeInitNonTrivial<0, 0, 5>();
  TestResizeInitNonTrivial<0, 7, 0>();
  TestResizeInitNonTrivial<0, 7, 4>();
  TestResizeInitNonTrivial<0, 7, 7>();
  TestResizeInitNonTrivial<0, 7, 10>();
  TestResizeInitNonTrivial<0, 7, 100>();
}

namespace {
template <size_t capacity, size_t size, size_t new_size>
void TestReserveNonTrivial() {
  constexpr int isize = static_cast<int>(size);
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<size, 1>();
  SmallVector<ElementType, capacity> v{base::VectorOf(arr)};
  const size_t real_capacity = v.capacity();
  ElementType::ResetCounters();

  {
    v.reserve(new_size);
    EXPECT_EQ(size, v.size());
    if (new_size <= real_capacity) {
      EXPECT_EQ(real_capacity, v.capacity());
    } else {
      EXPECT_LE(new_size, v.capacity());
    }
    for (int i = 0; i < isize; ++i) EXPECT_EQ(i + 1, v[i].value());
  }
  if (new_size <= real_capacity) {
    EXPECT_EQ(0, ElementType::GetCounter(kAll));
  } else {
    EXPECT_EQ(isize, ElementType::GetCounter(kMoveConstructor));
    EXPECT_EQ(isize, ElementType::GetCounter(kDestructor));
    EXPECT_EQ(ElementType::GetCounter(kMoveConstructor) +
                  ElementType::GetCounter(kDestructor),
              ElementType::GetCounter(kAll));
  }
}
}  // anonymous namespace

TEST(SmallVectorTest, ReserveNonTrivial) {
  TestReserveNonTrivial<10, 7, 0>();
  TestReserveNonTrivial<10, 7, 4>();
  TestReserveNonTrivial<10, 7, 7>();
  TestReserveNonTrivial<10, 7, 9>();
  TestReserveNonTrivial<10, 7, 10>();
  TestReserveNonTrivial<10, 7, 12>();
  TestReserveNonTrivial<10, 17, 0>();
  TestReserveNonTrivial<10, 17, 7>();
  TestReserveNonTrivial<10, 17, 17>();
  TestReserveNonTrivial<10, 17, 20>();
  TestReserveNonTrivial<10, 17, 100>();
  TestReserveNonTrivial<0, 0, 0>();
  TestReserveNonTrivial<0, 0, 5>();
  TestReserveNonTrivial<0, 7, 0>();
  TestReserveNonTrivial<0, 7, 4>();
  TestReserveNonTrivial<0, 7, 7>();
  TestReserveNonTrivial<0, 7, 10>();
  TestReserveNonTrivial<0, 7, 100>();
}

namespace {
template <size_t capacity, size_t size>
void TestClearNonTrivial() {
  constexpr int isize = static_cast<int>(size);
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<size, 1>();
  SmallVector<ElementType, capacity> v{base::VectorOf(arr)};
  const size_t real_capacity = v.capacity();
  ElementType::ResetCounters();

  {
    v.clear();
    EXPECT_EQ(0UL, v.size());
    EXPECT_EQ(real_capacity, v.capacity());
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.begin(), v.end());
    EXPECT_EQ(v.rbegin(), v.rend());
  }
  EXPECT_EQ(isize, ElementType::GetCounter(kDestructor));
  EXPECT_EQ(isize, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();
}
}  // anonymous namespace

TEST(SmallVectorTest, ClearNonTrivial) {
  TestClearNonTrivial<10, 7>();
  TestClearNonTrivial<10, 17>();
  TestClearNonTrivial<0, 0>();
  TestClearNonTrivial<0, 7>();
}

namespace {
template <size_t capacity>
void copy_assign(SmallVector<NonTrivial<int>, capacity>& trg,
                 const SmallVector<NonTrivial<int>, capacity>& src) {
  trg = src;
}

template <size_t capacity>
void move_assign(SmallVector<NonTrivial<int>, capacity>& trg,
                 SmallVector<NonTrivial<int>, capacity>&& src) {
  trg = std::move(src);
}

template <size_t capacity, size_t size>
void TestNoAssign() {
  constexpr int isize = static_cast<int>(size);
  using ElementType = NonTrivial<int>;
  auto arr = make_non_trivial_array<size, 1>();
  SmallVector<ElementType, capacity> v{base::VectorOf(arr)};
  const size_t real_capacity = v.capacity();
  ElementType::ResetCounters();

  {
    copy_assign(v, v);
    EXPECT_EQ(size, v.size());
    EXPECT_EQ(real_capacity, v.capacity());
    for (int i = 0; i < isize; ++i) EXPECT_EQ(i + 1, v[i].value());
  }
  EXPECT_EQ(0, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();

  {
    move_assign(v, std::move(v));
    EXPECT_EQ(size, v.size());
    EXPECT_EQ(real_capacity, v.capacity());
    for (int i = 0; i < isize; ++i) EXPECT_EQ(i + 1, v[i].value());
  }
  EXPECT_EQ(0, ElementType::GetCounter(kAll));
  ElementType::ResetCounters();
}
}  // anonymous namespace

TEST(SmallVectorTest, NoAssign) {
  TestNoAssign<10, 7>();
  TestNoAssign<10, 17>();
  TestNoAssign<0, 0>();
  TestNoAssign<0, 7>();
}

}  // namespace v8::base
