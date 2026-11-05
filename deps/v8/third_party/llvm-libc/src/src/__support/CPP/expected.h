//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Implementation of C++23-style expected and unexpected utilities.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_EXPECTED_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_EXPECTED_H

#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h"
#include "src/__support/macros/optimization.h"

namespace LIBC_NAMESPACE_DECL {
namespace cpp {

// This is used to hold an unexpected value so that a different constructor is
// selected.
template <class T> class unexpected {
  T value;

public:
  LIBC_INLINE constexpr explicit unexpected(T value) : value(value) {}
  LIBC_INLINE constexpr T error() { return value; }
};

template <class T> explicit unexpected(T) -> unexpected<T>;

template <class T, class E> class expected {
  union {
    T exp;
    E unexp;
  };
  bool is_expected;

public:
  LIBC_INLINE constexpr expected() : exp(), is_expected(true) {}
  LIBC_INLINE constexpr expected(T exp) : exp(exp), is_expected(true) {}
  LIBC_INLINE constexpr expected(unexpected<E> unexp)
      : unexp(unexp.error()), is_expected(false) {}

  LIBC_INLINE constexpr bool has_value() const { return is_expected; }

  LIBC_INLINE constexpr T &value() {
    if (LIBC_UNLIKELY(!is_expected))
      __builtin_trap();
    return exp;
  }
  LIBC_INLINE constexpr E &error() {
    if (LIBC_UNLIKELY(is_expected))
      __builtin_trap();
    return unexp;
  }
  LIBC_INLINE constexpr const T &value() const {
    if (LIBC_UNLIKELY(!is_expected))
      __builtin_trap();
    return exp;
  }
  LIBC_INLINE constexpr const E &error() const {
    if (LIBC_UNLIKELY(is_expected))
      __builtin_trap();
    return unexp;
  }

  LIBC_INLINE constexpr explicit operator bool() const { return is_expected; }

  LIBC_INLINE constexpr T &operator*() {
#ifndef NDEBUG
    if (LIBC_UNLIKELY(!is_expected))
      __builtin_trap();
#endif
    return exp;
  }
  LIBC_INLINE constexpr const T &operator*() const {
#ifndef NDEBUG
    if (LIBC_UNLIKELY(!is_expected))
      __builtin_trap();
#endif
    return exp;
  }
  LIBC_INLINE constexpr T *operator->() {
#ifndef NDEBUG
    if (LIBC_UNLIKELY(!is_expected))
      __builtin_trap();
#endif
    return &exp;
  }
  LIBC_INLINE constexpr const T *operator->() const {
#ifndef NDEBUG
    if (LIBC_UNLIKELY(!is_expected))
      __builtin_trap();
#endif
    return &exp;
  }
};

template <class E> class expected<void, E> {
  union {
    char dummy;
    E unexp;
  };
  bool is_expected;

public:
  LIBC_INLINE constexpr expected() : dummy(), is_expected(true) {}
  LIBC_INLINE constexpr expected(unexpected<E> unexp)
      : unexp(unexp.error()), is_expected(false) {}

  LIBC_INLINE constexpr bool has_value() const { return is_expected; }

  LIBC_INLINE constexpr void value() const {
    if (LIBC_UNLIKELY(!is_expected))
      __builtin_trap();
  }
  LIBC_INLINE constexpr E &error() {
    if (LIBC_UNLIKELY(is_expected))
      __builtin_trap();
    return unexp;
  }
  LIBC_INLINE constexpr const E &error() const {
    if (LIBC_UNLIKELY(is_expected))
      __builtin_trap();
    return unexp;
  }

  LIBC_INLINE constexpr explicit operator bool() const { return is_expected; }

  LIBC_INLINE constexpr void operator*() const {
#ifndef NDEBUG
    if (LIBC_UNLIKELY(!is_expected))
      __builtin_trap();
#endif
  }
};

} // namespace cpp
} // namespace LIBC_NAMESPACE_DECL

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_EXPECTED_H
