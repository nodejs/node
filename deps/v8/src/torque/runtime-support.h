// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_RUNTIME_SUPPORT_H_
#define V8_TORQUE_RUNTIME_SUPPORT_H_

#include <type_traits>

template <class T>
struct Identity {
  using type = T;
};

template <class T>
struct UnderlyingTypeHelper : Identity<typename std::underlying_type<T>::type> {
};

template <class T>
using UnderlyingTypeIfEnum =
    typename std::conditional_t<std::is_enum<T>::value, UnderlyingTypeHelper<T>,
                                Identity<T>>::type;

// Utility for extracting the underlying type of an enum, returns the type
// itself if not an enum.
template <class T>
UnderlyingTypeIfEnum<T> CastToUnderlyingTypeIfEnum(T x) {
  return static_cast<UnderlyingTypeIfEnum<T>>(x);
}

#endif  // V8_TORQUE_RUNTIME_SUPPORT_H_
