//
// Copyright 2017 The Abseil Authors.
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
// type_traits.h
// -----------------------------------------------------------------------------
//
// This file contains C++11-compatible versions of standard <type_traits> API
// functions for determining the characteristics of types. Such traits can
// support type inference, classification, and transformation, as well as
// make it easier to write templates based on generic type behavior.
//
// See https://en.cppreference.com/w/cpp/header/type_traits
//
// WARNING: use of many of the constructs in this header will count as "complex
// template metaprogramming", so before proceeding, please carefully consider
// https://google.github.io/styleguide/cppguide.html#Template_metaprogramming
//
// WARNING: using template metaprogramming to detect or depend on API
// features is brittle and not guaranteed. Neither the standard library nor
// Abseil provides any guarantee that APIs are stable in the face of template
// metaprogramming. Use with caution.
#ifndef ABSL_META_TYPE_TRAITS_H_
#define ABSL_META_TYPE_TRAITS_H_

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/config.h"

#ifdef __cpp_lib_span
#include <span>  // NOLINT(build/c++20)
#endif

// Defines the default alignment. `__STDCPP_DEFAULT_NEW_ALIGNMENT__` is a C++17
// feature.
#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
#define ABSL_INTERNAL_DEFAULT_NEW_ALIGNMENT __STDCPP_DEFAULT_NEW_ALIGNMENT__
#else  // defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
#define ABSL_INTERNAL_DEFAULT_NEW_ALIGNMENT alignof(std::max_align_t)
#endif  // defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)

