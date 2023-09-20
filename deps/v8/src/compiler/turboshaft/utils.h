// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_UTILS_H_
#define V8_COMPILER_TURBOSHAFT_UTILS_H_

#include <iostream>
#include <tuple>

#include "src/base/logging.h"

namespace v8::internal::compiler::turboshaft {

template <class... Ts>
struct any_of : std::tuple<const Ts&...> {
  explicit any_of(const Ts&... args) : std::tuple<const Ts&...>(args...) {}

  template <class T, size_t... indices>
  bool Contains(const T& value, std::index_sequence<indices...>) {
    return ((value == std::get<indices>(*this)) || ...);
  }

  template <size_t... indices>
  std::ostream& PrintTo(std::ostream& os, std::index_sequence<indices...>) {
    bool first = true;
    os << "any_of(";
    (((first ? (first = false, os) : os << ", "),
      os << base::PrintCheckOperand(std::get<indices>(*this))),
     ...);
    return os << ")";
  }
};
template <class... Args>
any_of(const Args&...) -> any_of<Args...>;

template <class T, class... Ts>
bool operator==(const T& value, any_of<Ts...> options) {
  return options.Contains(value, std::index_sequence_for<Ts...>{});
}

template <class... Ts>
std::ostream& operator<<(std::ostream& os, any_of<Ts...> any) {
  return any.PrintTo(os, std::index_sequence_for<Ts...>{});
}

template <class... Ts>
struct all_of : std::tuple<const Ts&...> {
  explicit all_of(const Ts&... args) : std::tuple<const Ts&...>(args...) {}

  template <class T, size_t... indices>
  bool AllEqualTo(const T& value, std::index_sequence<indices...>) {
    return ((value == std::get<indices>(*this)) && ...);
  }

  template <size_t... indices>
  std::ostream& PrintTo(std::ostream& os, std::index_sequence<indices...>) {
    bool first = true;
    os << "all_of(";
    (((first ? (first = false, os) : os << ", "),
      os << base::PrintCheckOperand(std::get<indices>(*this))),
     ...);
    return os << ")";
  }
};
template <class... Args>
all_of(const Args&...) -> all_of<Args...>;

template <class T, class... Ts>
bool operator==(all_of<Ts...> values, const T& target) {
  return values.AllEqualTo(target, std::index_sequence_for<Ts...>{});
}

template <class... Ts>
std::ostream& operator<<(std::ostream& os, all_of<Ts...> all) {
  return all.PrintTo(os, std::index_sequence_for<Ts...>{});
}

#ifdef DEBUG
bool ShouldSkipOptimizationStep();
#else
inline bool ShouldSkipOptimizationStep() { return false; }
#endif

// Set `*ptr` to `new_value` while the scope is active, reset to the previous
// value upon destruction.
template <class T>
class ScopedModification {
 public:
  ScopedModification(T* ptr, T new_value)
      : ptr_(ptr), old_value_(std::move(*ptr)) {
    *ptr = std::move(new_value);
  }

  ~ScopedModification() { *ptr_ = std::move(old_value_); }

  const T& old_value() const { return old_value_; }

 private:
  T* ptr_;
  T old_value_;
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_UTILS_H_
