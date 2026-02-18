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

#ifndef ABSL_EXTEND_INTERNAL_AGGREGATE_H_
#define ABSL_EXTEND_INTERNAL_AGGREGATE_H_

#include <tuple>
#include <type_traits>
#include <utility>

#include "absl/base/config.h"
#include "absl/extend/internal/num_bases.h"
#include "absl/extend/internal/num_initializers.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace aggregate_internal {

// This collection of constants to serialize information about type qualifiers
// in C++. Together with `ExtractQualifiers` and `ApplyQualifiers` below, this
// allows us to apply metafunctions that change the type but preserve
// qualifiers. For example, a meta-function that converted `T` to
// `std::vector<T>` could use `CopyQualifiers` below to ensure that
// `std::string` became `std::vector<std::string>`, and that
// `const std::string&` became `const std::vector<std::string>&`.
//
// Note: We use a non-power-of-2 for kRef so that operator| matches reference
// collapsing semantics. That is, `(kRefRef | kRef) == kRef` in the same way
// that `T&& &` is `T&`.
inline constexpr int kRefRef = 1;
inline constexpr int kRef = 3;
inline constexpr int kConst = 4;
inline constexpr int kVolatile = 8;

// `ExtractQualifiers` returns an integer value representing the qualifiers that
// are applied to the template parameter `T`.
template <typename T>
constexpr int ExtractQualifiers() {
  int quals = 0;
  if constexpr (std::is_lvalue_reference_v<T>) {
    quals |= kRef;
  } else if constexpr (std::is_rvalue_reference_v<T>) {
    quals |= kRefRef;
  }

  using dereffed_type = std::remove_reference_t<T>;
  if constexpr (std::is_const_v<dereffed_type>) quals |= kConst;
  if constexpr (std::is_volatile_v<dereffed_type>) quals |= kVolatile;

  return quals;
}

// `ApplyQualifiers<T, kQuals>::type` evaluates to a type which has the same
// underlying type as `T` but with additional qualifiers from `kQuals` applied.
// Note that we do not remove the qualifiers from `T`. So, for example,
// `ApplyQualifiers<const int, kRef>::type` will be `const int&` rather than
// `int&`.
//
// Also note that, because `
template <typename T, int kQuals>
struct ApplyQualifiers {
 private:
  static_assert(!std::is_reference_v<T>,
                "Apply qualifiers cannot be called on a reference type.");
  using handle_volatile =
      std::conditional_t<((kQuals & kVolatile) == kVolatile), volatile T, T>;
  using handle_const =
      std::conditional_t<((kQuals & kConst) == kConst), const handle_volatile,
                         handle_volatile>;
  using handle_refref = std::conditional_t<((kQuals & kRefRef) == kRefRef),
                                           handle_const&&, handle_const>;
  using handle_ref = std::conditional_t<((kQuals & kRef) == kRef),
                                        handle_refref&, handle_refref>;

 public:
  using type = handle_ref;
};

// `CopyQualifiers` extracts qualifiers from `From` and overwrites the
// qualifiers on `To`.
template <typename From, typename To>
using CopyQualifiers =
    typename ApplyQualifiers<To, ExtractQualifiers<From>()>::type;

// `CorrectQualifiers` is specific to the implementation of `GetFields`. In
// particular, when constructing a tuple of references to fields of a struct, we
// need to know which fields should be referenced and which should be forwarded.
// `CorrectQualifiers` forwards reference-members and otherwise copies the
// qualifiers on the struct to its members being referenced in the tuple.
template <typename Aggregate, typename Field, typename T>
constexpr decltype(auto) CorrectQualifiers(T& val) {
  if constexpr (std::is_reference_v<Field>) {
    return static_cast<Field>(val);
  } else {
    using type = CopyQualifiers<Aggregate&&, Field>;
    return static_cast<type>(val);
  }
}

// `RemoveQualifiersAndReferencesFromTuple` removes volatile and const
// qualifiers from the underlying type of each reference type within a tuple.
template <typename T>
struct RemoveQualifiersAndReferencesFromTuple;

template <typename... U>
struct RemoveQualifiersAndReferencesFromTuple<std::tuple<U...>> {
  using type =
      std::tuple<typename std::remove_cv_t<std::remove_reference_t<U>>...>;
};

