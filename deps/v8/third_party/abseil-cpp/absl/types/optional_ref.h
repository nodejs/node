// Copyright 2026 The Abseil Authors.
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
// File: optional_ref.h
// -----------------------------------------------------------------------------
//
// `optional_ref<T>` provides a `std::optional`-like interface around `T*`.
// It is similar to C++26's `std::optional<T&>`, but with slight enhancements,
// such as the fact that it permits construction from rvalues. That is, it
// relaxes the std::reference_constructs_from_temporary constraint. Its intent
// is to make it easier for functions to accept nullable object addresses,
// regardless of whether or not they point to temporaries.
//
// It can be constructed in the following ways:
//   * optional_ref<T> ref;
//   * optional_ref<T> ref = std::nullopt;
//   * T foo; optional_ref<T> ref = foo;
//   * std::optional<T> foo; optional_ref<T> ref = foo;
//   * T* foo = ...; optional_ref<T> ref = foo;
//   * optional_ref<T> foo; optional_ref<const T> ref = foo;
//
// Since it is trivially copyable and destructible, it should be passed by
// value.
//
// Other properties:
//   * Assignment is not allowed. Example:
//       optional_ref<int> ref;
//       // Compile error.
//       ref = 2;
//
//   * operator bool() is intentionally not defined, as it would be error prone
//     for optional_ref<bool>.
//
// Example usage, assuming some type `T` that is expensive to copy:
//   void ProcessT(optional_ref<const T> input) {
//     if (!input.has_value()) {
//       // Handle empty case.
//       return;
//     }
//     const T& val = *input;
//     // Do something with val.
//   }
//
//   ProcessT(std::nullopt);
//   ProcessT(BuildT());

#ifndef ABSL_TYPES_OPTIONAL_REF_H_
#define ABSL_TYPES_OPTIONAL_REF_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

template <typename T>
class optional_ref {
  template <typename U>
  using EnableIfConvertibleFrom =
      std::enable_if_t<std::is_convertible_v<U*, T*>>;

 public:
  using value_type = T;

  constexpr optional_ref() : ptr_(nullptr) {}
  constexpr optional_ref(  // NOLINT(google-explicit-constructor)
      std::nullopt_t)
      : ptr_(nullptr) {}

