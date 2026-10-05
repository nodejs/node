// Copyright 2018 The Abseil Authors.
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
// compare.h
// -----------------------------------------------------------------------------
//
// This header file defines the `absl::partial_ordering`, `absl::weak_ordering`,
// and `absl::strong_ordering` types for storing the results of three way
// comparisons.
//
// Example:
//   absl::weak_ordering compare(const std::string& a, const std::string& b);
//
// These are C++17 compatible versions of the C++20 corresponding types
// (`std::partial_ordering`, etc.) and are designed to be drop-in replacements
// for code compliant with C++20.

#ifndef ABSL_TYPES_COMPARE_H_
#define ABSL_TYPES_COMPARE_H_

#include "absl/base/config.h"

#ifdef ABSL_USES_STD_ORDERING

#include <compare>  // IWYU pragma: export
#include <type_traits>

#include "absl/meta/type_traits.h"

#else

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/meta/type_traits.h"

#endif

namespace absl {
ABSL_NAMESPACE_BEGIN

#ifdef ABSL_USES_STD_ORDERING

using std::partial_ordering;
using std::strong_ordering;
using std::weak_ordering;

#else

namespace compare_internal {

using value_type = int8_t;

class OnlyLiteralZero {
 public:
#if ABSL_HAVE_ATTRIBUTE(enable_if)
  // On clang, we can avoid triggering modernize-use-nullptr by only enabling
  // this overload when the value is a compile time integer constant equal to 0.
  //
  // In c++20, this could be a static_assert in a consteval function.
  constexpr OnlyLiteralZero(int n)  // NOLINT
      __attribute__((enable_if(n == 0, "Only literal `0` is allowed."))) {}
#else  // ABSL_HAVE_ATTRIBUTE(enable_if)
  // Accept only literal zero since it can be implicitly converted to a pointer
  // to member type. nullptr constants will be caught by the other constructor
  // which accepts a nullptr_t.
  //
  // This constructor is not used for clang since it triggers
  // modernize-use-nullptr.
  constexpr OnlyLiteralZero(int OnlyLiteralZero::*) noexcept {}  // NOLINT
#endif

  // Fails compilation when `nullptr` or integral type arguments other than
  // `int` are passed. This constructor doesn't accept `int` because literal `0`
  // has type `int`. Literal `0` arguments will be implicitly converted to
  // `std::nullptr_t` and accepted by the above constructor, while other `int`
  // arguments will fail to be converted and cause compilation failure.
  template <typename T, typename = std::enable_if_t<
                            std::is_same_v<T, std::nullptr_t> ||
                            (std::is_integral_v<T> && !std::is_same_v<T, int>)>>
  OnlyLiteralZero(T) {  // NOLINT
    static_assert(sizeof(T) < 0, "Only literal `0` is allowed.");
  }
};

enum class eq : value_type {
  equal = 0,
  equivalent = equal,
  nonequal = 1,
  nonequivalent = nonequal,
};

enum class ord : value_type { less = -1, greater = 1 };

enum class ncmp : value_type { unordered = -127 };

}  // namespace compare_internal

class partial_ordering {
  explicit constexpr partial_ordering(compare_internal::eq v) noexcept
      : value_(static_cast<compare_internal::value_type>(v)) {}
  explicit constexpr partial_ordering(compare_internal::ord v) noexcept
      : value_(static_cast<compare_internal::value_type>(v)) {}
  explicit constexpr partial_ordering(compare_internal::ncmp v) noexcept
      : value_(static_cast<compare_internal::value_type>(v)) {}

  constexpr bool is_ordered() const noexcept {
    return value_ !=
           compare_internal::value_type(compare_internal::ncmp::unordered);
  }

 public:
  static const partial_ordering less;
  static const partial_ordering equivalent;
  static const partial_ordering greater;
  static const partial_ordering unordered;