template <typename T, int kNumBases = -1>
constexpr int NumFields() {
  if constexpr (std::is_empty_v<T>) {
    return 0;
  } else if constexpr (std::is_aggregate_v<T>) {
    if constexpr (kNumBases == -1) {
      constexpr auto num_bases = aggregate_internal::NumBases<T>();
      static_assert(num_bases.has_value(),
                    "This library can only detect the number of bases when the "
                    "type is non-empty and has exactly 0 or 1 base classes. "
                    "However, the type has >= 2 bases.");
      return aggregate_internal::NumInitializers<T>() - num_bases.value();
    } else {
      return aggregate_internal::NumInitializers<T>() - kNumBases;
    }
  } else {
    return -1;
  }
}

// Structured binding declarations are weird in that they produce the same
// bindings whether the object is an rvalue or lvalue. Hence we add reference
// qualifiers ourselves.

struct FieldGetter {
  struct Error {};
  template <int kNumBases, int kFieldCount, typename T>
  constexpr static auto Unpack(T&& t) {
    static_assert(
        kNumBases == -1 || kNumBases == 0 || kNumBases == 1,
        "Unpack() only supports aggregate types that have either no "
        "base class or 1 empty base class (absl::Extend). You can "
        "also set the number of bases to -1 to let the library try "
        "to detect the number of bases. All other values are invalid.");
    if constexpr (kFieldCount == -1) {
      constexpr int kNumFields = NumFields<std::decay_t<T>, kNumBases>();
      if constexpr (kNumFields != -1) {
        return Unpack<kNumBases, kNumFields>(std::forward<T>(t));
      } else {
        return Error{};
      }
    } else if constexpr (kFieldCount == 0) {
      return std::make_tuple();
    } else if constexpr (kFieldCount == 1) {
      auto&& [f0] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0));
    } else if constexpr (kFieldCount == 2) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1));
    } else if constexpr (kFieldCount == 3) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2));
    } else if constexpr (kFieldCount == 4) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3));
    } else if constexpr (kFieldCount == 5) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4));
    } else if constexpr (kFieldCount == 6) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5));
    } else if constexpr (kFieldCount == 7) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6));
    } else if constexpr (kFieldCount == 8) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7));
    } else if constexpr (kFieldCount == 9) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8));
    } else if constexpr (kFieldCount == 10) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9));
    } else if constexpr (kFieldCount == 11) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10));
    } else if constexpr (kFieldCount == 12) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11));
    } else if constexpr (kFieldCount == 13) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12));
    } else if constexpr (kFieldCount == 14) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13));
    } else if constexpr (kFieldCount == 15) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14));
    } else if constexpr (kFieldCount == 16) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14,
              f15] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15));
    } else if constexpr (kFieldCount == 17) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16));
    } else if constexpr (kFieldCount == 18) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17));
    } else if constexpr (kFieldCount == 19) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18));
    } else if constexpr (kFieldCount == 20) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19));
    } else if constexpr (kFieldCount == 21) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20));
    } else if constexpr (kFieldCount == 22) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21));
    } else if constexpr (kFieldCount == 23) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22));
    } else if constexpr (kFieldCount == 24) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23));
    } else if constexpr (kFieldCount == 25) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24));
    } else if constexpr (kFieldCount == 26) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25));
    } else if constexpr (kFieldCount == 27) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26));
    } else if constexpr (kFieldCount == 28) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27));
    } else if constexpr (kFieldCount == 29) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28] =
          t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28));
    } else if constexpr (kFieldCount == 30) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29));
    } else if constexpr (kFieldCount == 31) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30));
    } else if constexpr (kFieldCount == 32) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31));
    } else if constexpr (kFieldCount == 33) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32));
    } else if constexpr (kFieldCount == 34) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33));
    } else if constexpr (kFieldCount == 35) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33, f34] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33),
                                   CorrectQualifiers<T, decltype(f34)>(f34));
    } else if constexpr (kFieldCount == 36) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33, f34, f35] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33),
                                   CorrectQualifiers<T, decltype(f34)>(f34),
                                   CorrectQualifiers<T, decltype(f35)>(f35));
    } else if constexpr (kFieldCount == 37) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33, f34, f35, f36] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33),
                                   CorrectQualifiers<T, decltype(f34)>(f34),
                                   CorrectQualifiers<T, decltype(f35)>(f35),
                                   CorrectQualifiers<T, decltype(f36)>(f36));
    } else if constexpr (kFieldCount == 38) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33, f34, f35, f36, f37] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33),
                                   CorrectQualifiers<T, decltype(f34)>(f34),
                                   CorrectQualifiers<T, decltype(f35)>(f35),
                                   CorrectQualifiers<T, decltype(f36)>(f36),
                                   CorrectQualifiers<T, decltype(f37)>(f37));
    } else if constexpr (kFieldCount == 39) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33, f34, f35, f36, f37, f38] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33),
                                   CorrectQualifiers<T, decltype(f34)>(f34),
                                   CorrectQualifiers<T, decltype(f35)>(f35),
                                   CorrectQualifiers<T, decltype(f36)>(f36),
                                   CorrectQualifiers<T, decltype(f37)>(f37),
                                   CorrectQualifiers<T, decltype(f38)>(f38));
    } else if constexpr (kFieldCount == 40) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33),
                                   CorrectQualifiers<T, decltype(f34)>(f34),
                                   CorrectQualifiers<T, decltype(f35)>(f35),
                                   CorrectQualifiers<T, decltype(f36)>(f36),
                                   CorrectQualifiers<T, decltype(f37)>(f37),
                                   CorrectQualifiers<T, decltype(f38)>(f38),
                                   CorrectQualifiers<T, decltype(f39)>(f39));
    } else if constexpr (kFieldCount == 41) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33),
                                   CorrectQualifiers<T, decltype(f34)>(f34),
                                   CorrectQualifiers<T, decltype(f35)>(f35),
                                   CorrectQualifiers<T, decltype(f36)>(f36),
                                   CorrectQualifiers<T, decltype(f37)>(f37),
                                   CorrectQualifiers<T, decltype(f38)>(f38),
                                   CorrectQualifiers<T, decltype(f39)>(f39),
                                   CorrectQualifiers<T, decltype(f40)>(f40));
    } else if constexpr (kFieldCount == 42) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41] =
          t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33),
                                   CorrectQualifiers<T, decltype(f34)>(f34),
                                   CorrectQualifiers<T, decltype(f35)>(f35),
                                   CorrectQualifiers<T, decltype(f36)>(f36),
                                   CorrectQualifiers<T, decltype(f37)>(f37),
                                   CorrectQualifiers<T, decltype(f38)>(f38),
                                   CorrectQualifiers<T, decltype(f39)>(f39),
                                   CorrectQualifiers<T, decltype(f40)>(f40),
                                   CorrectQualifiers<T, decltype(f41)>(f41));
    } else if constexpr (kFieldCount == 43) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
              f42] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33),
                                   CorrectQualifiers<T, decltype(f34)>(f34),
                                   CorrectQualifiers<T, decltype(f35)>(f35),
                                   CorrectQualifiers<T, decltype(f36)>(f36),
                                   CorrectQualifiers<T, decltype(f37)>(f37),
                                   CorrectQualifiers<T, decltype(f38)>(f38),
                                   CorrectQualifiers<T, decltype(f39)>(f39),
                                   CorrectQualifiers<T, decltype(f40)>(f40),
                                   CorrectQualifiers<T, decltype(f41)>(f41),
                                   CorrectQualifiers<T, decltype(f42)>(f42));
    } else if constexpr (kFieldCount == 44) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
              f42, f43] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33),
                                   CorrectQualifiers<T, decltype(f34)>(f34),
                                   CorrectQualifiers<T, decltype(f35)>(f35),
                                   CorrectQualifiers<T, decltype(f36)>(f36),
                                   CorrectQualifiers<T, decltype(f37)>(f37),
                                   CorrectQualifiers<T, decltype(f38)>(f38),
                                   CorrectQualifiers<T, decltype(f39)>(f39),
                                   CorrectQualifiers<T, decltype(f40)>(f40),
                                   CorrectQualifiers<T, decltype(f41)>(f41),
                                   CorrectQualifiers<T, decltype(f42)>(f42),
                                   CorrectQualifiers<T, decltype(f43)>(f43));
    } else if constexpr (kFieldCount == 45) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
              f42, f43, f44] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33),
                                   CorrectQualifiers<T, decltype(f34)>(f34),
                                   CorrectQualifiers<T, decltype(f35)>(f35),
                                   CorrectQualifiers<T, decltype(f36)>(f36),
                                   CorrectQualifiers<T, decltype(f37)>(f37),
                                   CorrectQualifiers<T, decltype(f38)>(f38),
                                   CorrectQualifiers<T, decltype(f39)>(f39),
                                   CorrectQualifiers<T, decltype(f40)>(f40),
                                   CorrectQualifiers<T, decltype(f41)>(f41),
                                   CorrectQualifiers<T, decltype(f42)>(f42),
                                   CorrectQualifiers<T, decltype(f43)>(f43),
                                   CorrectQualifiers<T, decltype(f44)>(f44));
    } else if constexpr (kFieldCount == 46) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
              f42, f43, f44, f45] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33),
                                   CorrectQualifiers<T, decltype(f34)>(f34),
                                   CorrectQualifiers<T, decltype(f35)>(f35),
                                   CorrectQualifiers<T, decltype(f36)>(f36),
                                   CorrectQualifiers<T, decltype(f37)>(f37),
                                   CorrectQualifiers<T, decltype(f38)>(f38),
                                   CorrectQualifiers<T, decltype(f39)>(f39),
                                   CorrectQualifiers<T, decltype(f40)>(f40),
                                   CorrectQualifiers<T, decltype(f41)>(f41),
                                   CorrectQualifiers<T, decltype(f42)>(f42),
                                   CorrectQualifiers<T, decltype(f43)>(f43),
                                   CorrectQualifiers<T, decltype(f44)>(f44),
                                   CorrectQualifiers<T, decltype(f45)>(f45));
    } else if constexpr (kFieldCount == 47) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
              f42, f43, f44, f45, f46] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33),
                                   CorrectQualifiers<T, decltype(f34)>(f34),
                                   CorrectQualifiers<T, decltype(f35)>(f35),
                                   CorrectQualifiers<T, decltype(f36)>(f36),
                                   CorrectQualifiers<T, decltype(f37)>(f37),
                                   CorrectQualifiers<T, decltype(f38)>(f38),
                                   CorrectQualifiers<T, decltype(f39)>(f39),
                                   CorrectQualifiers<T, decltype(f40)>(f40),
                                   CorrectQualifiers<T, decltype(f41)>(f41),
                                   CorrectQualifiers<T, decltype(f42)>(f42),
                                   CorrectQualifiers<T, decltype(f43)>(f43),
                                   CorrectQualifiers<T, decltype(f44)>(f44),
                                   CorrectQualifiers<T, decltype(f45)>(f45),
                                   CorrectQualifiers<T, decltype(f46)>(f46));
    } else if constexpr (kFieldCount == 48) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
              f42, f43, f44, f45, f46, f47] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33),
                                   CorrectQualifiers<T, decltype(f34)>(f34),
                                   CorrectQualifiers<T, decltype(f35)>(f35),
                                   CorrectQualifiers<T, decltype(f36)>(f36),
                                   CorrectQualifiers<T, decltype(f37)>(f37),
                                   CorrectQualifiers<T, decltype(f38)>(f38),
                                   CorrectQualifiers<T, decltype(f39)>(f39),
                                   CorrectQualifiers<T, decltype(f40)>(f40),
                                   CorrectQualifiers<T, decltype(f41)>(f41),
                                   CorrectQualifiers<T, decltype(f42)>(f42),
                                   CorrectQualifiers<T, decltype(f43)>(f43),
                                   CorrectQualifiers<T, decltype(f44)>(f44),
                                   CorrectQualifiers<T, decltype(f45)>(f45),
                                   CorrectQualifiers<T, decltype(f46)>(f46),
                                   CorrectQualifiers<T, decltype(f47)>(f47));
    } else if constexpr (kFieldCount == 49) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
              f42, f43, f44, f45, f46, f47, f48] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33),
                                   CorrectQualifiers<T, decltype(f34)>(f34),
                                   CorrectQualifiers<T, decltype(f35)>(f35),
                                   CorrectQualifiers<T, decltype(f36)>(f36),
                                   CorrectQualifiers<T, decltype(f37)>(f37),
                                   CorrectQualifiers<T, decltype(f38)>(f38),
                                   CorrectQualifiers<T, decltype(f39)>(f39),
                                   CorrectQualifiers<T, decltype(f40)>(f40),
                                   CorrectQualifiers<T, decltype(f41)>(f41),
                                   CorrectQualifiers<T, decltype(f42)>(f42),
                                   CorrectQualifiers<T, decltype(f43)>(f43),
                                   CorrectQualifiers<T, decltype(f44)>(f44),
                                   CorrectQualifiers<T, decltype(f45)>(f45),
                                   CorrectQualifiers<T, decltype(f46)>(f46),
                                   CorrectQualifiers<T, decltype(f47)>(f47),
                                   CorrectQualifiers<T, decltype(f48)>(f48));
    } else if constexpr (kFieldCount == 50) {
      auto&& [f0,  // Did you forget `friend absl::EnableExtensions`?
              f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13, f14, f15,
              f16, f17, f18, f19, f20, f21, f22, f23, f24, f25, f26, f27, f28,
              f29, f30, f31, f32, f33, f34, f35, f36, f37, f38, f39, f40, f41,
              f42, f43, f44, f45, f46, f47, f48, f49] = t;
      return std::forward_as_tuple(CorrectQualifiers<T, decltype(f0)>(f0),
                                   CorrectQualifiers<T, decltype(f1)>(f1),
                                   CorrectQualifiers<T, decltype(f2)>(f2),
                                   CorrectQualifiers<T, decltype(f3)>(f3),
                                   CorrectQualifiers<T, decltype(f4)>(f4),
                                   CorrectQualifiers<T, decltype(f5)>(f5),
                                   CorrectQualifiers<T, decltype(f6)>(f6),
                                   CorrectQualifiers<T, decltype(f7)>(f7),
                                   CorrectQualifiers<T, decltype(f8)>(f8),
                                   CorrectQualifiers<T, decltype(f9)>(f9),
                                   CorrectQualifiers<T, decltype(f10)>(f10),
                                   CorrectQualifiers<T, decltype(f11)>(f11),
                                   CorrectQualifiers<T, decltype(f12)>(f12),
                                   CorrectQualifiers<T, decltype(f13)>(f13),
                                   CorrectQualifiers<T, decltype(f14)>(f14),
                                   CorrectQualifiers<T, decltype(f15)>(f15),
                                   CorrectQualifiers<T, decltype(f16)>(f16),
                                   CorrectQualifiers<T, decltype(f17)>(f17),
                                   CorrectQualifiers<T, decltype(f18)>(f18),
                                   CorrectQualifiers<T, decltype(f19)>(f19),
                                   CorrectQualifiers<T, decltype(f20)>(f20),
                                   CorrectQualifiers<T, decltype(f21)>(f21),
                                   CorrectQualifiers<T, decltype(f22)>(f22),
                                   CorrectQualifiers<T, decltype(f23)>(f23),
                                   CorrectQualifiers<T, decltype(f24)>(f24),
                                   CorrectQualifiers<T, decltype(f25)>(f25),
                                   CorrectQualifiers<T, decltype(f26)>(f26),
                                   CorrectQualifiers<T, decltype(f27)>(f27),
                                   CorrectQualifiers<T, decltype(f28)>(f28),
                                   CorrectQualifiers<T, decltype(f29)>(f29),
                                   CorrectQualifiers<T, decltype(f30)>(f30),
                                   CorrectQualifiers<T, decltype(f31)>(f31),
                                   CorrectQualifiers<T, decltype(f32)>(f32),
                                   CorrectQualifiers<T, decltype(f33)>(f33),
                                   CorrectQualifiers<T, decltype(f34)>(f34),
                                   CorrectQualifiers<T, decltype(f35)>(f35),
                                   CorrectQualifiers<T, decltype(f36)>(f36),
                                   CorrectQualifiers<T, decltype(f37)>(f37),
                                   CorrectQualifiers<T, decltype(f38)>(f38),
                                   CorrectQualifiers<T, decltype(f39)>(f39),
                                   CorrectQualifiers<T, decltype(f40)>(f40),
                                   CorrectQualifiers<T, decltype(f41)>(f41),
                                   CorrectQualifiers<T, decltype(f42)>(f42),
                                   CorrectQualifiers<T, decltype(f43)>(f43),
                                   CorrectQualifiers<T, decltype(f44)>(f44),
                                   CorrectQualifiers<T, decltype(f45)>(f45),
                                   CorrectQualifiers<T, decltype(f46)>(f46),
                                   CorrectQualifiers<T, decltype(f47)>(f47),
                                   CorrectQualifiers<T, decltype(f48)>(f48),
                                   CorrectQualifiers<T, decltype(f49)>(f49));
    } else {
      static_assert(
          kFieldCount <= 50,
          "We are unlikely to extend support beyond 50 fields, both because of "
          "the compile-time cost associated with doing so, and because we "
          "believe structs this large are probably better off grouped into "
          "well-designed smaller structs that can be nested.");
    }
  }
};

template <typename T,
          typename = std::enable_if_t<!std::is_same_v<T, FieldGetter::Error>>>
T NotAnError(T);

// Given a reference to an aggregate `T`, constructs a tuple of references to
// the fields in the aggregate. This only works for types that have either no
// base class or 1 empty base class.
template <typename T>
auto Unpack(T&& t) -> decltype(NotAnError(
    FieldGetter::Unpack<
        0, absl::aggregate_internal::NumFields<std::decay_t<T>>(), T>(
        std::forward<T>(t)))) {
  return FieldGetter::Unpack<
      0, absl::aggregate_internal::NumFields<std::decay_t<T>>(), T>(
      std::forward<T>(t));
}

}  // namespace aggregate_internal
ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_EXTEND_INTERNAL_AGGREGATE_H_
