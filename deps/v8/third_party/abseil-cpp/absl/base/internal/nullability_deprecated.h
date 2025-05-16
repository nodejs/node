// Copyright 2023 The Abseil Authors.
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
#ifndef ABSL_BASE_INTERNAL_NULLABILITY_DEPRECATED_H_
#define ABSL_BASE_INTERNAL_NULLABILITY_DEPRECATED_H_

#include "absl/base/attributes.h"
#include "absl/base/config.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace nullability_internal {

template <typename T>
using NullableImpl
#if ABSL_HAVE_CPP_ATTRIBUTE(clang::annotate)
    [[clang::annotate("Nullable")]]
#endif
// Don't add the _Nullable attribute in Objective-C compiles. Many Objective-C
// projects enable the `-Wnullable-to-nonnull-conversion warning`, which is
// liable to produce false positives.
#if ABSL_HAVE_FEATURE(nullability_on_classes) && !defined(__OBJC__)
    = T _Nullable;
#else
    = T;
#endif

template <typename T>
using NonnullImpl
#if ABSL_HAVE_CPP_ATTRIBUTE(clang::annotate)
    [[clang::annotate("Nonnull")]]
#endif
#if ABSL_HAVE_FEATURE(nullability_on_classes) && !defined(__OBJC__)
    = T _Nonnull;
#else
    = T;
#endif

template <typename T>
using NullabilityUnknownImpl
#if ABSL_HAVE_CPP_ATTRIBUTE(clang::annotate)
    [[clang::annotate("Nullability_Unspecified")]]
#endif
#if ABSL_HAVE_FEATURE(nullability_on_classes) && !defined(__OBJC__)
    = T _Null_unspecified;
#else
    = T;
#endif

}  // namespace nullability_internal

// The following template aliases are deprecated forms of nullability
// annotations. They have some limitations, for example, an incompatibility with
// `auto*` pointers, as `auto` cannot be used in a template argument.
//
// It is important to note that these annotations are not distinct strong
// *types*. They are alias templates defined to be equal to the underlying
// pointer type. A pointer annotated `Nonnull<T*>`, for example, is simply a
// pointer of type `T*`.
//
// Prefer the macro style annotations in `absl/base/nullability.h` instead.

// absl::Nonnull, analogous to absl_nonnull
//
// Example:
// absl::Nonnull<int*> foo;
// Is equivalent to:
// int* absl_nonnull foo;
template <typename T>
using Nonnull =
    nullability_internal::NonnullImpl<T>;

// absl::Nullable, analogous to absl_nullable
//
// Example:
// absl::Nullable<int*> foo;
// Is equivalent to:
// int* absl_nullable foo;
template <typename T>
using Nullable =
    nullability_internal::NullableImpl<T>;

// absl::NullabilityUnknown, analogous to absl_nullability_unknown
//
// Example:
// absl::NullabilityUnknown<int*> foo;
// Is equivalent to:
// int* absl_nullability_unknown foo;
template <typename T>
using NullabilityUnknown =
    nullability_internal::NullabilityUnknownImpl<T>;

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_BASE_INTERNAL_NULLABILITY_DEPRECATED_H_
