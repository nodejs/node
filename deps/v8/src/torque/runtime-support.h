// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_RUNTIME_SUPPORT_H_
#define V8_TORQUE_RUNTIME_SUPPORT_H_

#include <type_traits>

// Utility for extracting the underlying type of an enum, returns the type
// itself if not an enum.
template <class T>
auto CastToUnderlyingTypeIfEnum(T x) {
  if constexpr (std::is_enum_v<T>) {
    return static_cast<std::underlying_type_t<T>>(x);
  } else {
    return x;
  }
}

template <class To, class From>
auto CastIfEnumClass(From x) {
  // Enum classes don't implicitly convert to their underlying type, so we
  // have to explicitly convert them.
  if constexpr (std::is_enum_v<From>) {
    // Nested if-else since `underlying_type_t` is not available for non-enum.
    if constexpr (!std::is_convertible_v<From, std::underlying_type_t<From>>) {
      return static_cast<To>(x);
    } else {
      return x;
    }
  } else {
    return x;
  }
}

#endif  // V8_TORQUE_RUNTIME_SUPPORT_H_
