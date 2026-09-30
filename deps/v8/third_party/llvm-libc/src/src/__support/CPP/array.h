//===-- A self contained equivalent of std::array ---------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_ARRAY_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_ARRAY_H

#include "src/__support/CPP/iterator.h" // reverse_iterator
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include <stddef.h> // For size_t.

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

template <class T, size_t N> struct array {
  static_assert(N != 0,
                "Cannot create a LIBC_NAMESPACE::cpp::array of size 0.");

  T Data[N];
  using value_type = T;
  using iterator = T *;
  using const_iterator = const T *;
  using reverse_iterator = cpp::reverse_iterator<iterator>;
  using const_reverse_iterator = cpp::reverse_iterator<const_iterator>;

  LIBC_INLINE constexpr T *data() { return Data; }
  LIBC_INLINE constexpr const T *data() const { return Data; }

  LIBC_INLINE constexpr T &front() { return Data[0]; }
  LIBC_INLINE constexpr const T &front() const { return Data[0]; }

  LIBC_INLINE constexpr T &back() { return Data[N - 1]; }
  LIBC_INLINE constexpr const T &back() const { return Data[N - 1]; }

  LIBC_INLINE constexpr T &operator[](size_t Index) { return Data[Index]; }

  LIBC_INLINE constexpr const T &operator[](size_t Index) const {
    return Data[Index];
  }

  LIBC_INLINE constexpr size_t size() const { return N; }

  LIBC_INLINE constexpr bool empty() const { return N == 0; }

  LIBC_INLINE constexpr iterator begin() { return Data; }
  LIBC_INLINE constexpr const_iterator begin() const { return Data; }
  LIBC_INLINE constexpr const_iterator cbegin() const { return begin(); }

  LIBC_INLINE constexpr iterator end() { return Data + N; }
  LIBC_INLINE constexpr const_iterator end() const { return Data + N; }
  LIBC_INLINE constexpr const_iterator cend() const { return end(); }

  LIBC_INLINE constexpr reverse_iterator rbegin() {
    return reverse_iterator{end()};
  }
  LIBC_INLINE constexpr const_reverse_iterator rbegin() const {
    return const_reverse_iterator{end()};
  }
  LIBC_INLINE constexpr const_reverse_iterator crbegin() const {
    return rbegin();
  }

  LIBC_INLINE constexpr reverse_iterator rend() {
    return reverse_iterator{begin()};
  }
  LIBC_INLINE constexpr const_reverse_iterator rend() const {
    return const_reverse_iterator{begin()};
  }
  LIBC_INLINE constexpr const_reverse_iterator crend() const { return rend(); }
};

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_ARRAY_H
