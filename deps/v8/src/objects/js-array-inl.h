// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ARRAY_INL_H_
#define V8_OBJECTS_JS_ARRAY_INL_H_

#include "src/objects/js-array.h"
// Include the non-inl header before the rest of the headers.

#include "src/objects/js-objects-inl.h"
#include "src/objects/tagged-field-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Tagged<Number> JSArray::length() const { return length_.load(); }

void JSArray::set_length(Tagged<Number> value, WriteBarrierMode mode) {
  length_.Relaxed_Store(this, value, mode);
}

Tagged<Number> JSArray::length(RelaxedLoadTag) const {
  return length_.Relaxed_Load();
}

void JSArray::set_length(Tagged<Smi> length) {
  set_length(Tagged<Number>(length), SKIP_WRITE_BARRIER);
}

bool JSArray::SetLengthWouldNormalize(Heap* heap, uint32_t new_length) {
  return new_length > kMaxFastArrayLength;
}

void JSArray::SetContent(Isolate* isolate, DirectHandle<JSArray> array,
                         DirectHandle<FixedArrayBase> storage) {
  const uint32_t storage_len = storage->ulength().value();
  JSObject::EnsureCanContainElements(isolate, Cast<JSObject>(array), storage,
                                     storage_len, ALLOW_COPIED_DOUBLE_ELEMENTS);
#ifdef DEBUG
  ReadOnlyRoots roots = GetReadOnlyRoots();
  Tagged<Map> map = storage->map();
  if (map == roots.fixed_double_array_map()) {
    DCHECK(IsDoubleElementsKind(array->GetElementsKind()));
  } else {
    DCHECK_NE(map, roots.fixed_double_array_map());
    if (IsSmiElementsKind(array->GetElementsKind())) {
      auto elems = Cast<FixedArray>(storage);
      const uint32_t elems_len = elems->ulength().value();
      Tagged<Object> the_hole = roots.the_hole_value();
      for (uint32_t i = 0; i < elems_len; i++) {
        Tagged<Object> candidate = elems->get(i);
        DCHECK(IsSmi(candidate) || candidate == the_hole);
      }
    } else {
      DCHECK(IsObjectElementsKind(array->GetElementsKind()));
    }
  }
#endif  // DEBUG
  array->set_elements(*storage);
  array->set_length(Smi::FromUInt(storage_len));
}

bool JSArray::HasArrayPrototype(Isolate* isolate) {
  return map()->prototype() == *isolate->initial_array_prototype();
}

Tagged<JSArray> TemplateLiteralObject::raw() const { return raw_.load(); }
void TemplateLiteralObject::set_raw(Tagged<JSArray> value,
                                    WriteBarrierMode mode) {
  raw_.store(this, value, mode);
}
int TemplateLiteralObject::function_literal_id() const {
  return function_literal_id_.load().value();
}
void TemplateLiteralObject::set_function_literal_id(int value) {
  function_literal_id_.store(this, Smi::FromInt(value));
}
int TemplateLiteralObject::slot_id() const { return slot_id_.load().value(); }
void TemplateLiteralObject::set_slot_id(int value) {
  slot_id_.store(this, Smi::FromInt(value));
}

Tagged<JSReceiver> JSArrayIterator::iterated_object() const {
  return iterated_object_.load();
}
void JSArrayIterator::set_iterated_object(Tagged<JSReceiver> value,
                                          WriteBarrierMode mode) {
  iterated_object_.store(this, value, mode);
}

Tagged<Number> JSArrayIterator::next_index() const {
  return next_index_.load();
}
void JSArrayIterator::set_next_index(Tagged<Number> value,
                                     WriteBarrierMode mode) {
  next_index_.store(this, value, mode);
}

int JSArrayIterator::raw_kind() const { return kind_.load().value(); }

void JSArrayIterator::set_raw_kind(int value) {
  kind_.store(this, Smi::FromInt(value));
}

IterationKind JSArrayIterator::kind() const {
  return static_cast<IterationKind>(raw_kind());
}

void JSArrayIterator::set_kind(IterationKind kind) {
  set_raw_kind(static_cast<int>(kind));
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ARRAY_INL_H_
