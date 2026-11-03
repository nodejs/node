// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ODDBALL_INL_H_
#define V8_OBJECTS_ODDBALL_INL_H_

#include "src/objects/oddball.h"
// Include the non-inl header before the rest of the headers.

#include "src/handles/handles-inl.h"
#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/heap-number-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/heap-object-set-map-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball-predicates-inl.h"
#include "src/objects/tagged-field-inl.h"
#include "src/roots/roots-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

DEF_CAST_TRAITS(Oddball)
ODDBALL_LIST(DEF_CAST_TRAITS)

Oddball::Oddball(Tagged<ReadOnly<Map>> map, uint8_t kind)
    : PrimitiveHeapObject(map), kind_(Smi::FromInt(kind)) {}

void Oddball::FinishInitialization(Tagged<String> to_string,
                                   Tagged<Number> to_number,
                                   Tagged<String> type_of) {
  if (IsHeapNumber(to_number)) {
    set_to_number_raw_as_bits(Cast<HeapNumber>(to_number)->value_as_bits());
  } else {
    set_to_number_raw(Object::NumberValue(to_number));
  }
  set_to_number(to_number, SKIP_WRITE_BARRIER);
  set_to_string(to_string, SKIP_WRITE_BARRIER);
  set_type_of(type_of, SKIP_WRITE_BARRIER);
}

Null::Null(const AllocationWitness& witness)
    : Oddball(witness.roots().null_map(), Oddball::kNull) {}

Undefined::Undefined(const AllocationWitness& witness)
    : Oddball(witness.roots().undefined_map(), Oddball::kUndefined) {}

Boolean::Boolean(const AllocationWitness& witness, uint8_t kind)
    : Oddball(witness.roots().boolean_map(), kind) {}

True::True(const AllocationWitness& witness)
    : Boolean(witness, Oddball::kTrue) {}

False::False(const AllocationWitness& witness)
    : Boolean(witness, Oddball::kFalse) {}

double Oddball::to_number_raw() const { return to_number_raw_.value(); }
void Oddball::set_to_number_raw(double value) {
  to_number_raw_.set_value(value);
}

void Oddball::set_to_number_raw_as_bits(uint64_t bits) {
  // Bug(v8:8875): HeapNumber's double may be unaligned.
  to_number_raw_.set_value_as_bits(bits);
}

Tagged<String> Oddball::to_string() const { return to_string_.load(); }
void Oddball::set_to_string(Tagged<String> value, WriteBarrierMode mode) {
  to_string_.store(this, value, mode);
}

Tagged<Number> Oddball::to_number() const { return to_number_.load(); }
void Oddball::set_to_number(Tagged<Number> value, WriteBarrierMode mode) {
  to_number_.store(this, value, mode);
}

Tagged<String> Oddball::type_of() const { return type_of_.load(); }
void Oddball::set_type_of(Tagged<String> value, WriteBarrierMode mode) {
  type_of_.store(this, value, mode);
}

uint8_t Oddball::kind() const { return kind_.load().value(); }

// static
Handle<Number> Oddball::ToNumber(Isolate* isolate,
                                 DirectHandle<Oddball> input) {
  return handle(input->to_number(), isolate);
}

DEF_HEAP_OBJECT_PREDICATE(IsBoolean) {
  return IsOddball(obj) &&
         ((Cast<Oddball>(obj)->kind() & Oddball::kNotBooleanMask) == 0);
}

bool Boolean::ToBool(Isolate* isolate) const {
  DCHECK(IsBoolean(Tagged<HeapObject>(this)));
  return Is<True>(this);
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ODDBALL_INL_H_