namespace absl {
ABSL_NAMESPACE_BEGIN

namespace type_traits_internal {

template <typename... Ts>
struct VoidTImpl {
  using type = void;
};

////////////////////////////////
// Library Fundamentals V2 TS //
////////////////////////////////

// NOTE: The `is_detected` family of templates here differ from the library
// fundamentals specification in that for library fundamentals, `Op<Args...>` is
// evaluated as soon as the type `is_detected<Op, Args...>` undergoes
// substitution, regardless of whether or not the `::value` is accessed. That
// is inconsistent with all other standard traits and prevents lazy evaluation
// in larger contexts (such as if the `is_detected` check is a trailing argument
// of a `conjunction`. This implementation opts to instead be lazy in the same
// way that the standard traits are (this "defect" of the detection idiom
// specifications has been reported).

template <class Enabler, template <class...> class Op, class... Args>
struct is_detected_impl {
  using type = std::false_type;
};

template <template <class...> class Op, class... Args>
struct is_detected_impl<typename VoidTImpl<Op<Args...>>::type, Op, Args...> {
  using type = std::true_type;
};

template <template <class...> class Op, class... Args>
struct is_detected : is_detected_impl<void, Op, Args...>::type {};

}  // namespace type_traits_internal

// void_t()
//
// Ignores the type of any its arguments and returns `void`. In general, this
// metafunction allows you to create a general case that maps to `void` while
// allowing specializations that map to specific types.
//
// This metafunction is not 100% compatible with the C++17 `std::void_t`
// metafunction. It has slightly different behavior, such as when ordering
// partial specializations. It is recommended to use `std::void_t` instead.
template <typename... Ts>
using void_t = typename type_traits_internal::VoidTImpl<Ts...>::type;

// Historical note: Abseil once provided implementations of these type traits
// for platforms that lacked full support. New code should prefer to use the
// std variants.
//
// See the documentation for the STL <type_traits> header for more information:
// https://en.cppreference.com/w/cpp/header/type_traits
using std::add_const_t;
using std::add_cv_t;
using std::add_lvalue_reference_t;
using std::add_pointer_t;
using std::add_rvalue_reference_t;
using std::add_volatile_t;
using std::common_type_t;
using std::conditional_t;
using std::conjunction;
using std::decay_t;
using std::enable_if_t;
using std::disjunction;
using std::is_copy_assignable;
using std::is_function;
using std::is_move_assignable;
using std::is_trivially_copy_assignable;
using std::is_trivially_copy_constructible;
using std::is_trivially_default_constructible;
using std::is_trivially_destructible;
using std::is_trivially_move_assignable;
using std::is_trivially_move_constructible;
using std::make_signed_t;
using std::make_unsigned_t;
using std::negation;
using std::remove_all_extents_t;
using std::remove_const_t;
using std::remove_cv_t;
using std::remove_extent_t;
using std::remove_pointer_t;
using std::remove_reference_t;
using std::remove_volatile_t;
using std::underlying_type_t;

#if defined(__cpp_lib_remove_cvref) && __cpp_lib_remove_cvref >= 201711L
template <typename T>
using remove_cvref = std::remove_cvref<T>;

template <typename T>
using remove_cvref_t = typename std::remove_cvref<T>::type;
#else
// remove_cvref()
//
// C++11 compatible implementation of std::remove_cvref which was added in
// C++20.
template <typename T>
struct remove_cvref {
  using type =
      typename std::remove_cv<typename std::remove_reference<T>::type>::type;
};

template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;
#endif

namespace type_traits_internal {

#if (defined(__cpp_lib_is_invocable) && __cpp_lib_is_invocable >= 201703L) || \
    (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L)
// std::result_of is deprecated (C++17) or removed (C++20)
template <typename>
struct result_of;
template <typename F, typename... Args>
struct result_of<F(Args...)> : std::invoke_result<F, Args...> {};
#else
template <typename F>
using result_of = std::result_of<F>;
#endif

}  // namespace type_traits_internal

template <typename F>
using result_of_t = typename type_traits_internal::result_of<F>::type;

namespace type_traits_internal {
// In MSVC we can't probe std::hash or stdext::hash because it triggers a
// static_assert instead of failing substitution. Libc++ prior to 4.0
// also used a static_assert.
//
#if defined(_MSC_VER) || (defined(_LIBCPP_VERSION) && \
                          _LIBCPP_VERSION < 4000 && _LIBCPP_STD_VER > 11)
#define ABSL_META_INTERNAL_STD_HASH_SFINAE_FRIENDLY_ 0
#else
#define ABSL_META_INTERNAL_STD_HASH_SFINAE_FRIENDLY_ 1
#endif

#if !ABSL_META_INTERNAL_STD_HASH_SFINAE_FRIENDLY_
template <typename Key, typename = size_t>
struct IsHashable : std::true_type {};
#else   // ABSL_META_INTERNAL_STD_HASH_SFINAE_FRIENDLY_
template <typename Key, typename = void>
struct IsHashable : std::false_type {};

template <typename Key>
struct IsHashable<
    Key,
    absl::enable_if_t<std::is_convertible<
        decltype(std::declval<std::hash<Key>&>()(std::declval<Key const&>())),
        std::size_t>::value>> : std::true_type {};
#endif  // !ABSL_META_INTERNAL_STD_HASH_SFINAE_FRIENDLY_

struct AssertHashEnabledHelper {
 private:
  static void Sink(...) {}
  struct NAT {};

  template <class Key>
  static auto GetReturnType(int)
      -> decltype(std::declval<std::hash<Key>>()(std::declval<Key const&>()));
  template <class Key>
  static NAT GetReturnType(...);

  template <class Key>
  static std::nullptr_t DoIt() {
    static_assert(IsHashable<Key>::value,
                  "std::hash<Key> does not provide a call operator");
    static_assert(
        std::is_default_constructible<std::hash<Key>>::value,
        "std::hash<Key> must be default constructible when it is enabled");
    static_assert(
        std::is_copy_constructible<std::hash<Key>>::value,
        "std::hash<Key> must be copy constructible when it is enabled");
    static_assert(absl::is_copy_assignable<std::hash<Key>>::value,
                  "std::hash<Key> must be copy assignable when it is enabled");
    // is_destructible is unchecked as it's implied by each of the
    // is_constructible checks.
    using ReturnType = decltype(GetReturnType<Key>(0));
    static_assert(std::is_same<ReturnType, NAT>::value ||
                      std::is_same<ReturnType, size_t>::value,
                  "std::hash<Key> must return size_t");
    return nullptr;
  }