  // Comparisons
  friend constexpr bool operator==(partial_ordering v,
                                   compare_internal::OnlyLiteralZero) noexcept {
    return v.is_ordered() && v.value_ == 0;
  }
  friend constexpr bool operator!=(partial_ordering v,
                                   compare_internal::OnlyLiteralZero) noexcept {
    return !v.is_ordered() || v.value_ != 0;
  }
  friend constexpr bool operator<(partial_ordering v,
                                  compare_internal::OnlyLiteralZero) noexcept {
    return v.is_ordered() && v.value_ < 0;
  }
  friend constexpr bool operator<=(partial_ordering v,
                                   compare_internal::OnlyLiteralZero) noexcept {
    return v.is_ordered() && v.value_ <= 0;
  }
  friend constexpr bool operator>(partial_ordering v,
                                  compare_internal::OnlyLiteralZero) noexcept {
    return v.is_ordered() && v.value_ > 0;
  }
  friend constexpr bool operator>=(partial_ordering v,
                                   compare_internal::OnlyLiteralZero) noexcept {
    return v.is_ordered() && v.value_ >= 0;
  }
  friend constexpr bool operator==(compare_internal::OnlyLiteralZero,
                                   partial_ordering v) noexcept {
    return v.is_ordered() && 0 == v.value_;
  }
  friend constexpr bool operator!=(compare_internal::OnlyLiteralZero,
                                   partial_ordering v) noexcept {
    return !v.is_ordered() || 0 != v.value_;
  }
  friend constexpr bool operator<(compare_internal::OnlyLiteralZero,
                                  partial_ordering v) noexcept {
    return v.is_ordered() && 0 < v.value_;
  }
  friend constexpr bool operator<=(compare_internal::OnlyLiteralZero,
                                   partial_ordering v) noexcept {
    return v.is_ordered() && 0 <= v.value_;
  }
  friend constexpr bool operator>(compare_internal::OnlyLiteralZero,
                                  partial_ordering v) noexcept {
    return v.is_ordered() && 0 > v.value_;
  }
  friend constexpr bool operator>=(compare_internal::OnlyLiteralZero,
                                   partial_ordering v) noexcept {
    return v.is_ordered() && 0 >= v.value_;
  }
  friend constexpr bool operator==(partial_ordering v1,
                                   partial_ordering v2) noexcept {
    return v1.value_ == v2.value_;
  }
  friend constexpr bool operator!=(partial_ordering v1,
                                   partial_ordering v2) noexcept {
    return v1.value_ != v2.value_;
  }

 private:
  compare_internal::value_type value_;
};
inline constexpr partial_ordering partial_ordering::less(
    compare_internal::ord::less);
inline constexpr partial_ordering partial_ordering::equivalent(
    compare_internal::eq::equivalent);
inline constexpr partial_ordering partial_ordering::greater(
    compare_internal::ord::greater);
inline constexpr partial_ordering partial_ordering::unordered(
    compare_internal::ncmp::unordered);

class weak_ordering {
  explicit constexpr weak_ordering(compare_internal::eq v) noexcept
      : value_(static_cast<compare_internal::value_type>(v)) {}
  explicit constexpr weak_ordering(compare_internal::ord v) noexcept
      : value_(static_cast<compare_internal::value_type>(v)) {}

 public:
  static const weak_ordering less;
  static const weak_ordering equivalent;
  static const weak_ordering greater;

