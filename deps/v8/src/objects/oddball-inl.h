// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ODDBALL_INL_H_
#define V8_OBJECTS_ODDBALL_INL_H_

#include "src/handles/handles.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/primitive-heap-object-inl.h"

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

ACCESSORS(Oddball, to_string, Tagged<String>, kToStringOffset)
ACCESSORS(Oddball, to_number, Tagged<Object>, kToNumberOffset)
ACCESSORS(Oddball, type_of, Tagged<String>, kTypeOfOffset)

uint8_t Oddball::kind() const {
  return Smi::ToInt(TaggedField<Smi>::load(*this, kKindOffset));
}

void Oddball::set_kind(uint8_t value) {
  WRITE_FIELD(*this, kKindOffset, Smi::FromInt(value));
}

// static
Handle<Object> Oddball::ToNumber(Isolate* isolate, Handle<Oddball> input) {
  return Handle<Object>(input->to_number(), isolate);
}

DEF_HEAP_OBJECT_PREDICATE(HeapObject, IsBoolean) {
  return IsOddball(obj, cage_base) &&
         ((Oddball::cast(obj)->kind() & Oddball::kNotBooleanMask) == 0);
}

TQ_CPP_OBJECT_DEFINITION_ASSERTS(Null, Oddball)
OBJECT_CONSTRUCTORS_IMPL(Null, Oddball)
CAST_ACCESSOR(Null)

TQ_CPP_OBJECT_DEFINITION_ASSERTS(Undefined, Oddball)
OBJECT_CONSTRUCTORS_IMPL(Undefined, Oddball)
CAST_ACCESSOR(Undefined)

TQ_CPP_OBJECT_DEFINITION_ASSERTS(Boolean, Oddball)
OBJECT_CONSTRUCTORS_IMPL(Boolean, Oddball)
CAST_ACCESSOR(Boolean)

bool Boolean::ToBool(Isolate* isolate) const {
  DCHECK(IsBoolean(*this, isolate));
  return IsTrue(*this, isolate);
}

TQ_CPP_OBJECT_DEFINITION_ASSERTS(True, Boolean)
OBJECT_CONSTRUCTORS_IMPL(True, Boolean)
CAST_ACCESSOR(True)

TQ_CPP_OBJECT_DEFINITION_ASSERTS(False, Boolean)
OBJECT_CONSTRUCTORS_IMPL(False, Boolean)
CAST_ACCESSOR(False)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ODDBALL_INL_H_
