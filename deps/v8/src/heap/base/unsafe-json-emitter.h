// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_BASE_UNSAFE_JSON_EMITTER_H_
#define V8_HEAP_BASE_UNSAFE_JSON_EMITTER_H_

#include <sstream>
#include <type_traits>

namespace heap::base {

// This class allows to build a JSON object as a C++ string. Use only on trusted
// input.
class UnsafeJsonEmitter final {
 public:
  UnsafeJsonEmitter() = default;

  UnsafeJsonEmitter& object_start();
  UnsafeJsonEmitter& object_end();

  template <typename T>
  UnsafeJsonEmitter& p(const char* name, T value) {
    if (!first_) out_ << ",";
    emit_property_name(name);
    emit_value(value);
    first_ = false;
    return *this;
  }

  std::string ToString();

 private:
  void emit_property_name(const char* name);

  void emit_value(bool b) { out_ << (b ? "true" : "false"); }
  void emit_value(const char* value) { out_ << "\"" << value << "\""; }

  template <typename T>
  void emit_value(T value)
    requires std::is_floating_point_v<T> || std::is_integral_v<T>
  {
    out_ << value;
  }

  std::stringstream out_;
  bool first_ = false;
};

}  // namespace heap::base

#endif  // V8_HEAP_BASE_UNSAFE_JSON_EMITTER_H_
