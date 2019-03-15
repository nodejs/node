// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HEAP_NUMBER_INL_H_
#define V8_OBJECTS_HEAP_NUMBER_INL_H_

#include "src/objects/heap-number.h"

#include "src/objects-inl.h"
#include "src/objects/heap-object-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(HeapNumberBase, HeapObject)
OBJECT_CONSTRUCTORS_IMPL(HeapNumber, HeapNumberBase)
OBJECT_CONSTRUCTORS_IMPL(MutableHeapNumber, HeapNumberBase)

CAST_ACCESSOR(HeapNumber)
CAST_ACCESSOR(MutableHeapNumber)

double HeapNumberBase::value() const {
  return READ_DOUBLE_FIELD(*this, kValueOffset);
}

void HeapNumberBase::set_value(double value) {
  WRITE_DOUBLE_FIELD(*this, kValueOffset, value);
}

uint64_t HeapNumberBase::value_as_bits() const {
  return READ_UINT64_FIELD(*this, kValueOffset);
}

void HeapNumberBase::set_value_as_bits(uint64_t bits) {
  WRITE_UINT64_FIELD(*this, kValueOffset, bits);
}

int HeapNumberBase::get_exponent() {
  return ((READ_INT_FIELD(*this, kExponentOffset) & kExponentMask) >>
          kExponentShift) -
         kExponentBias;
}

int HeapNumberBase::get_sign() {
  return READ_INT_FIELD(*this, kExponentOffset) & kSignMask;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HEAP_NUMBER_INL_H_
