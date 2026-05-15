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
// File: casts.h
// -----------------------------------------------------------------------------
//
// This header file defines casting templates to fit use cases not covered by
// the standard casts provided in the C++ standard. As with all cast operations,
// use these with caution and only if alternatives do not exist.

#ifndef ABSL_BASE_CASTS_H_
#define ABSL_BASE_CASTS_H_

#include <cstring>
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <utility>

#ifdef __has_include
#if __has_include(<version>)
#include <version>  // For __cpp_lib_bit_cast.
#endif
#endif

#if defined(__cpp_lib_bit_cast) && __cpp_lib_bit_cast >= 201806L
#include <bit>  // For std::bit_cast.
#endif  // defined(__cpp_lib_bit_cast) && __cpp_lib_bit_cast >= 201806L

#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/base/options.h"
#include "absl/meta/type_traits.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

// implicit_cast()
//
// Performs an implicit conversion between types following the language
// rules for implicit conversion; if an implicit conversion is otherwise
// allowed by the language in the given context, this function performs such an
// implicit conversion.
//
// Example:
//
//   // If the context allows implicit conversion:
//   From from;
//   To to = from;
//
//   // Such code can be replaced by:
//   implicit_cast<To>(from);
//
// An `implicit_cast()` may also be used to annotate numeric type conversions
// that, although safe, may produce compiler warnings (such as `long` to `int`).
// Additionally, an `implicit_cast()` is also useful within return statements to
// indicate a specific implicit conversion is being undertaken.
//
// Example:
//
//   return implicit_cast<double>(size_in_bytes) / capacity_;
//
// Annotating code with `implicit_cast()` allows you to explicitly select
// particular overloads and template instantiations, while providing a safer
// cast than `reinterpret_cast()` or `static_cast()`.
//
// Additionally, an `implicit_cast()` can be used to allow upcasting within a
// type hierarchy where incorrect use of `static_cast()` could accidentally
// allow downcasting.
//
// Finally, an `implicit_cast()` can be used to perform implicit conversions
// from unrelated types that otherwise couldn't be implicitly cast directly;
// C++ will normally only implicitly cast "one step" in such conversions.
//
// That is, if C is a type which can be implicitly converted to B, with B being
// a type that can be implicitly converted to A, an `implicit_cast()` can be
// used to convert C to B (which the compiler can then implicitly convert to A
// using language rules).
//
// Example:
//
//   // Assume an object C is convertible to B, which is implicitly convertible
//   // to A
//   A a = implicit_cast<B>(C);
//
// Such implicit cast chaining may be useful within template logic.
template <typename To>
constexpr std::enable_if_t<
    !type_traits_internal::IsView<std::enable_if_t<
        !std::is_reference_v<To>, std::remove_cv_t<To>>>::value,
    To>
implicit_cast(absl::type_identity_t<To> to) {
  return to;
}
template <typename To>
constexpr std::enable_if_t<
    type_traits_internal::IsView<std::enable_if_t<!std::is_reference_v<To>,
                                                  std::remove_cv_t<To>>>::value,
    To>
implicit_cast(absl::type_identity_t<To> to ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return to;
}
template <typename To>
constexpr std::enable_if_t<std::is_reference_v<To>, To> implicit_cast(
    absl::type_identity_t<To> to ABSL_ATTRIBUTE_LIFETIME_BOUND) {
  return std::forward<absl::type_identity_t<To>>(to);
}

// bit_cast()
//
// Creates a value of the new type `Dest` whose representation is the same as
// that of the argument, which is of (deduced) type `Source` (a "bitwise cast";
// every bit in the value representation of the result is equal to the
// corresponding bit in the object representation of the source). Source and
// destination types must be of the same size, and both types must be trivially
// copyable.
//
// As with most casts, use with caution. A `bit_cast()` might be needed when you
// need to treat a value as the value of some other type, for example, to access
// the individual bits of an object which are not normally accessible through
// the object's type, such as for working with the binary representation of a
// floating point value:
//
//   float f = 3.14159265358979;
//   int i = bit_cast<int>(f);
//   // i = 0x40490fdb
//
// Reinterpreting and accessing a value directly as a different type (as shown
// below) usually results in undefined behavior.
//
// Example:
//
//   // WRONG
//   float f = 3.14159265358979;
//   int i = reinterpret_cast<int&>(f);    // Wrong
//   int j = *reinterpret_cast<int*>(&f);  // Equally wrong
//   int k = *bit_cast<int*>(&f);          // Equally wrong
//
// Reinterpret-casting results in undefined behavior according to the ISO C++
// specification, section [basic.lval]. Roughly, this section says: if an object
// in memory has one type, and a program accesses it with a different type, the
// result is undefined behavior for most "different type".
//
// Using bit_cast on a pointer and then dereferencing it is no better than using
// reinterpret_cast. You should only use bit_cast on the value itself.
//
// Such casting results in type punning: holding an object in memory of one type
// and reading its bits back using a different type. A `bit_cast()` avoids this
// issue by copying the object representation to a new value, which avoids
// introducing this undefined behavior (since the original value is never
// accessed in the wrong way).
//
// The requirements of `absl::bit_cast` are more strict than that of
// `std::bit_cast` unless compiler support is available. Specifically, without
// compiler support, this implementation also requires `Dest` to be
// default-constructible. In C++20, `absl::bit_cast` is replaced by
// `std::bit_cast`.
#if defined(__cpp_lib_bit_cast) && __cpp_lib_bit_cast >= 201806L

using std::bit_cast;