  // Constructor given a concrete value.
  constexpr optional_ref(  // NOLINT(google-explicit-constructor)
      T& input ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : ptr_(std::addressof(input)) {}

  // Constructors given an existing std::optional value.
  // Templated on the input optional's type to avoid creating a temporary.
  template <typename U, typename = EnableIfConvertibleFrom<const U>>
  constexpr optional_ref(  // NOLINT(google-explicit-constructor)
      const std::optional<U>& input ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : ptr_(input.has_value() ? std::addressof(*input) : nullptr) {}
  template <typename U, typename = EnableIfConvertibleFrom<U>>
  constexpr optional_ref(  // NOLINT(google-explicit-constructor)
      std::optional<U>& input ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : ptr_(input.has_value() ? std::addressof(*input) : nullptr) {}

  // Constructor given a T*, where nullptr indicates empty/absent.
  constexpr optional_ref(  // NOLINT(google-explicit-constructor)
      T* input ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : ptr_(input) {}

  // Don't allow naked nullptr as input, as this creates confusion in the case
  // of optional_ref<T*>. Use std::nullopt instead to create an empty
  // optional_ref.
  constexpr optional_ref(  // NOLINT(google-explicit-constructor)
      std::nullptr_t) = delete;

  // Copying is allowed.
  optional_ref(const optional_ref<T>&) = default;
  // Assignment is not allowed.
  optional_ref<T>& operator=(const optional_ref<T>&) = delete;

  // Conversion from optional_ref<U> is allowed iff U* is convertible to T*.
  // (Note this also allows non-const to const conversions.)
  template <typename U, typename = EnableIfConvertibleFrom<U>>
  constexpr optional_ref(  // NOLINT(google-explicit-constructor)
      optional_ref<U> input)
      : ptr_(input.as_pointer()) {}

  // Determines whether the `optional_ref` contains a value. Returns `false` if
  // and only if `*this` is empty.
  constexpr bool has_value() const { return ptr_ != nullptr; }

  // Returns a reference to an `optional_ref`s underlying value. The constness
  // and lvalue/rvalue-ness of the `optional_ref` is preserved to the view of
  // the `T` sub-object. Throws the same error as `std::optional`'s `value()`
  // when the `optional_ref` is empty.
  constexpr T& value() const {
    return ABSL_PREDICT_TRUE(ptr_ != nullptr)
               ? *ptr_
               // Replicate the same error logic as in `std::optional`'s
               // `value()`. It either throws an exception or aborts the
               // program. We intentionally ignore the return value of
               // the constructed optional's value as we only need to run
               // the code for error checking.
               : ((void)std::optional<T>().value(), *ptr_);
  }

  // Returns the value iff *this has a value, otherwise returns `default_value`.
  template <typename U>
  constexpr T value_or(U&& default_value) const {
    // Instantiate std::optional<T>::value_or(U) to trigger its static_asserts.
    if (false) {
      // We use `std::add_const_t` here since just using `const` makes MSVC
      // complain about the syntax.
      (void)std::add_const_t<std::optional<T>>{}.value_or(
          std::forward<U>(default_value));
    }
    return ptr_ != nullptr ? *ptr_
                           : static_cast<T>(std::forward<U>(default_value));
  }

  // Accesses the underlying `T` value of an `optional_ref`. If the
  // `optional_ref` is empty, behavior is undefined.
  constexpr T& operator*() const {
    ABSL_HARDENING_ASSERT(ptr_ != nullptr);
    return *ptr_;
  }
  constexpr T* operator->() const {
    ABSL_HARDENING_ASSERT(ptr_ != nullptr);
    return ptr_;
  }

  // Convenience function to represent the `optional_ref` as a `T*` pointer.
  constexpr T* as_pointer() const { return ptr_; }
  // Convenience function to represent the `optional_ref` as an `optional`,
  // which incurs a copy when the `optional_ref` is non-empty. The template type
  // allows for implicit type conversion; example:
  //   optional_ref<std::string> a = ...;
  //   std::optional<std::string_view> b = a.as_optional<std::string_view>();
  template <typename U = std::decay_t<T>>
  constexpr std::optional<U> as_optional() const {
    if (ptr_ == nullptr) return std::nullopt;
    return *ptr_;
  }

 private:
  T* const ptr_;

  // T constraint checks.  You can't have an optional of nullopt_t or
  // in_place_t.
  static_assert(!std::is_same_v<std::nullopt_t, std::remove_cv_t<T>>,
                "optional_ref<nullopt_t> is not allowed.");
  static_assert(!std::is_same_v<std::in_place_t, std::remove_cv_t<T>>,
                "optional_ref<in_place_t> is not allowed.");
};

// Template type deduction guides:

template <typename T>
optional_ref(const T&) -> optional_ref<const T>;
template <typename T>
optional_ref(T&) -> optional_ref<T>;

template <typename T>
optional_ref(const std::optional<T>&) -> optional_ref<const T>;
template <typename T>
optional_ref(std::optional<T>&) -> optional_ref<T>;

template <typename T>
optional_ref(T*) -> optional_ref<T>;

namespace optional_ref_internal {

// This is a C++-11 compatible version of std::equality_comparable_with that
// only requires `t == u` is a valid boolean expression.
//
// We still need this for a couple reasons:
// -  As of 2026-02-13, Abseil supports C++17.
//  - Even for targets that are built with the default toolchain, using
//    std::equality_comparable_with gives us an error due to mutual recursion
//    between its definition and our definition of operator==.
//
template <typename T, typename U>
using enable_if_equality_comparable_t = std::enable_if_t<std::is_convertible_v<
    decltype(std::declval<T>() == std::declval<U>()), bool>>;

}  // namespace optional_ref_internal

// Compare an optional referenced value to std::nullopt.

template <typename T>
constexpr bool operator==(optional_ref<T> a, std::nullopt_t) {
  return !a.has_value();
}
template <typename T>
constexpr bool operator==(std::nullopt_t, optional_ref<T> b) {
  return !b.has_value();
}
template <typename T>
constexpr bool operator!=(optional_ref<T> a, std::nullopt_t) {
  return a.has_value();
}
template <typename T>
constexpr bool operator!=(std::nullopt_t, optional_ref<T> b) {
  return b.has_value();
}

// Compare two optional referenced values. Note, this does not test that the
// contained `ptr_`s are equal. If the caller wants "shallow" reference equality
// semantics, they should use `as_pointer()` explicitly.

template <typename T, typename U>
constexpr bool operator==(optional_ref<T> a, optional_ref<U> b) {
  return a.has_value() ? *a == b : !b.has_value();
}

// Compare an optional referenced value to a non-optional value.

template <
    typename T, typename U,
    typename = optional_ref_internal::enable_if_equality_comparable_t<T, U>>
constexpr bool operator==(const T& a, optional_ref<U> b) {
  return b.has_value() && a == *b;
}
template <
    typename T, typename U,
    typename = optional_ref_internal::enable_if_equality_comparable_t<T, U>>
constexpr bool operator==(optional_ref<T> a, const U& b) {
  return b == a;
}

// Inequality operators, as above.

template <typename T, typename U>
constexpr bool operator!=(optional_ref<T> a, optional_ref<U> b) {
  return !(a == b);
}
template <
    typename T, typename U,
    typename = optional_ref_internal::enable_if_equality_comparable_t<T, U>>
constexpr bool operator!=(optional_ref<T> a, const U& b) {
  return !(a == b);
}
template <
    typename T, typename U,
    typename = optional_ref_internal::enable_if_equality_comparable_t<T, U>>
constexpr bool operator!=(const T& a, optional_ref<U> b) {
  return !(a == b);
}

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_TYPES_OPTIONAL_REF_H_
