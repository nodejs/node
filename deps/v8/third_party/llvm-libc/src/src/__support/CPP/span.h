//===-- Standalone implementation std::span ---------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_SPAN_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_SPAN_H

#include <stddef.h> // For size_t

#include "array.h" // For array
#include "limits.h"
#include "src/__support/macros/config.h"
#include "type_traits.h" // For remove_cv_t, enable_if_t, is_same_v, is_const_v

#include "src/__support/macros/attributes.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// A trimmed down implementation of std::span.
// Missing features:
// - No constant size spans (e.g. Span<int, 4>),
// - Only handle pointer like types, no fancy interators nor object overriding
//   the & operator,
// - No implicit type conversion (e.g. Span<B>, initialized with As where A
//   inherits from B),
// - No reverse iterators
template <typename T> class span {
  template <typename U>
  LIBC_INLINE_VAR static constexpr bool is_const_view_v =
      !cpp::is_const_v<U> && cpp::is_const_v<T> &&
      cpp::is_same_v<U, remove_cv_t<T>>;

  template <typename U>
  LIBC_INLINE_VAR static constexpr bool is_compatible_v =
      cpp::is_same_v<U, T> || is_const_view_v<U>;

public:
  using element_type = T;
  using value_type = remove_cv_t<T>;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using pointer = T *;
  using const_pointer = const T *;
  using reference = T &;
  using const_reference = const T &;
  using iterator = T *;

  LIBC_INLINE_VAR static constexpr size_type dynamic_extent =
      cpp::numeric_limits<size_type>::max();

  LIBC_INLINE constexpr span() : span_data(nullptr), span_size(0) {}

  LIBC_INLINE constexpr span(const span &) = default;

  LIBC_INLINE constexpr span(pointer first, size_type count)
      : span_data(first), span_size(count) {}

  LIBC_INLINE constexpr span(pointer first, pointer end)
      : span_data(first), span_size(static_cast<size_t>(end - first)) {}

  template <typename U, size_t N,
            cpp::enable_if_t<is_compatible_v<U>, bool> = true>
  LIBC_INLINE constexpr span(U (&arr)[N]) : span_data(arr), span_size(N) {}

  template <typename U, size_t N,
            cpp::enable_if_t<is_compatible_v<U>, bool> = true>
  LIBC_INLINE constexpr span(array<U, N> &arr)
      : span_data(arr.data()), span_size(arr.size()) {}

  template <typename U, cpp::enable_if_t<is_compatible_v<U>, bool> = true>
  LIBC_INLINE constexpr span(span<U> &s)
      : span_data(s.data()), span_size(s.size()) {}

  template <typename U, cpp::enable_if_t<is_compatible_v<U>, bool> = true>
  LIBC_INLINE constexpr span &operator=(span<U> &s) {
    span_data = s.data();
    span_size = s.size();
    return *this;
  }

  LIBC_INLINE ~span() = default;

  LIBC_INLINE constexpr reference operator[](size_type index) const {
    return data()[index];
  }

  LIBC_INLINE constexpr iterator begin() const { return data(); }
  LIBC_INLINE constexpr iterator end() const { return data() + size(); }
  LIBC_INLINE constexpr reference front() const { return (*this)[0]; }
  LIBC_INLINE constexpr reference back() const { return (*this)[size() - 1]; }
  LIBC_INLINE constexpr pointer data() const { return span_data; }
  LIBC_INLINE constexpr size_type size() const { return span_size; }
  LIBC_INLINE constexpr size_type size_bytes() const {
    return sizeof(T) * size();
  }
  LIBC_INLINE constexpr bool empty() const { return size() == 0; }

  LIBC_INLINE constexpr span<element_type>
  subspan(size_type offset, size_type count = dynamic_extent) const {
    return span<element_type>(data() + offset, count_to_size(offset, count));
  }

  LIBC_INLINE constexpr span<element_type> first(size_type count) const {
    return subspan(0, count);
  }

  LIBC_INLINE constexpr span<element_type> last(size_type count) const {
    return span<element_type>(data() + (size() - count), count);
  }

private:
  LIBC_INLINE constexpr size_type count_to_size(size_type offset,
                                                size_type count) const {
    if (count == dynamic_extent) {
      return size() - offset;
    }
    return count;
  }

  T *span_data;
  size_t span_size;
};

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_SPAN_H