  // Conversions
  constexpr operator partial_ordering() const noexcept {  // NOLINT
    return value_ == 0 ? partial_ordering::equivalent
                       : (value_ < 0 ? partial_ordering::less
                                     : partial_ordering::greater);
  }
  // Comparisons
  friend constexpr bool operator==(weak_ordering v,
                                   compare_internal::OnlyLiteralZero) noexcept {
    return v.value_ == 0;
  }
  friend constexpr bool operator!=(weak_ordering v,
                                   compare_internal::OnlyLiteralZero) noexcept {
    return v.value_ != 0;
  }
  friend constexpr bool operator<(weak_ordering v,
                                  compare_internal::OnlyLiteralZero) noexcept {
    return v.value_ < 0;
  }
  friend constexpr bool operator<=(weak_ordering v,
                                   compare_internal::OnlyLiteralZero) noexcept {
    return v.value_ <= 0;
  }
  friend constexpr bool operator>(weak_ordering v,
                                  compare_internal::OnlyLiteralZero) noexcept {
    return v.value_ > 0;
  }
  friend constexpr bool operator>=(weak_ordering v,
                                   compare_internal::OnlyLiteralZero) noexcept {
    return v.value_ >= 0;
  }
  friend constexpr bool operator==(compare_internal::OnlyLiteralZero,
                                   weak_ordering v) noexcept {
    return 0 == v.value_;
  }
  friend constexpr bool operator!=(compare_internal::OnlyLiteralZero,
                                   weak_ordering v) noexcept {
    return 0 != v.value_;
  }
  friend constexpr bool operator<(compare_internal::OnlyLiteralZero,
                                  weak_ordering v) noexcept {
    return 0 < v.value_;
  }
  friend constexpr bool operator<=(compare_internal::OnlyLiteralZero,
                                   weak_ordering v) noexcept {
    return 0 <= v.value_;
  }
  friend constexpr bool operator>(compare_internal::OnlyLiteralZero,
                                  weak_ordering v) noexcept {
    return 0 > v.value_;
  }
  friend constexpr bool operator>=(compare_internal::OnlyLiteralZero,
                                   weak_ordering v) noexcept {
    return 0 >= v.value_;
  }
  friend constexpr bool operator==(weak_ordering v1,
                                   weak_ordering v2) noexcept {
    return v1.value_ == v2.value_;
  }
  friend constexpr bool operator!=(weak_ordering v1,
                                   weak_ordering v2) noexcept {
    return v1.value_ != v2.value_;
  }

 private:
  compare_internal::value_type value_;
};
inline constexpr weak_ordering weak_ordering::less(compare_internal::ord::less);
inline constexpr weak_ordering weak_ordering::equivalent(
    compare_internal::eq::equivalent);
inline constexpr weak_ordering weak_ordering::greater(
    compare_internal::ord::greater);

class strong_ordering {
  explicit constexpr strong_ordering(compare_internal::eq v) noexcept
      : value_(static_cast<compare_internal::value_type>(v)) {}
  explicit constexpr strong_ordering(compare_internal::ord v) noexcept
      : value_(static_cast<compare_internal::value_type>(v)) {}

 public:
  static const strong_ordering less;
  static const strong_ordering equal;
  static const strong_ordering equivalent;
  static const strong_ordering greater;

  // Conversions
  constexpr operator partial_ordering() const noexcept {  // NOLINT
    return value_ == 0 ? partial_ordering::equivalent
                       : (value_ < 0 ? partial_ordering::less
                                     : partial_ordering::greater);
  }
  constexpr operator weak_ordering() const noexcept {  // NOLINT
    return value_ == 0
               ? weak_ordering::equivalent
               : (value_ < 0 ? weak_ordering::less : weak_ordering::greater);
  }
  // Comparisons
  friend constexpr bool operator==(strong_ordering v,
                                   compare_internal::OnlyLiteralZero) noexcept {
    return v.value_ == 0;
  }
  friend constexpr bool operator!=(strong_ordering v,
                                   compare_internal::OnlyLiteralZero) noexcept {
    return v.value_ != 0;
  }
  friend constexpr bool operator<(strong_ordering v,
                                  compare_internal::OnlyLiteralZero) noexcept {
    return v.value_ < 0;
  }
  friend constexpr bool operator<=(strong_ordering v,
                                   compare_internal::OnlyLiteralZero) noexcept {
    return v.value_ <= 0;
  }
  friend constexpr bool operator>(strong_ordering v,
                                  compare_internal::OnlyLiteralZero) noexcept {
    return v.value_ > 0;
  }
  friend constexpr bool operator>=(strong_ordering v,
                                   compare_internal::OnlyLiteralZero) noexcept {
    return v.value_ >= 0;
  }
  friend constexpr bool operator==(compare_internal::OnlyLiteralZero,
                                   strong_ordering v) noexcept {
    return 0 == v.value_;
  }
  friend constexpr bool operator!=(compare_internal::OnlyLiteralZero,
                                   strong_ordering v) noexcept {
    return 0 != v.value_;
  }
  friend constexpr bool operator<(compare_internal::OnlyLiteralZero,
                                  strong_ordering v) noexcept {
    return 0 < v.value_;
  }
  friend constexpr bool operator<=(compare_internal::OnlyLiteralZero,
                                   strong_ordering v) noexcept {
    return 0 <= v.value_;
  }
  friend constexpr bool operator>(compare_internal::OnlyLiteralZero,
                                  strong_ordering v) noexcept {
    return 0 > v.value_;
  }
  friend constexpr bool operator>=(compare_internal::OnlyLiteralZero,
                                   strong_ordering v) noexcept {
    return 0 >= v.value_;
  }
  friend constexpr bool operator==(strong_ordering v1,
                                   strong_ordering v2) noexcept {
    return v1.value_ == v2.value_;
  }
  friend constexpr bool operator!=(strong_ordering v1,
                                   strong_ordering v2) noexcept {
    return v1.value_ != v2.value_;
  }