  template <class... Ts>
  friend void AssertHashEnabled();
};

template <class... Ts>
inline void AssertHashEnabled() {
  using Helper = AssertHashEnabledHelper;
  Helper::Sink(Helper::DoIt<Ts>()...);
}

}  // namespace type_traits_internal

// An internal namespace that is required to implement the C++17 swap traits.
// It is not further nested in type_traits_internal to avoid long symbol names.
namespace swap_internal {

// Necessary for the traits.
using std::swap;

// This declaration prevents global `swap` and `absl::swap` overloads from being
// considered unless ADL picks them up.
void swap();

template <class T>
using IsSwappableImpl = decltype(swap(std::declval<T&>(), std::declval<T&>()));

// NOTE: This dance with the default template parameter is for MSVC.
template <class T,
          class IsNoexcept = std::integral_constant<
              bool, noexcept(swap(std::declval<T&>(), std::declval<T&>()))>>
using IsNothrowSwappableImpl = typename std::enable_if<IsNoexcept::value>::type;

// IsSwappable
//
// Determines whether the standard swap idiom is a valid expression for
// arguments of type `T`.
template <class T>
struct IsSwappable
    : absl::type_traits_internal::is_detected<IsSwappableImpl, T> {};

// IsNothrowSwappable
//
// Determines whether the standard swap idiom is a valid expression for
// arguments of type `T` and is noexcept.
template <class T>
struct IsNothrowSwappable
    : absl::type_traits_internal::is_detected<IsNothrowSwappableImpl, T> {};

// Swap()
//
// Performs the swap idiom from a namespace where valid candidates may only be
// found in `std` or via ADL.
template <class T, absl::enable_if_t<IsSwappable<T>::value, int> = 0>
void Swap(T& lhs, T& rhs) noexcept(IsNothrowSwappable<T>::value) {
  swap(lhs, rhs);
}

// StdSwapIsUnconstrained
//
// Some standard library implementations are broken in that they do not
// constrain `std::swap`. This will effectively tell us if we are dealing with
// one of those implementations.
using StdSwapIsUnconstrained = IsSwappable<void()>;

}  // namespace swap_internal

namespace type_traits_internal {

// Make the swap-related traits/function accessible from this namespace.
using swap_internal::IsNothrowSwappable;
using swap_internal::IsSwappable;
using swap_internal::StdSwapIsUnconstrained;
using swap_internal::Swap;

}  // namespace type_traits_internal

