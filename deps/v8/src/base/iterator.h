// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_ITERATOR_H_
#define V8_BASE_ITERATOR_H_

#include <iterator>

namespace v8 {
namespace base {

template <class Category, class Type, class Diff = std::ptrdiff_t,
          class Pointer = Type*, class Reference = Type&>
struct iterator {
  using iterator_category = Category;
  using value_type = Type;
  using difference_type = Diff;
  using pointer = Pointer;
  using reference = Reference;
};

// The intention of the base::iterator_range class is to encapsulate two
// iterators so that the range defined by the iterators can be used like
// a regular STL container (actually only a subset of the full container
// functionality is available usually).
template <typename ForwardIterator>
class iterator_range {
 public:
  using iterator = ForwardIterator;
  using const_iterator = ForwardIterator;
  using pointer = typename std::iterator_traits<iterator>::pointer;
  using reference = typename std::iterator_traits<iterator>::reference;
  using value_type = typename std::iterator_traits<iterator>::value_type;
  using difference_type =
      typename std::iterator_traits<iterator>::difference_type;

  iterator_range() : begin_(), end_() {}
  iterator_range(ForwardIterator begin, ForwardIterator end)
      : begin_(begin), end_(end) {}

  iterator begin() const { return begin_; }
  iterator end() const { return end_; }
  const_iterator cbegin() const { return begin_; }
  const_iterator cend() const { return end_; }
  auto rbegin() const { return std::make_reverse_iterator(end_); }
  auto rend() const { return std::make_reverse_iterator(begin_); }

  bool empty() const { return cbegin() == cend(); }

  // Random Access iterators only.
  reference operator[](difference_type n) { return begin()[n]; }
  difference_type size() const { return cend() - cbegin(); }

 private:
  const_iterator const begin_;
  const_iterator const end_;
};

template <typename ForwardIterator>
auto make_iterator_range(ForwardIterator begin, ForwardIterator end) {
  return iterator_range<ForwardIterator>{begin, end};
}

template <class T>
struct DerefPtrIterator : base::iterator<std::bidirectional_iterator_tag, T> {
  T* const* ptr;

  explicit DerefPtrIterator(T* const* ptr) : ptr(ptr) {}

  T& operator*() { return **ptr; }
  DerefPtrIterator& operator++() {
    ++ptr;
    return *this;
  }
  DerefPtrIterator& operator--() {
    --ptr;
    return *this;
  }
  bool operator!=(DerefPtrIterator other) { return ptr != other.ptr; }
};

// {Reversed} returns a container adapter usable in a range-based "for"
// statement for iterating a reversible container in reverse order.
//
// Example:
//
//   std::vector<int> v = ...;
//   for (int i : base::Reversed(v)) {
//     // iterates through v from back to front
//   }
//
// The signature avoids binding to temporaries (T&& / const T&) on purpose. The
// lifetime of a temporary would not extend to a range-based for loop using it.
template <typename T>
auto Reversed(T& t) {  // NOLINT(runtime/references): match {rbegin} and {rend}
  return make_iterator_range(std::rbegin(t), std::rend(t));
}

// This overload of `Reversed` is safe even when the argument is a temporary,
// because we rely on the wrapped iterators instead of the `iterator_range`
// object itself.
template <typename T>
auto Reversed(const iterator_range<T>& t) {
  return make_iterator_range(std::rbegin(t), std::rend(t));
}

}  // namespace base
}  // namespace v8

#endif  // V8_BASE_ITERATOR_H_
