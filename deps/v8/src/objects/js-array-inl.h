// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_ARRAY_INL_H_
#define V8_OBJECTS_JS_ARRAY_INL_H_

#include "src/objects/js-array.h"

#include "src/objects-inl.h"  // Needed for write barriers

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(JSArray, JSObject)
OBJECT_CONSTRUCTORS_IMPL(JSArrayIterator, JSObject)

CAST_ACCESSOR(JSArray)
CAST_ACCESSOR(JSArrayIterator)

ACCESSORS(JSArray, length, Object, kLengthOffset)

void JSArray::set_length(Smi length) {
  // Don't need a write barrier for a Smi.
  set_length(Object(length.ptr()), SKIP_WRITE_BARRIER);
}

bool JSArray::SetLengthWouldNormalize(Heap* heap, uint32_t new_length) {
  return new_length > kMaxFastArrayLength;
}

bool JSArray::AllowsSetLength() {
  bool result = elements()->IsFixedArray() || elements()->IsFixedDoubleArray();
  DCHECK(result == !HasFixedTypedArrayElements());
  return result;
}

void JSArray::SetContent(Handle<JSArray> array,
                         Handle<FixedArrayBase> storage) {
  EnsureCanContainElements(array, storage, storage->length(),
                           ALLOW_COPIED_DOUBLE_ELEMENTS);

  DCHECK(
      (storage->map() == array->GetReadOnlyRoots().fixed_double_array_map() &&
       IsDoubleElementsKind(array->GetElementsKind())) ||
      ((storage->map() != array->GetReadOnlyRoots().fixed_double_array_map()) &&
       (IsObjectElementsKind(array->GetElementsKind()) ||
        (IsSmiElementsKind(array->GetElementsKind()) &&
         Handle<FixedArray>::cast(storage)->ContainsOnlySmisOrHoles()))));
  array->set_elements(*storage);
  array->set_length(Smi::FromInt(storage->length()));
}

bool JSArray::HasArrayPrototype(Isolate* isolate) {
  return map()->prototype() == *isolate->initial_array_prototype();
}

ACCESSORS(JSArrayIterator, iterated_object, Object, kIteratedObjectOffset)
ACCESSORS(JSArrayIterator, next_index, Object, kNextIndexOffset)

IterationKind JSArrayIterator::kind() const {
  return static_cast<IterationKind>(
      Smi::cast(READ_FIELD(*this, kKindOffset))->value());
}

void JSArrayIterator::set_kind(IterationKind kind) {
  WRITE_FIELD(*this, kKindOffset, Smi::FromInt(static_cast<int>(kind)));
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_ARRAY_INL_H_
