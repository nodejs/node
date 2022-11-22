// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_CONTAINER_UTILS_H_
#define V8_BASE_CONTAINER_UTILS_H_

#include <algorithm>
#include <optional>
#include <vector>

namespace v8::base {

// Returns true iff the {element} is found in the {container}.
template <typename C, typename T>
bool contains(const C& container, const T& element) {
  const auto e = end(container);
  return std::find(begin(container), e, element) != e;
}

// Returns the first index of {element} in {container}. Returns std::nullopt if
// {container} does not contain {element}.
template <typename C, typename T>
std::optional<size_t> index_of(const C& container, const T& element) {
  const auto b = begin(container);
  const auto e = end(container);
  if (auto it = std::find(b, e, element); it != e) {
    return {std::distance(b, it)};
  }
  return std::nullopt;
}

// Returns the index of the first element in {container} that satisfies
// {predicate}. Returns std::nullopt if no element satisfies {predicate}.
template <typename C, typename P>
std::optional<size_t> index_of_if(const C& container, const P& predicate) {
  const auto b = begin(container);
  const auto e = end(container);
  if (auto it = std::find_if(b, e, predicate); it != e) {
    return {std::distance(b, it)};
  }
  return std::nullopt;
}

// Removes {count} elements from {container} starting at {index}. If {count} is
// larger than the number of elements after {index}, all elements after {index}
// are removed. Returns the number of removed elements.
template <typename C>
inline size_t erase_at(C& container, size_t index, size_t count = 1) {
  // TODO(C++20): Replace with std::erase.
  if (size(container) <= index) return 0;
  auto start = begin(container) + index;
  count = std::min<size_t>(count, std::distance(start, end(container)));
  container.erase(start, start + count);
  return count;
}

// Removes all elements from {container} that satisfy {predicate}. Returns the
// number of removed elements.
// TODO(C++20): Replace with std::erase_if.
template <typename C, typename P>
inline size_t erase_if(C& container, const P& predicate) {
  size_t count = 0;
  auto e = end(container);
  for (auto it = begin(container); it != e;) {
    it = std::find_if(it, e, predicate);
    if (it == e) break;
    it = container.erase(it);
    e = end(container);
    ++count;
  }
  return count;
}

// Helper for std::count_if.
template <typename C, typename P>
inline size_t count_if(const C& container, const P& predicate) {
  return std::count_if(begin(container), end(container), predicate);
}

// Helper for std::all_of.
template <typename C, typename P>
inline bool all_of(const C& container, const P& predicate) {
  return std::all_of(begin(container), end(container), predicate);
}

// Helper for std::none_of.
template <typename C, typename P>
inline bool none_of(const C& container, const P& predicate) {
  return std::none_of(begin(container), end(container), predicate);
}

// Returns true iff all elements of {container} compare equal using operator==.
template <typename C>
inline bool all_equal(const C& container) {
  if (size(container) <= 1) return true;
  auto b = begin(container);
  const auto& value = *b;
  return std::all_of(++b, end(container),
                     [&](const auto& v) { return v == value; });
}

// Returns true iff all elements of {container} compare equal to {value} using
// operator==.
template <typename C, typename T>
inline bool all_equal(const C& container, const T& value) {
  return std::all_of(begin(container), end(container),
                     [&](const auto& v) { return v == value; });
}

// Appends to vector {v} all the elements in the range {begin(container)} and
// {end(container)}.
template <typename T, typename A, typename C>
inline void vector_append(std::vector<T, A>& v, const C& container) {
  v.insert(end(v), begin(container), end(container));
}

}  // namespace v8::base

#endif  // V8_BASE_CONTAINER_UTILS_H_