// absl::is_trivially_relocatable<T>
//
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p2786r11.html
//
// Detects whether a type is known to be "trivially relocatable" -- meaning it
// can be relocated from one place to another as if by memcpy/memmove.
// This implies that its object representation doesn't depend on its address,
// and also none of its special member functions do anything strange.
//
// Note that when relocating the caller code should ensure that if the object is
// polymorphic, the dynamic type is of the most derived type. Padding bytes
// should not be copied.
//
// This trait is conservative. If it's true then the type is definitely
// trivially relocatable, but if it's false then the type may or may not be. For
// example, std::vector<int> is trivially relocatable on every known STL
// implementation, but absl::is_trivially_relocatable<std::vector<int>> remains
// false.
//
// Example:
//
// if constexpr (absl::is_trivially_relocatable<T>::value) {
//   memcpy(new_location, old_location, sizeof(T));
// } else {
//   new(new_location) T(std::move(*old_location));
//   old_location->~T();
// }
//
// Upstream documentation:
//
// https://clang.llvm.org/docs/LanguageExtensions.html#:~:text=__builtin_is_cpp_trivially_relocatable
//
// Clang on Windows has the builtin, but it falsely claims types with a
// user-provided destructor are trivial (http://b/275003464). So we opt out
// there.
//
// TODO(b/275003464): remove the opt-out once the bug is fixed.
//
// Starting with Xcode 15, the Apple compiler will falsely say a type
// with a user-provided move constructor is trivially relocatable
// (b/324278148). We will opt out without a version check, due to
// the fluidity of Apple versions.
//
// TODO(b/324278148): If all versions we use have the bug fixed, then
// remove the condition.
//
// Clang on all platforms fails to detect that a type with a user-provided
// move-assignment operator is not trivially relocatable so we also check for
// is_trivially_move_assignable for Clang.
//
// TODO(b/325479096): Remove the Clang is_trivially_move_assignable version once
// Clang's behavior is fixed.
//
// According to https://github.com/abseil/abseil-cpp/issues/1479, this does not
// work with NVCC either.
#if ABSL_HAVE_BUILTIN(__builtin_is_cpp_trivially_relocatable)
// https://github.com/llvm/llvm-project/pull/127636#pullrequestreview-2637005293
// In the current implementation, __builtin_is_cpp_trivially_relocatable will
// only return true for types that are trivially relocatable according to the
// standard. Notably, this means that marking a type [[clang::trivial_abi]] aka
// ABSL_HAVE_ATTRIBUTE_TRIVIAL_ABI will have no effect on this trait.
template <class T>
struct is_trivially_relocatable
    : std::integral_constant<bool, __builtin_is_cpp_trivially_relocatable(T)> {
};
#elif ABSL_HAVE_BUILTIN(__is_trivially_relocatable) && defined(__clang__) && \
    !(defined(_WIN32) || defined(_WIN64)) && !defined(__APPLE__) &&          \
    !defined(__NVCC__)
// https://github.com/llvm/llvm-project/pull/139061
//  __is_trivially_relocatable is deprecated.
// TODO(b/325479096): Remove this case.
template <class T>
struct is_trivially_relocatable
    : std::integral_constant<
          bool, std::is_trivially_copyable<T>::value ||
                    (__is_trivially_relocatable(T) &&
                     std::is_trivially_move_assignable<T>::value)> {};
#else
// Otherwise we use a fallback that detects only those types we can feasibly
// detect. Any type that is trivially copyable is by definition trivially
// relocatable.
template <class T>
struct is_trivially_relocatable : std::is_trivially_copyable<T> {};
#endif

// absl::is_constant_evaluated()
//
// Detects whether the function call occurs within a constant-evaluated context.
// Returns true if the evaluation of the call occurs within the evaluation of an
// expression or conversion that is manifestly constant-evaluated; otherwise
// returns false.
//
// This function is implemented in terms of `std::is_constant_evaluated` for
// c++20 and up. For older c++ versions, the function is implemented in terms
// of `__builtin_is_constant_evaluated` if available, otherwise the function
// will fail to compile.
//
// Applications can inspect `ABSL_HAVE_CONSTANT_EVALUATED` at compile time
// to check if this function is supported.
//
// Example:
//
// constexpr MyClass::MyClass(int param) {
// #ifdef ABSL_HAVE_CONSTANT_EVALUATED
//   if (!absl::is_constant_evaluated()) {
//     ABSL_LOG(INFO) << "MyClass(" << param << ")";
//   }
// #endif  // ABSL_HAVE_CONSTANT_EVALUATED
// }
//
// Upstream documentation:
//
// http://en.cppreference.com/w/cpp/types/is_constant_evaluated
// http://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html#:~:text=__builtin_is_constant_evaluated
//
#if defined(ABSL_HAVE_CONSTANT_EVALUATED)
constexpr bool is_constant_evaluated() noexcept {
#ifdef __cpp_lib_is_constant_evaluated
  return std::is_constant_evaluated();
#elif ABSL_HAVE_BUILTIN(__builtin_is_constant_evaluated)
  return __builtin_is_constant_evaluated();
#endif
}
#endif  // ABSL_HAVE_CONSTANT_EVALUATED