#else  // defined(__cpp_lib_bit_cast) && __cpp_lib_bit_cast >= 201806L

template <
    typename Dest, typename Source,
    typename std::enable_if<sizeof(Dest) == sizeof(Source) &&
                                std::is_trivially_copyable<Source>::value &&
                                std::is_trivially_copyable<Dest>::value
#if !ABSL_HAVE_BUILTIN(__builtin_bit_cast)
                                && std::is_default_constructible<Dest>::value
#endif  // !ABSL_HAVE_BUILTIN(__builtin_bit_cast)
                            ,
                            int>::type = 0>
#if ABSL_HAVE_BUILTIN(__builtin_bit_cast)
inline constexpr Dest bit_cast(const Source& source) {
  return __builtin_bit_cast(Dest, source);
}
#else  // ABSL_HAVE_BUILTIN(__builtin_bit_cast)
inline Dest bit_cast(const Source& source) {
  Dest dest;
  memcpy(static_cast<void*>(std::addressof(dest)),
         static_cast<const void*>(std::addressof(source)), sizeof(dest));
  return dest;
}
#endif  // ABSL_HAVE_BUILTIN(__builtin_bit_cast)

#endif  // defined(__cpp_lib_bit_cast) && __cpp_lib_bit_cast >= 201806L

namespace base_internal {

[[noreturn]] ABSL_ATTRIBUTE_NOINLINE void BadDownCastCrash(
    const char* source_type, const char* target_type);

template <typename To, typename From>
inline void ValidateDownCast(From* f ABSL_ATTRIBUTE_UNUSED) {
  // Assert only if RTTI is enabled and in debug mode or hardened asserts are
  // enabled.
#ifdef ABSL_INTERNAL_HAS_RTTI
#if !defined(NDEBUG) || (ABSL_OPTION_HARDENED == 1)
  // Suppress erroneous nonnull comparison warning on older GCC.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnonnull-compare"
#endif
  if (ABSL_PREDICT_FALSE(f != nullptr && dynamic_cast<To>(f) == nullptr)) {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
    absl::base_internal::BadDownCastCrash(
        typeid(*f).name(), typeid(std::remove_pointer_t<To>).name());
  }
#endif
#endif
}

}  // namespace base_internal

// An "upcast", i.e. a conversion from a pointer to an object to a pointer to a
// base subobject, always succeeds if the base is unambiguous and accessible,
// and so it's fine to use implicit_cast.
//
// A "downcast", i.e. a conversion from a pointer to an object to a pointer
// to a more-derived object that may contain the original object as a base
// subobject, cannot safely be done using static_cast, because you do not
// generally know whether the source object is really the base subobject of
// a containing, more-derived object of the target type. Thus, when you
// downcast in a polymorphic type hierarchy, you should use the following
// function template.
//
// This function only returns null when the input is null. In debug mode, we
// use dynamic_cast to double-check whether the downcast is legal (we die if
// it's not). In normal mode, we do the efficient static_cast instead. Because
// the process will die in debug mode, it's important to test to make sure the
// cast is legal before calling this function!
//
// dynamic_cast should be avoided except as allowed by the style guide
// (https://google.github.io/styleguide/cppguide.html#Run-Time_Type_Information__RTTI_).

template <typename To, typename From>  // use like this: down_cast<T*>(foo);
[[nodiscard]]
inline To down_cast(From* f) {  // so we only accept pointers
  static_assert(std::is_pointer<To>::value, "target type not a pointer");
  // dynamic_cast allows casting to the same type or a more cv-qualified
  // version of the same type without them being polymorphic.
  if constexpr (!std::is_same<std::remove_cv_t<std::remove_pointer_t<To>>,
                              std::remove_cv_t<From>>::value) {
    static_assert(std::is_polymorphic<From>::value,
                  "source type must be polymorphic");
    static_assert(std::is_polymorphic<std::remove_pointer_t<To>>::value,
                  "target type must be polymorphic");
  }
  static_assert(
      std::is_convertible<std::remove_cv_t<std::remove_pointer_t<To>>*,
                          std::remove_cv_t<From>*>::value,
      "target type not derived from source type");

  absl::base_internal::ValidateDownCast<To>(f);

  return static_cast<To>(f);
}

// Overload of down_cast for references. Use like this:
// absl::down_cast<T&>(foo). The code is slightly convoluted because we're still
// using the pointer form of dynamic cast. (The reference form throws an
// exception if it fails.)
//
// There's no need for a special const overload either for the pointer
// or the reference form. If you call down_cast with a const T&, the
// compiler will just bind From to const T.
template <typename To, typename From>
[[nodiscard]]
inline To down_cast(From& f) {
  static_assert(std::is_lvalue_reference<To>::value,
                "target type not a reference");
  // dynamic_cast allows casting to the same type or a more cv-qualified
  // version of the same type without them being polymorphic.
  if constexpr (!std::is_same<std::remove_cv_t<std::remove_reference_t<To>>,
                              std::remove_cv_t<From>>::value) {
    static_assert(std::is_polymorphic<From>::value,
                  "source type must be polymorphic");
    static_assert(std::is_polymorphic<std::remove_reference_t<To>>::value,
                  "target type must be polymorphic");
  }
  static_assert(
      std::is_convertible<std::remove_cv_t<std::remove_reference_t<To>>*,
                          std::remove_cv_t<From>*>::value,
      "target type not derived from source type");

  absl::base_internal::ValidateDownCast<std::remove_reference_t<To>*>(
      std::addressof(f));

  return static_cast<To>(f);
}

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_BASE_CASTS_H_