 private:
  compare_internal::value_type value_;
};
inline constexpr strong_ordering strong_ordering::less(
    compare_internal::ord::less);
inline constexpr strong_ordering strong_ordering::equal(
    compare_internal::eq::equal);
inline constexpr strong_ordering strong_ordering::equivalent(
    compare_internal::eq::equivalent);
inline constexpr strong_ordering strong_ordering::greater(
    compare_internal::ord::greater);

#endif  // ABSL_USES_STD_ORDERING

namespace compare_internal {
// We also provide these comparator adapter functions for internal absl use.

// Helper functions to do a boolean comparison of two keys given a boolean
// or three-way comparator.
// SFINAE prevents implicit conversions to bool (such as from int).
template <typename BoolT,
          std::enable_if_t<std::is_same_v<bool, BoolT>, int> = 0>
constexpr bool compare_result_as_less_than(const BoolT r) {
  return r;
}
constexpr bool compare_result_as_less_than(const absl::weak_ordering r) {
  return r < 0;
}

template <typename Compare, typename K, typename LK>
constexpr bool do_less_than_comparison(const Compare& compare, const K& x,
                                       const LK& y) {
  return compare_result_as_less_than(compare(x, y));
}

// Helper functions to do a three-way comparison of two keys given a boolean or
// three-way comparator.
// SFINAE prevents implicit conversions to int (such as from bool).
template <typename Int, std::enable_if_t<std::is_same_v<int, Int>, int> = 0>
constexpr absl::weak_ordering compare_result_as_ordering(const Int c) {
  return c < 0    ? absl::weak_ordering::less
         : c == 0 ? absl::weak_ordering::equivalent
                  : absl::weak_ordering::greater;
}
constexpr absl::weak_ordering compare_result_as_ordering(
    const absl::weak_ordering c) {
  return c;
}

template <
    typename Compare, typename K, typename LK,
    std::enable_if_t<
        !std::is_same_v<bool,
                        std::invoke_result_t<Compare, const K&, const LK&>>,
        int> = 0>
constexpr absl::weak_ordering do_three_way_comparison(const Compare& compare,
                                                      const K& x, const LK& y) {
  return compare_result_as_ordering(compare(x, y));
}
template <
    typename Compare, typename K, typename LK,
    std::enable_if_t<
        std::is_same_v<bool,
                       std::invoke_result_t<Compare, const K&, const LK&>>,
        int> = 0>
constexpr absl::weak_ordering do_three_way_comparison(const Compare& compare,
                                                      const K& x, const LK& y) {
  return compare(x, y)   ? absl::weak_ordering::less
         : compare(y, x) ? absl::weak_ordering::greater
                         : absl::weak_ordering::equivalent;
}

}  // namespace compare_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_TYPES_COMPARE_H_
