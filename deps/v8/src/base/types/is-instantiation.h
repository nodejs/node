// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Originally in Chromium as "base/types/is_instantiation.h"
// Slightly adapted for inclusion in V8.
// Copyright 2025 the V8 project authors. All rights reserved.

#ifndef V8_BASE_TYPES_IS_INSTANTIATION_H_
#define V8_BASE_TYPES_IS_INSTANTIATION_H_

namespace v8::base {
namespace internal {

// True if and only if `T` is `C<Types...>` for some set of types, i.e. `T` is
// an instantiation of the template `C`.
//
// This is false by default. We specialize it to true below for pairs of
// arguments that satisfy the condition.
template <typename T, template <typename...> class C>
inline constexpr bool is_instantiation_v = false;

template <template <typename...> class C, typename... Ts>
inline constexpr bool is_instantiation_v<C<Ts...>, C> = true;

}  // namespace internal

// True if and only if the type `T` is an instantiation of the template `C` with
// some set of type arguments.
//
// Note that there is no allowance for reference or const/volatile qualifiers;
// if these are a concern you probably want to feed through `std::decay_t<T>`.
template <typename T, template <typename...> class C>
concept is_instantiation = internal::is_instantiation_v<T, C>;

}  // namespace v8::base

#endif  // V8_BASE_TYPES_IS_INSTANTIATION_H_
