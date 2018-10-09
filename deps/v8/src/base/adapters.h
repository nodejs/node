// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Slightly adapted for inclusion in V8.
// Copyright 2014 the V8 project authors. All rights reserved.

#ifndef V8_BASE_ADAPTERS_H_
#define V8_BASE_ADAPTERS_H_

#include <iterator>

#include "src/base/macros.h"

namespace v8 {
namespace base {

// Internal adapter class for implementing base::Reversed.
template <typename T>
class ReversedAdapter {
 public:
  using Iterator =
      std::reverse_iterator<decltype(std::begin(std::declval<T>()))>;

  explicit ReversedAdapter(T& t) : t_(t) {}
  ReversedAdapter(const ReversedAdapter& ra) = default;

  // TODO(clemensh): Use std::rbegin/std::rend once we have C++14 support.
  Iterator begin() const { return Iterator(std::end(t_)); }
  Iterator end() const { return Iterator(std::begin(t_)); }

 private:
  T& t_;

  DISALLOW_ASSIGN(ReversedAdapter);
};

// Reversed returns a container adapter usable in a range-based "for" statement
// for iterating a reversible container in reverse order.
//
// Example:
//
//   std::vector<int> v = ...;
//   for (int i : base::Reversed(v)) {
//     // iterates through v from back to front
//   }
template <typename T>
ReversedAdapter<T> Reversed(T& t) {
  return ReversedAdapter<T>(t);
}

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_ADAPTERS_H_
