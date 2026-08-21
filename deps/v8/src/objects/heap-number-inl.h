// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HEAP_NUMBER_INL_H_
#define V8_OBJECTS_HEAP_NUMBER_INL_H_

#include "src/objects/heap-number.h"
// Include the non-inl header before the rest of the headers.

namespace v8 {
namespace internal {

double HeapNumber::value() const { return value_.value(); }
void HeapNumber::set_value(double value) {
  value_.set_value(value);
}

uint64_t HeapNumber::value_as_bits() const { return value_.value_as_bits(); }

void HeapNumber::set_value_as_bits(uint64_t bits) {
  value_.set_value_as_bits(bits);
}

bool HeapNumber::is_the_hole() const {
  return value_as_bits() == kHoleNanInt64;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_HEAP_NUMBER_INL_H_
