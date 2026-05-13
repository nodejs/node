//===-- Standalone implementation of iterator -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_ITERATOR_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_ITERATOR_H

#include "src/__support/CPP/type_traits/enable_if.h"
#include "src/__support/CPP/type_traits/is_convertible.h"
#include "src/__support/CPP/type_traits/is_same.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

template <typename T> struct iterator_traits;
template <typename T> struct iterator_traits<T *> {
  using reference = T &;
  using value_type = T;
};

template <typename Iter> class reverse_iterator {
  Iter current;

public:
  using reference = typename iterator_traits<Iter>::reference;
  using value_type = typename iterator_traits<Iter>::value_type;
  using iterator_type = Iter;

  LIBC_INLINE reverse_iterator() : current() {}
  LIBC_INLINE constexpr explicit reverse_iterator(Iter it) : current(it) {}

  template <typename Other,
            cpp::enable_if_t<!cpp::is_same_v<Iter, Other> &&
                                 cpp::is_convertible_v<const Other &, Iter>,
                             int> = 0>
  LIBC_INLINE constexpr explicit reverse_iterator(const Other &it)
      : current(it) {}

  LIBC_INLINE friend constexpr bool operator==(const reverse_iterator &lhs,
                                               const reverse_iterator &rhs) {
    return lhs.base() == rhs.base();
  }

  LIBC_INLINE friend constexpr bool operator!=(const reverse_iterator &lhs,
                                               const reverse_iterator &rhs) {
    return lhs.base() != rhs.base();
  }

  LIBC_INLINE friend constexpr bool operator<(const reverse_iterator &lhs,
                                              const reverse_iterator &rhs) {
    return lhs.base() > rhs.base();
  }

  LIBC_INLINE friend constexpr bool operator<=(const reverse_iterator &lhs,
                                               const reverse_iterator &rhs) {
    return lhs.base() >= rhs.base();
  }

  LIBC_INLINE friend constexpr bool operator>(const reverse_iterator &lhs,
                                              const reverse_iterator &rhs) {
    return lhs.base() < rhs.base();
  }

  LIBC_INLINE friend constexpr bool operator>=(const reverse_iterator &lhs,
                                               const reverse_iterator &rhs) {
    return lhs.base() <= rhs.base();
  }

  LIBC_INLINE constexpr iterator_type base() const { return current; }

  LIBC_INLINE constexpr reference operator*() const {
    Iter tmp = current;
    return *--tmp;
  }
  LIBC_INLINE constexpr reverse_iterator operator--() {
    ++current;
    return *this;
  }
  LIBC_INLINE constexpr reverse_iterator &operator++() {
    --current;
    return *this;
  }
  LIBC_INLINE constexpr reverse_iterator operator++(int) {
    reverse_iterator tmp(*this);
    --current;
    return tmp;
  }
};

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_ITERATOR_H
