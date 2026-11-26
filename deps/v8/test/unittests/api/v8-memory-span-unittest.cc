// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8-memory-span.h"

#include <concepts>
#include <iterator>
#include <memory>
#include <span>
#include <vector>

#include "include/v8-internal.h"
#include "include/v8-local-handle.h"
#include "src/base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

START_ALLOW_USE_DEPRECATED()

TEST(MemorySpanTest, Simple) {
  std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

  v8::MemorySpan<int> span(v.begin(), v.end());
  int count = 0;
  int sum = 0;
  for (int i : span) {
    ++count;
    sum += i;
  }
  EXPECT_EQ(10, count);
  EXPECT_EQ(55, sum);
}

TEST(MemorySpanTest, Empty) {
  std::vector<int> empty;

  v8::MemorySpan<int> span(empty.begin(), empty.end());
  int count = 0;
  for (int i : span) {
    ++count;
    USE(i);
  }
  EXPECT_EQ(0, count);
}

TEST(MemorySpanTest, Reserved) {
  std::vector<int> ready;
  ready.reserve(10);

  v8::MemorySpan<int> span(ready.begin(), ready.end());
  int count = 0;
  for (int i : span) {
    ++count;
    USE(i);
  }
  EXPECT_EQ(0, count);
}

TEST(MemorySpanTest, ImplicitConversions) {
  auto test = [](const v8::MemorySpan<int>& span, int expected_count,
                 int expected_sum) {
    int count = 0;
    int sum = 0;
    for (int i : span) {
      ++count;
      sum += i;
    }
    EXPECT_EQ(expected_count, count);
    EXPECT_EQ(expected_sum, sum);
  };

  {
    int a[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    test(a, 10, 55);
  }
  {
    std::array<int, 10> a{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    test(a, 10, 55);
  }
  {
    std::vector<int> v{1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    test(v, 10, 55);
  }
  {
    std::array<int, 0> empty;
    test(empty, 0, 0);
  }
  {
    std::vector<int> empty;
    test(empty, 0, 0);
  }
  {
    std::vector<int> ready;
    ready.reserve(10);
    test(ready, 0, 0);
  }
}

END_ALLOW_USE_DEPRECATED()

TEST(MemorySpanTest, WrappedIteratorContiguousAndToAddress) {
  struct Base {
    int value;
  };
  struct Derived : Base {};

  using Iter = WrappedIterator<std::vector<Derived>::iterator, Base>;
  using ConstIter =
      WrappedIterator<std::vector<Derived>::const_iterator, const Base>;
  static_assert(std::contiguous_iterator<Iter>);
  static_assert(std::contiguous_iterator<ConstIter>);
  static_assert(std::contiguous_iterator<v8::LocalVector<v8::Value>::iterator>);
  static_assert(
      std::contiguous_iterator<v8::LocalVector<v8::Value>::const_iterator>);
  static_assert(requires(const Iter& it) {
    { std::pointer_traits<Iter>::to_address(it) } -> std::same_as<Base*>;
  });
  static_assert(requires(const ConstIter& it) {
    {
      std::pointer_traits<ConstIter>::to_address(it)
    } -> std::same_as<const Base*>;
  });

  {
    std::vector<Derived> empty;
    Iter begin(empty.begin());
    Iter end(empty.end());
    EXPECT_EQ(std::to_address(begin), std::to_address(end));
    std::span<Base> span(begin, end);
    EXPECT_TRUE(span.empty());
  }
  {
    std::vector<Derived> reserved;
    reserved.reserve(8);
    Iter begin(reserved.begin());
    Iter end(reserved.end());
    EXPECT_EQ(std::to_address(begin), std::to_address(end));
    std::span<Base> span(begin, end);
    EXPECT_TRUE(span.empty());
  }
  {
    std::vector<Derived> values{{{1}, {2}, {3}}};
    Iter begin(values.begin());
    Iter end(values.end());
    EXPECT_EQ(std::to_address(end),
              static_cast<Base*>(values.data()) + values.size());
    std::span<Base> span(begin, end);
    ASSERT_EQ(3u, span.size());
    EXPECT_EQ(1, span[0].value);
    EXPECT_EQ(2, span[1].value);
    EXPECT_EQ(3, span[2].value);

    const auto& const_values = values;
    ConstIter cbegin(const_values.begin());
    ConstIter cend(const_values.end());
    EXPECT_EQ(
        std::to_address(cend),
        static_cast<const Base*>(const_values.data()) + const_values.size());
    std::span<const Base> cspan(cbegin, cend);
    ASSERT_EQ(3u, cspan.size());
    EXPECT_EQ(3, cspan[2].value);
  }
  {
    std::vector<int> ints{1, 2, 3};
    using SameIter = WrappedIterator<std::vector<int>::iterator>;
    using ConstSameIter =
        WrappedIterator<std::vector<int>::iterator, const int>;
    static_assert(std::contiguous_iterator<SameIter>);
    static_assert(std::contiguous_iterator<ConstSameIter>);
    EXPECT_EQ(ints.data(), std::to_address(SameIter(ints.begin())));
    EXPECT_EQ(ints.data() + ints.size(), std::to_address(SameIter(ints.end())));
    EXPECT_EQ(ints.data() + ints.size(),
              std::to_address(ConstSameIter(ints.end())));
  }
  {
    using PtrIter = WrappedIterator<Derived*, Base>;
    using ConstPtrIter = WrappedIterator<const Derived*, const Base>;
    static_assert(std::contiguous_iterator<PtrIter>);
    static_assert(std::contiguous_iterator<ConstPtrIter>);
    static_assert(std::to_address(PtrIter{}) == nullptr);
    static constexpr Derived kArr[2] = {{{1}}, {{2}}};
    static_assert(std::to_address(ConstPtrIter(kArr)) ==
                  static_cast<const Base*>(kArr));
    Derived raw[2] = {{{1}}, {{2}}};
    EXPECT_EQ(static_cast<Base*>(raw) + 2, std::to_address(PtrIter(raw + 2)));
  }
}

}  // namespace internal
}  // namespace v8
