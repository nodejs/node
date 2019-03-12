// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ODDBALL_INL_H_
#define V8_OBJECTS_ODDBALL_INL_H_

#include "src/objects/oddball.h"

#include "src/heap/heap-write-barrier-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(Oddball, HeapObject)

CAST_ACCESSOR(Oddball)

double Oddball::to_number_raw() const {
  return READ_DOUBLE_FIELD(this, kToNumberRawOffset);
}

void Oddball::set_to_number_raw(double value) {
  WRITE_DOUBLE_FIELD(this, kToNumberRawOffset, value);
}

void Oddball::set_to_number_raw_as_bits(uint64_t bits) {
  WRITE_UINT64_FIELD(this, kToNumberRawOffset, bits);
}

ACCESSORS(Oddball, to_string, String, kToStringOffset)
ACCESSORS(Oddball, to_number, Object, kToNumberOffset)
ACCESSORS(Oddball, type_of, String, kTypeOfOffset)

byte Oddball::kind() const { return Smi::ToInt(READ_FIELD(this, kKindOffset)); }

void Oddball::set_kind(byte value) {
  WRITE_FIELD(this, kKindOffset, Smi::FromInt(value));
}

// static
Handle<Object> Oddball::ToNumber(Isolate* isolate, Handle<Oddball> input) {
  return handle(input->to_number(), isolate);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ODDBALL_INL_H_
