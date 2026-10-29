// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/base/unsafe-json-emitter.h"

namespace heap::base {

UnsafeJsonEmitter& UnsafeJsonEmitter::object_start() {
  out_ << "{";
  first_ = true;
  return *this;
}

UnsafeJsonEmitter& UnsafeJsonEmitter::object_end() {
  out_ << "}";
  return *this;
}

std::string UnsafeJsonEmitter::ToString() { return out_.str(); }

void UnsafeJsonEmitter::emit_property_name(const char* name) {
  out_ << "\"" << name << "\":";
}

}  // namespace heap::base