namespace type_traits_internal {

// Detects if a class's definition has declared itself to be an owner by
// declaring
//   using absl_internal_is_view = std::true_type;
// as a member.
// Types that don't want either must either omit this declaration entirely, or
// (if e.g. inheriting from a base class) define the member to something that
// isn't a Boolean trait class, such as `void`.
// Do not specialize or use this directly. It's an implementation detail.
template <typename T, typename = void>
struct IsOwnerImpl : std::false_type {
  static_assert(std::is_same<T, absl::remove_cvref_t<T>>::value,
                "type must lack qualifiers");
};

template <typename T>
struct IsOwnerImpl<
    T,
    std::enable_if_t<std::is_class<typename T::absl_internal_is_view>::value>>
    : absl::negation<typename T::absl_internal_is_view> {};

// A trait to determine whether a type is an owner.
// Do *not* depend on the correctness of this trait for correct code behavior.
// It is only a safety feature and its value may change in the future.
// Do not specialize this; instead, define the member trait inside your type so
// that it can be auto-detected, and to prevent ODR violations.
// If it ever becomes possible to detect [[gsl::Owner]], we should leverage it:
// https://wg21.link/p1179
template <typename T>
struct IsOwner : IsOwnerImpl<T> {};

template <typename T, typename Traits, typename Alloc>
struct IsOwner<std::basic_string<T, Traits, Alloc>> : std::true_type {};

template <typename T, typename Alloc>
struct IsOwner<std::vector<T, Alloc>> : std::true_type {};

// Detects if a class's definition has declared itself to be a view by declaring
//   using absl_internal_is_view = std::true_type;
// as a member.
// Do not specialize or use this directly.
template <typename T, typename = void>
struct IsViewImpl : std::false_type {
  static_assert(std::is_same<T, absl::remove_cvref_t<T>>::value,
                "type must lack qualifiers");
};

template <typename T>
struct IsViewImpl<
    T,
    std::enable_if_t<std::is_class<typename T::absl_internal_is_view>::value>>
    : T::absl_internal_is_view {};

// A trait to determine whether a type is a view.
// Do *not* depend on the correctness of this trait for correct code behavior.
// It is only a safety feature, and its value may change in the future.
// Do not specialize this trait. Instead, define the member
//   using absl_internal_is_view = std::true_type;
// in your class to allow its detection while preventing ODR violations.
// If it ever becomes possible to detect [[gsl::Pointer]], we should leverage
// it: https://wg21.link/p1179
template <typename T>
struct IsView : std::integral_constant<bool, std::is_pointer<T>::value ||
                                                 IsViewImpl<T>::value> {};

template <typename Char, typename Traits>
struct IsView<std::basic_string_view<Char, Traits>> : std::true_type {};

#ifdef __cpp_lib_span
template <typename T>
struct IsView<std::span<T>> : std::true_type {};
#endif

// Determines whether the assignment of the given types is lifetime-bound.
// Do *not* depend on the correctness of this trait for correct code behavior.
// It is only a safety feature and its value may change in the future.
// If it ever becomes possible to detect [[clang::lifetimebound]] directly,
// we should change the implementation to leverage that.
// Until then, we consider an assignment from an "owner" (such as std::string)
// to a "view" (such as std::string_view) to be a lifetime-bound assignment.
template <typename T, typename U>
using IsLifetimeBoundAssignment = absl::conjunction<
    std::integral_constant<bool, !std::is_lvalue_reference<U>::value>,
    IsOwner<absl::remove_cvref_t<U>>, IsView<absl::remove_cvref_t<T>>>;

}  // namespace type_traits_internal

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_META_TYPE_TRAITS_H_
