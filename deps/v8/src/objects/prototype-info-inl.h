// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_PROTOTYPE_INFO_INL_H_
#define V8_OBJECTS_PROTOTYPE_INFO_INL_H_

#include "src/objects/prototype-info.h"

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/map-inl.h"
#include "src/objects/maybe-object.h"
#include "src/objects/objects-inl.h"
#include "src/objects/struct-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/prototype-info-tq-inl.inc"

TQ_OBJECT_CONSTRUCTORS_IMPL(PrototypeInfo)

Map PrototypeInfo::ObjectCreateMap() {
  return Map::cast(object_create_map()->GetHeapObjectAssumeWeak());
}

// static
void PrototypeInfo::SetObjectCreateMap(Handle<PrototypeInfo> info,
                                       Handle<Map> map) {
  info->set_object_create_map(HeapObjectReference::Weak(*map));
}

bool PrototypeInfo::HasObjectCreateMap() {
  MaybeObject cache = object_create_map();
  return cache->IsWeak();
}

BOOL_ACCESSORS(PrototypeInfo, bit_field, should_be_fast_map,
               ShouldBeFastBit::kShift)

void PrototypeUsers::MarkSlotEmpty(WeakArrayList array, int index) {
  DCHECK_GT(index, 0);
  DCHECK_LT(index, array.length());
  // Chain the empty slots into a linked list (each empty slot contains the
  // index of the next empty slot).
  array.Set(index, MaybeObject::FromObject(empty_slot_index(array)));
  set_empty_slot_index(array, index);
}

Smi PrototypeUsers::empty_slot_index(WeakArrayList array) {
  return array.Get(kEmptySlotIndex).ToSmi();
}

void PrototypeUsers::set_empty_slot_index(WeakArrayList array, int index) {
  array.Set(kEmptySlotIndex, MaybeObject::FromObject(Smi::FromInt(index)));
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_PROTOTYPE_INFO_INL_H_
