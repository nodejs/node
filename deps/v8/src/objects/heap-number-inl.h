// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_HEAP_NUMBER_INL_H_
#define V8_OBJECTS_HEAP_NUMBER_INL_H_

#include "src/objects/heap-number.h"

#include "src/objects/heap-object-inl.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(HeapNumberBase, HeapObject)
OBJECT_CONSTRUCTORS_IMPL(HeapNumber, HeapNumberBase)
OBJECT_CONSTRUCTORS_IMPL(MutableHeapNumber, HeapNumberBase)

CAST_ACCESSOR(HeapNumber)
CAST_ACCESSOR(MutableHeapNumber)

double HeapNumberBase::value() const { return ReadField<double>(kValueOffset); }

void HeapNumberBase::set_value(double value) {
  WriteField<double>(kValueOffset, value);
}

uint64_t HeapNumberBase::value_as_bits() const {
  // Bug(v8:8875): HeapNumber's double may be unaligned.
  return ReadUnalignedValue<uint64_t>(field_address(kValueOffset));
}

void HeapNumberBase::set_value_as_bits(uint64_t bits) {
  WriteUnalignedValue<uint64_t>(field_address(kValueOffset), bits);
}

int HeapNumberBase::get_exponent() {
  return ((ReadField<int>(kExponentOffset) & kExponentMask) >> kExponentShift) -
         kExponentBias;
}

int HeapNumberBase::get_sign() {
  return ReadField<int>(kExponentOffset) & kSignMask;
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_HEAP_NUMBER_INL_H_
