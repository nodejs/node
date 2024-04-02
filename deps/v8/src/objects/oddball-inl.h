// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ODDBALL_INL_H_
#define V8_OBJECTS_ODDBALL_INL_H_

#include "src/objects/oddball.h"

#include "src/handles/handles.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

TQ_CPP_OBJECT_DEFINITION_ASSERTS(Oddball, PrimitiveHeapObject)

OBJECT_CONSTRUCTORS_IMPL(Oddball, PrimitiveHeapObject)

CAST_ACCESSOR(Oddball)

DEF_PRIMITIVE_ACCESSORS(Oddball, to_number_raw, kToNumberRawOffset, double)

void Oddball::set_to_number_raw_as_bits(uint64_t bits) {
  // Bug(v8:8875): HeapNumber's double may be unaligned.
  base::WriteUnalignedValue<uint64_t>(field_address(kToNumberRawOffset), bits);
}

ACCESSORS(Oddball, to_string, String, kToStringOffset)
ACCESSORS(Oddball, to_number, Object, kToNumberOffset)
ACCESSORS(Oddball, type_of, String, kTypeOfOffset)

byte Oddball::kind() const {
  return Smi::ToInt(TaggedField<Smi>::load(*this, kKindOffset));
}

void Oddball::set_kind(byte value) {
  WRITE_FIELD(*this, kKindOffset, Smi::FromInt(value));
}

// static
Handle<Object> Oddball::ToNumber(Isolate* isolate, Handle<Oddball> input) {
  return Handle<Object>(input->to_number(), isolate);
}

DEF_GETTER(HeapObject, IsBoolean, bool) {
  return IsOddball(cage_base) &&
         ((Oddball::cast(*this).kind() & Oddball::kNotBooleanMask) == 0);
}

bool Oddball::ToBool(Isolate* isolate) const {
  DCHECK(IsBoolean(isolate));
  return IsTrue(isolate);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ODDBALL_INL_H_
