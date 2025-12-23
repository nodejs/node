// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Slightly adapted for inclusion in V8.
// Copyright 2025 the V8 project authors. All rights reserved.

#ifndef V8_BASE_NUMERICS_OSTREAM_OPERATORS_H_
#define V8_BASE_NUMERICS_OSTREAM_OPERATORS_H_

#include <ostream>
#include <type_traits>

namespace v8::base {
namespace internal {

template <typename T>
  requires std::is_arithmetic_v<T>
class ClampedNumeric;
template <typename T>
  requires std::is_arithmetic_v<T>
class StrictNumeric;

// Overload the ostream output operator to make logging work nicely.
template <typename T>
std::ostream& operator<<(std::ostream& os, const StrictNumeric<T>& value) {
  os << static_cast<T>(value);
  return os;
}

// Overload the ostream output operator to make logging work nicely.
template <typename T>
std::ostream& operator<<(std::ostream& os, const ClampedNumeric<T>& value) {
  os << static_cast<T>(value);
  return os;
}

}  // namespace internal
}  // namespace v8::base

#endif  // V8_BASE_NUMERICS_OSTREAM_OPERATORS_H_
