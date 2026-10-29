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

#ifndef ABSL_BASE_INTERNAL_ITERATOR_TRAITS_TEST_HELPER_H_
#define ABSL_BASE_INTERNAL_ITERATOR_TRAITS_TEST_HELPER_H_

#include <iterator>
#include <utility>

#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace base_internal {

// This would be a forward_iterator in C++20, but it's only an input iterator
// before that, since it has a non-reference `reference`.
template <typename Iterator>
class Cpp20ForwardZipIterator {
  using IteratorReference = typename std::iterator_traits<Iterator>::reference;

 public:
  Cpp20ForwardZipIterator() = default;
  explicit Cpp20ForwardZipIterator(Iterator left, Iterator right)
      : left_(left), right_(right) {}

  Cpp20ForwardZipIterator& operator++() {
    ++left_;
    ++right_;
    return *this;
  }

  Cpp20ForwardZipIterator operator++(int) {
    Cpp20ForwardZipIterator tmp(*this);
    ++*this;
    return *this;
  }

  std::pair<IteratorReference, IteratorReference> operator*() const {
    return {*left_, *right_};
  }

  // C++17 input iterators require `operator->`, but this isn't  possible to
  // implement. C++20 dropped the requirement.

  friend bool operator==(const Cpp20ForwardZipIterator& lhs,
                         const Cpp20ForwardZipIterator& rhs) {
    return lhs.left_ == rhs.left_ && lhs.right_ == rhs.right_;
  }

  friend bool operator!=(const Cpp20ForwardZipIterator& lhs,
                         const Cpp20ForwardZipIterator& rhs) {
    return !(lhs == rhs);
  }

 private:
  Iterator left_{};
  Iterator right_{};
};

}  // namespace base_internal
ABSL_NAMESPACE_END
}  // namespace absl

template <typename Iterator>
struct std::iterator_traits<
    absl::base_internal::Cpp20ForwardZipIterator<Iterator>> {
 private:
  using IteratorReference = typename std::iterator_traits<Iterator>::reference;

 public:
  using iterator_category = std::input_iterator_tag;
  using iterator_concept = std::forward_iterator_tag;
  using value_type = std::pair<IteratorReference, IteratorReference>;
  using difference_type =
      typename std::iterator_traits<Iterator>::difference_type;
  using reference = value_type;
  using pointer = void;
};

#if defined(__cpp_lib_concepts)
static_assert(
    std::forward_iterator<absl::base_internal::Cpp20ForwardZipIterator<int*>>);
#endif  // defined(__cpp_lib_concepts)

#endif  // ABSL_BASE_INTERNAL_ITERATOR_TRAITS_TEST_HELPER_H_
