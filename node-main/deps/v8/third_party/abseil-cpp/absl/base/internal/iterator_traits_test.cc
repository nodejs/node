// Copyright 2025 The Abseil Authors
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

#include "absl/base/internal/iterator_traits.h"

#include <forward_list>
#include <iterator>
#include <list>
#include <vector>

#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/base/internal/iterator_traits_test_helper.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace base_internal {
namespace {

TEST(IsAtLeastIteratorTest, IsAtLeastIterator) {
  EXPECT_TRUE((IsAtLeastIterator<std::input_iterator_tag, int*>()));
  EXPECT_TRUE((IsAtLeastIterator<std::forward_iterator_tag, int*>()));
  EXPECT_TRUE((IsAtLeastIterator<std::bidirectional_iterator_tag, int*>()));
  EXPECT_TRUE((IsAtLeastIterator<std::random_access_iterator_tag, int*>()));
  EXPECT_TRUE((IsAtLeastIterator<std::input_iterator_tag,
                                 std::vector<int>::iterator>()));
  EXPECT_TRUE((IsAtLeastIterator<std::forward_iterator_tag,
                                 std::vector<int>::iterator>()));
  EXPECT_TRUE((IsAtLeastIterator<std::bidirectional_iterator_tag,
                                 std::vector<int>::iterator>()));
  EXPECT_TRUE((IsAtLeastIterator<std::random_access_iterator_tag,
                                 std::vector<int>::iterator>()));

  EXPECT_TRUE(
      (IsAtLeastIterator<std::input_iterator_tag, std::list<int>::iterator>()));
  EXPECT_TRUE((IsAtLeastIterator<std::forward_iterator_tag,
                                 std::list<int>::iterator>()));
  EXPECT_TRUE((IsAtLeastIterator<std::bidirectional_iterator_tag,
                                 std::list<int>::iterator>()));
  EXPECT_FALSE((IsAtLeastIterator<std::random_access_iterator_tag,
                                  std::list<int>::iterator>()));

  EXPECT_TRUE((IsAtLeastIterator<std::input_iterator_tag,
                                 std::forward_list<int>::iterator>()));
  EXPECT_TRUE((IsAtLeastIterator<std::forward_iterator_tag,
                                 std::forward_list<int>::iterator>()));
  EXPECT_FALSE((IsAtLeastIterator<std::bidirectional_iterator_tag,
                                  std::forward_list<int>::iterator>()));
  EXPECT_FALSE((IsAtLeastIterator<std::random_access_iterator_tag,
                                  std::forward_list<int>::iterator>()));

  EXPECT_TRUE((IsAtLeastIterator<std::input_iterator_tag,
                                 std::istream_iterator<int>>()));
  EXPECT_FALSE((IsAtLeastIterator<std::forward_iterator_tag,
                                  std::istream_iterator<int>>()));
  EXPECT_FALSE((IsAtLeastIterator<std::bidirectional_iterator_tag,
                                  std::istream_iterator<int>>()));
  EXPECT_FALSE((IsAtLeastIterator<std::random_access_iterator_tag,
                                  std::istream_iterator<int>>()));

  EXPECT_TRUE((IsAtLeastIterator<std::input_iterator_tag,
                                 Cpp20ForwardZipIterator<int*>>()));
  EXPECT_TRUE((IsAtLeastIterator<std::forward_iterator_tag,
                                 Cpp20ForwardZipIterator<int*>>()));
  EXPECT_FALSE((IsAtLeastIterator<std::bidirectional_iterator_tag,
                                  Cpp20ForwardZipIterator<int*>>()));
  EXPECT_FALSE((IsAtLeastIterator<std::random_access_iterator_tag,
                                  Cpp20ForwardZipIterator<int*>>()));
}

}  // namespace
}  // namespace base_internal
ABSL_NAMESPACE_END
}  // namespace absl
