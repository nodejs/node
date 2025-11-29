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
// Historical note: Abseil once provided an implementation of `absl::variant`
// as a polyfill for `std::variant` prior to C++17. Now that C++17 is required,
// `absl::variant` is an alias for `std::variant`.

#ifndef ABSL_TYPES_VARIANT_H_
#define ABSL_TYPES_VARIANT_H_

#include <variant>

#include "absl/base/config.h"
#include "absl/utility/utility.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
using std::bad_variant_access;
using std::get;
using std::get_if;
using std::holds_alternative;
using std::monostate;
using std::variant;
using std::variant_alternative;
using std::variant_alternative_t;
using std::variant_npos;
using std::variant_size;
using std::variant_size_v;
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
// Helper functions to convert an `absl::variant` to a variant of another set of
// types, provided that the alternative type of the new variant type can be
// converted from any type in the source variant.
//
// Example:
//
//   absl::variant<name1, name2, float> InternalReq(const Req&);
//
//   // name1 and name2 are convertible to name
//   absl::variant<name, float> ExternalReq(const Req& req) {
//     return absl::ConvertVariantTo<absl::variant<name, float>>(
//              InternalReq(req));
//   }
template <typename To, typename Variant>
To ConvertVariantTo(Variant&& variant) {
  return absl::visit(variant_internal::ConversionVisitor<To>{},
                     std::forward<Variant>(variant));
}

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_TYPES_VARIANT_H_
