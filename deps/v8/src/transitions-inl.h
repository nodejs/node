// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_TRANSITIONS_INL_H_
#define V8_TRANSITIONS_INL_H_

#include "objects-inl.h"
#include "transitions.h"

namespace v8 {
namespace internal {


#define FIELD_ADDR(p, offset) \
  (reinterpret_cast<byte*>(p) + offset - kHeapObjectTag)

#define WRITE_FIELD(p, offset, value) \
  (*reinterpret_cast<Object**>(FIELD_ADDR(p, offset)) = value)

#define CONDITIONAL_WRITE_BARRIER(heap, object, offset, value, mode)    \
  if (mode == UPDATE_WRITE_BARRIER) {                                   \
    heap->incremental_marking()->RecordWrite(                           \
      object, HeapObject::RawField(object, offset), value);             \
    if (heap->InNewSpace(value)) {                                      \
      heap->RecordWrite(object->address(), offset);                     \
    }                                                                   \
  }


TransitionArray* TransitionArray::cast(Object* object) {
  ASSERT(object->IsTransitionArray());
  return reinterpret_cast<TransitionArray*>(object);
}


Map* TransitionArray::elements_transition() {
  Object* transition_map = get(kElementsTransitionIndex);
  return Map::cast(transition_map);
}


void TransitionArray::ClearElementsTransition() {
  WRITE_FIELD(this, kElementsTransitionOffset, Smi::FromInt(0));
}


bool TransitionArray::HasElementsTransition() {
  return IsFullTransitionArray() &&
      get(kElementsTransitionIndex) != Smi::FromInt(0);
}


void TransitionArray::set_elements_transition(Map* transition_map,
                                              WriteBarrierMode mode) {
  ASSERT(IsFullTransitionArray());
  Heap* heap = GetHeap();
  WRITE_FIELD(this, kElementsTransitionOffset, transition_map);
  CONDITIONAL_WRITE_BARRIER(
      heap, this, kElementsTransitionOffset, transition_map, mode);
}


Object* TransitionArray::back_pointer_storage() {
  return get(kBackPointerStorageIndex);
}


void TransitionArray::set_back_pointer_storage(Object* back_pointer,
                                               WriteBarrierMode mode) {
  Heap* heap = GetHeap();
  WRITE_FIELD(this, kBackPointerStorageOffset, back_pointer);
  CONDITIONAL_WRITE_BARRIER(
      heap, this, kBackPointerStorageOffset, back_pointer, mode);
}


bool TransitionArray::HasPrototypeTransitions() {
  return IsFullTransitionArray() &&
      get(kPrototypeTransitionsIndex) != Smi::FromInt(0);
}


FixedArray* TransitionArray::GetPrototypeTransitions() {
  ASSERT(IsFullTransitionArray());
  Object* prototype_transitions = get(kPrototypeTransitionsIndex);
  return FixedArray::cast(prototype_transitions);
}


HeapObject* TransitionArray::UncheckedPrototypeTransitions() {
  ASSERT(HasPrototypeTransitions());
  return reinterpret_cast<HeapObject*>(get(kPrototypeTransitionsIndex));
}


void TransitionArray::SetPrototypeTransitions(FixedArray* transitions,
                                              WriteBarrierMode mode) {
  ASSERT(IsFullTransitionArray());
  ASSERT(transitions->IsFixedArray());
  Heap* heap = GetHeap();
  WRITE_FIELD(this, kPrototypeTransitionsOffset, transitions);
  CONDITIONAL_WRITE_BARRIER(
      heap, this, kPrototypeTransitionsOffset, transitions, mode);
}


Object** TransitionArray::GetPrototypeTransitionsSlot() {
  return HeapObject::RawField(reinterpret_cast<HeapObject*>(this),
                              kPrototypeTransitionsOffset);
}


Object** TransitionArray::GetKeySlot(int transition_number) {
  ASSERT(!IsSimpleTransition());
  ASSERT(transition_number < number_of_transitions());
  return HeapObject::RawField(
      reinterpret_cast<HeapObject*>(this),
      OffsetOfElementAt(ToKeyIndex(transition_number)));
}


String* TransitionArray::GetKey(int transition_number) {
  if (IsSimpleTransition()) {
    Map* target = GetTarget(kSimpleTransitionIndex);
    int descriptor = target->LastAdded();
    String* key = target->instance_descriptors()->GetKey(descriptor);
    return key;
  }
  ASSERT(transition_number < number_of_transitions());
  return String::cast(get(ToKeyIndex(transition_number)));
}


void TransitionArray::SetKey(int transition_number, String* key) {
  ASSERT(!IsSimpleTransition());
  ASSERT(transition_number < number_of_transitions());
  set(ToKeyIndex(transition_number), key);
}


Map* TransitionArray::GetTarget(int transition_number) {
  if (IsSimpleTransition()) {
    ASSERT(transition_number == kSimpleTransitionIndex);
    return Map::cast(get(kSimpleTransitionTarget));
  }
  ASSERT(transition_number < number_of_transitions());
  return Map::cast(get(ToTargetIndex(transition_number)));
}


void TransitionArray::SetTarget(int transition_number, Map* value) {
  if (IsSimpleTransition()) {
    ASSERT(transition_number == kSimpleTransitionIndex);
    return set(kSimpleTransitionTarget, value);
  }
  ASSERT(transition_number < number_of_transitions());
  set(ToTargetIndex(transition_number), value);
}


PropertyDetails TransitionArray::GetTargetDetails(int transition_number) {
  Map* map = GetTarget(transition_number);
  DescriptorArray* descriptors = map->instance_descriptors();
  int descriptor = map->LastAdded();
  return descriptors->GetDetails(descriptor);
}


int TransitionArray::Search(String* name) {
  if (IsSimpleTransition()) {
    String* key = GetKey(kSimpleTransitionIndex);
    if (key->Equals(name)) return kSimpleTransitionIndex;
    return kNotFound;
  }
  return internal::Search<ALL_ENTRIES>(this, name);
}


void TransitionArray::NoIncrementalWriteBarrierSet(int transition_number,
                                                   String* key,
                                                   Map* target) {
  FixedArray::NoIncrementalWriteBarrierSet(
      this, ToKeyIndex(transition_number), key);
  FixedArray::NoIncrementalWriteBarrierSet(
      this, ToTargetIndex(transition_number), target);
}


#undef FIELD_ADDR
#undef WRITE_FIELD
#undef CONDITIONAL_WRITE_BARRIER


} }  // namespace v8::internal

#endif  // V8_TRANSITIONS_INL_H_
