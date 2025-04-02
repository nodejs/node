// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HEAP_NUMBER_INL_H_
#define V8_OBJECTS_HEAP_NUMBER_INL_H_

#include "src/base/memory.h"
#include "src/objects/heap-number.h"
#include "src/objects/primitive-heap-object-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

double HeapNumber::value() const { return value_.value(); }
void HeapNumber::set_value(double value) {
#ifdef V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE
  // TODO(nicohartmann): Turn into DCHECK.
  CHECK(!IsUndefinedNan(value));
#endif  // V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE
  value_.set_value(value);
}

uint64_t HeapNumber::value_as_bits() const { return value_.value_as_bits(); }

void HeapNumber::set_value_as_bits(uint64_t bits) {
#ifdef V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE
  // TODO(nicohartmann): Turn into DCHECK.
  CHECK_NE(bits, kUndefinedNanInt64);
#endif  // V8_ENABLE_EXPERIMENTAL_UNDEFINED_DOUBLE
  value_.set_value_as_bits(bits);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HEAP_NUMBER_INL_H_
