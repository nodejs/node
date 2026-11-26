//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// Implementation of cpp::span.
///
//===----------------------------------------------------------------------===//
#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_SPAN_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_SPAN_H

#include <stddef.h> // For ptrdiff_t

#include "hdr/types/size_t.h"
#include "src/__support/CPP/array.h"
#include "src/__support/CPP/cstddef.h"
#include "src/__support/CPP/iterator.h"
#include "src/__support/CPP/limits.h"
#include "src/__support/CPP/type_traits.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// A trimmed down implementation of std::span.
// Missing features:
// - No constant size spans (e.g. Span<int, 4>),
// - Only handle pointer like types, no fancy iterators nor object overriding
//   the & operator,
// - No implicit type conversion (e.g. Span<B>, initialized with As where A
//   inherits from B).
template <typename T> class span {
  template <typename U>
  LIBC_INLINE_VAR static constexpr bool is_const_view_v =
      !cpp::is_const_v<U> && cpp::is_const_v<T> &&
      cpp::is_same_v<U, remove_cv_t<T>>;

  template <typename U>
  LIBC_INLINE_VAR static constexpr bool is_compatible_v =
      cpp::is_same_v<U, T> || is_const_view_v<U>;

  LIBC_INLINE static constexpr void assert_precondition(bool cond) {
#ifndef NDEBUG
    if (LIBC_UNLIKELY(!cond))
      __builtin_trap();
#else
    (void)cond;
#endif
  }

  template <typename End>
  LIBC_INLINE static constexpr size_t distance_to_end(T *first, End end) {
    if constexpr (cpp::is_null_pointer_v<End>) {
      assert_precondition(first == nullptr);
      return 0;
    } else {
      T *end_ptr = end;
      assert_precondition(first <= end_ptr);
      return static_cast<size_t>(end_ptr - first);
    }
  }

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
  using const_iterator = const T *;
  using reverse_iterator = cpp::reverse_iterator<iterator>;
  using const_reverse_iterator = cpp::reverse_iterator<const_iterator>;

  LIBC_INLINE_VAR static constexpr size_type dynamic_extent =
      cpp::numeric_limits<size_type>::max();

  LIBC_INLINE constexpr span() : span_data(nullptr), span_size(0) {}

  LIBC_INLINE constexpr span(const span &) = default;
  LIBC_INLINE constexpr span &operator=(const span &) = default;

  LIBC_INLINE constexpr span(pointer first, size_type count)
      : span_data(first), span_size(count) {}

  template <typename End,
            cpp::enable_if_t<cpp::is_same_v<End, pointer> ||
                                 cpp::is_same_v<End, value_type *> ||
                                 cpp::is_null_pointer_v<End>,
                             bool> = true>
  LIBC_INLINE constexpr span(pointer first, End end)
      : span_data(first), span_size(distance_to_end(first, end)) {}

  template <typename U, size_t N,
            cpp::enable_if_t<is_compatible_v<U>, bool> = true>
  LIBC_INLINE constexpr span(U (&arr)[N]) : span_data(arr), span_size(N) {}

  template <typename U, size_t N,
            cpp::enable_if_t<is_compatible_v<U>, bool> = true>
  LIBC_INLINE constexpr span(array<U, N> &arr)
      : span_data(arr.data()), span_size(arr.size()) {}

  template <typename U, size_t N,
            cpp::enable_if_t<is_compatible_v<const U>, bool> = true>
  LIBC_INLINE constexpr span(const array<U, N> &arr)
      : span_data(arr.data()), span_size(arr.size()) {}

  template <typename U, cpp::enable_if_t<is_compatible_v<U>, bool> = true>
  LIBC_INLINE constexpr span(const span<U> &s)
      : span_data(s.data()), span_size(s.size()) {}

  template <typename U, cpp::enable_if_t<is_compatible_v<U>, bool> = true>
  LIBC_INLINE constexpr span &operator=(const span<U> &s) {
    span_data = s.data();
    span_size = s.size();
    return *this;
  }

  LIBC_INLINE ~span() = default;

  LIBC_INLINE constexpr reference operator[](size_type index) const {
    assert_precondition(index < size());
    return data()[index];
  }

  LIBC_INLINE constexpr iterator begin() const { return data(); }
  LIBC_INLINE constexpr iterator end() const { return data() + size(); }
  LIBC_INLINE constexpr const_iterator cbegin() const { return data(); }
  LIBC_INLINE constexpr const_iterator cend() const { return data() + size(); }

  LIBC_INLINE constexpr reverse_iterator rbegin() const {
    return reverse_iterator(end());
  }
  LIBC_INLINE constexpr reverse_iterator rend() const {
    return reverse_iterator(begin());
  }
  LIBC_INLINE constexpr const_reverse_iterator crbegin() const {
    return const_reverse_iterator(cend());
  }
  LIBC_INLINE constexpr const_reverse_iterator crend() const {
    return const_reverse_iterator(cbegin());
  }

  LIBC_INLINE constexpr reference front() const {
    assert_precondition(!empty());
    return data()[0];
  }

  LIBC_INLINE constexpr reference back() const {
    assert_precondition(!empty());
    return data()[size() - 1];
  }

  LIBC_INLINE constexpr pointer data() const { return span_data; }
  LIBC_INLINE constexpr size_type size() const { return span_size; }
  LIBC_INLINE constexpr size_type size_bytes() const {
    return sizeof(T) * size();
  }
  LIBC_INLINE constexpr bool empty() const { return size() == 0; }

  LIBC_INLINE constexpr span<element_type>
  subspan(size_type offset, size_type count = dynamic_extent) const {
    assert_precondition(offset <= size() &&
                        (count == dynamic_extent || count <= size() - offset));
    return span<element_type>(data() + offset, count_to_size(offset, count));
  }

  template <size_type Offset, size_type Count = dynamic_extent>
  LIBC_INLINE constexpr span<element_type> subspan() const {
    return subspan(Offset, Count);
  }

  LIBC_INLINE constexpr span<element_type> first(size_type count) const {
    assert_precondition(count <= size());
    return span<element_type>(data(), count);
  }

  template <size_type Count>
  LIBC_INLINE constexpr span<element_type> first() const {
    return first(Count);
  }

  LIBC_INLINE constexpr span<element_type> last(size_type count) const {
    assert_precondition(count <= size());
    return span<element_type>(data() + (size() - count), count);
  }

  template <size_type Count>
  LIBC_INLINE constexpr span<element_type> last() const {
    return last(Count);
  }

private:
  LIBC_INLINE constexpr size_type count_to_size(size_type offset,
                                                size_type count) const {
    if (count == dynamic_extent)
      return size() - offset;
    return count;
  }

  T *span_data;
  size_t span_size;
};

template <typename T, size_t N> span(T (&)[N]) -> span<T>;

template <typename T, size_t N> span(array<T, N> &) -> span<T>;

template <typename T, size_t N> span(const array<T, N> &) -> span<const T>;

template <typename T> span(T *, size_t) -> span<T>;

template <typename T> span(T *, T *) -> span<T>;

template <typename T> LIBC_INLINE span<const byte> as_bytes(span<T> s) {
  return span<const byte>(reinterpret_cast<const byte *>(s.data()),
                          s.size_bytes());
}

template <typename T, cpp::enable_if_t<!cpp::is_const_v<T>, bool> = true>
LIBC_INLINE span<byte> as_writable_bytes(span<T> s) {
  return span<byte>(reinterpret_cast<byte *>(s.data()), s.size_bytes());
}

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_SPAN_H
