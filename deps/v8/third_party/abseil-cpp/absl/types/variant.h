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
// variant.h
// -----------------------------------------------------------------------------
//
// Historical note: Abseil once provided an implementation of `std::variant`
// as a polyfill for `std::variant` prior to C++17. Now that C++17 is required,
// `std::variant` is an alias for `std::variant`.

#ifndef ABSL_TYPES_VARIANT_H_
#define ABSL_TYPES_VARIANT_H_

#include <stddef.h>

#include <variant>

#include "absl/base/config.h"
#include "absl/base/macros.h"
#include "absl/utility/utility.h"

namespace absl {
ABSL_NAMESPACE_BEGIN

using bad_variant_access ABSL_REFACTOR_INLINE
    = std::bad_variant_access;

using std::get;
using std::get_if;
using std::holds_alternative;

using monostate ABSL_REFACTOR_INLINE
    = std::monostate;

template <typename... Types>
using variant ABSL_REFACTOR_INLINE
    = std::variant<Types...>;

template <size_t I, typename T>
using variant_alternative ABSL_REFACTOR_INLINE
    = std::variant_alternative<I, T>;

template <size_t I, typename T>
using variant_alternative_t ABSL_REFACTOR_INLINE
    = std::variant_alternative_t<I, T>;

inline constexpr size_t variant_npos ABSL_REFACTOR_INLINE
    = std::variant_npos;

template <typename T>
using variant_size ABSL_REFACTOR_INLINE
    = std::variant_size<T>;

template <typename T>
inline constexpr size_t variant_size_v ABSL_REFACTOR_INLINE
    = std::variant_size_v<T>;

using std::visit;

namespace variant_internal {
// Helper visitor for converting a variant<Ts...>` into another type (mostly
// variant) that can be constructed from any type.
template <typename To>
struct ConversionVisitor {
  template <typename T>
  To operator()(T&& v) const {
    return To(std::forward<T>(v));
  }
};
}  // namespace variant_internal

// ConvertVariantTo()
//
// Helper functions to convert an `std::variant` to a variant of another set of
// types, provided that the alternative type of the new variant type can be
// converted from any type in the source variant.
//
// Example:
//
//   std::variant<name1, name2, float> InternalReq(const Req&);
//
//   // name1 and name2 are convertible to name
//   std::variant<name, float> ExternalReq(const Req& req) {
//     return absl::ConvertVariantTo<std::variant<name, float>>(
//              InternalReq(req));
//   }
template <typename To, typename Variant>
To ConvertVariantTo(Variant&& variant) {
  return std::visit(variant_internal::ConversionVisitor<To>{},
                    std::forward<Variant>(variant));
}

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_TYPES_VARIANT_H_
