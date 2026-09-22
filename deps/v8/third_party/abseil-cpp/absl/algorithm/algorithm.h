// Copyright 2017 The Abseil Authors.
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
//
// -----------------------------------------------------------------------------
// File: algorithm.h
// -----------------------------------------------------------------------------
//
// This header file contains Google extensions to the standard <algorithm> C++
// header.

#ifndef ABSL_ALGORITHM_ALGORITHM_H_
#define ABSL_ALGORITHM_ALGORITHM_H_

#include <algorithm>
#include <iterator>
#include <type_traits>

#include "absl/base/config.h"
#include "absl/base/macros.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

// equal()
// rotate()
//
// Historical note: Abseil once provided implementations of these algorithms
// prior to their adoption in C++14. New code should prefer to use the std
// variants.
//
// See the documentation for the STL <algorithm> header for more information:
// https://en.cppreference.com/w/cpp/header/algorithm

template <class InputIt1, class InputIt2>
ABSL_DEPRECATE_AND_INLINE()
constexpr bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2) {
  return std::equal(first1, last1, first2);
}

template <class InputIt1, class InputIt2, class BinaryPredicate>
ABSL_DEPRECATE_AND_INLINE()
constexpr bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2,
                     BinaryPredicate p) {
  return std::equal(first1, last1, first2, p);
}

template <class InputIt1, class InputIt2>
ABSL_DEPRECATE_AND_INLINE()
constexpr bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2,
                     InputIt2 last2) {
  return std::equal(first1, last1, first2, last2);
}

template <class InputIt1, class InputIt2, class BinaryPredicate>
ABSL_DEPRECATE_AND_INLINE()
constexpr bool equal(InputIt1 first1, InputIt1 last1, InputIt2 first2,
                     InputIt2 last2, BinaryPredicate p) {
  return std::equal(first1, last1, first2, last2, p);
}

template <class ForwardIt>
ABSL_DEPRECATE_AND_INLINE()
constexpr ForwardIt rotate(ForwardIt first, ForwardIt n_first, ForwardIt last) {
  return std::rotate(first, n_first, last);
}

// linear_search()
//
// Performs a linear search for `value` using the iterator `first` up to
// but not including `last`, returning true if [`first`, `last`) contains an
// element equal to `value`.
//
// A linear search is of O(n) complexity which is guaranteed to make at most
// n = (`last` - `first`) comparisons. A linear search over short containers
// may be faster than a binary search, even when the container is sorted.
template <typename InputIterator, typename EqualityComparable>
constexpr bool linear_search(InputIterator first, InputIterator last,
                             const EqualityComparable& value) {
  return std::find(first, last, value) != last;
}

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_ALGORITHM_ALGORITHM_H_
